#include <nm_core.h>

#if defined (NM_WITH_OVF_SUPPORT)
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>

#include <limits.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libxml2 */
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define NM_TEST_FILE "/home/void/stuff/ova/test.ova"
#define NM_OVA_DIR_TEMPL "/tmp/ova_extract_XXXXXX"
#define NM_BLOCK_SIZE 10240

#define NM_XML_OVF_NS "ovf"
#define NM_XML_RASD_NS "rasd"
#define NM_XML_OVF_HREF "http://schemas.dmtf.org/ovf/envelope/1"
#define NM_XML_RASD_HREF "http://schemas.dmtf.org/wbem/wscim/1" \
    "/cim-schema/2/CIM_ResourceAllocationSettingData"
#define NM_XPATH_NAME "/ovf:Envelope/ovf:VirtualSystem/ovf:Name/text()"
#define NM_XPATH_MEM  "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/" \
    "ovf:Item[rasd:ResourceType/text()=4]/rasd:VirtualQuantity/text()"
#define NM_XPATH_NCPU "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/" \
    "ovf:Item[rasd:ResourceType/text()=3]/rasd:VirtualQuantity/text()"
#define NM_XPATH_DRIVE_ID "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/" \
    "ovf:Item[rasd:ResourceType/text()=17]/rasd:HostResource/text()"
#define NM_XPATH_DRIVE_REF "/ovf:Envelope/ovf:DiskSection/" \
    "ovf:Disk[@ovf:diskId=\"%s\"]/@ovf:fileRef" // vmdisk1
#define NM_XPATH_DRIVE_CAP "/ovf:Envelope/ovf:DiskSection/" \
    "ovf:Disk[@ovf:diskId=\"%s\"]/@ovf:capacity"
#define NM_XPATH_DRIVE_HREF "/ovf:Envelope/ovf:References/" \
    "ovf:File[@ovf:id=\"%s\"]/@ovf:href" // file1
#define NM_XPATH_NETH "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/" \
    "ovf:Item[rasd:ResourceType/text()=10]"

typedef struct archive nm_archive_t;
typedef struct archive_entry nm_archive_entry_t;

typedef xmlChar nm_xml_char_t;
typedef xmlDocPtr nm_xml_doc_pt;
typedef xmlNodePtr nm_xml_node_pt;
typedef xmlNodeSetPtr nm_xml_node_set_pt;
typedef xmlXPathObjectPtr nm_xml_xpath_obj_pt;
typedef xmlXPathContextPtr nm_xml_xpath_ctx_pt;

typedef struct {
    nm_str_t file_name;
    nm_str_t capacity;
} nm_drive_t;

#define NM_INIT_DRIVE { NM_INIT_STR, NM_INIT_STR }
#define nm_drive_file(p) ((nm_drive_t *) p)->file_name
#define nm_drive_size(p) ((nm_drive_t *) p)->capacity

void nm_drive_vect_ins_cb(const void *unit_p, const void *ctx);
void nm_drive_vect_free_cb(const void *unit_p);

static int nm_clean_temp_dir(const char *tmp_dir, const nm_vect_t *files);
static void nm_archive_copy_data(nm_archive_t *in, nm_archive_t *out);
static void nm_ovf_extract(const nm_str_t *ova_path, const char *tmp_dir,
                           nm_vect_t *files);
static const char *nm_find_ovf(const nm_vect_t *files);
static nm_xml_doc_pt nm_ovf_open(const char *tmp_dir, const char *ovf_file);
static int nm_register_xml_ns(nm_xml_xpath_ctx_pt ctx);
static int __nm_register_xml_ns(nm_xml_xpath_ctx_pt ctx, const char *ns,
                                const char *href);
static nm_xml_xpath_obj_pt nm_exec_xpath(const char *xpath,
                                         nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_name(nm_str_t *name, nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_ncpu(nm_str_t *ncpu, nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_mem(nm_str_t *mem, nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_drives(nm_vect_t *drives, nm_xml_xpath_ctx_pt ctx);
static uint32_t nm_ovf_get_neth(nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_text(nm_str_t *res, nm_xml_xpath_ctx_pt ctx,
                            const char *xpath, const char *param);
static inline void nm_drive_free(nm_drive_t *d);

void nm_ovf_import(void)
{
    char templ_path[] = NM_OVA_DIR_TEMPL;
    const char *ovf_file;
    nm_vect_t files = NM_INIT_VECT;
    nm_vect_t drives = NM_INIT_VECT;
    nm_str_t ova_path = NM_INIT_STR;
    nm_vm_t vm = NM_INIT_VM;
    nm_xml_doc_pt doc = NULL;
    nm_xml_xpath_ctx_pt xpath_ctx = NULL;

    nm_str_alloc_text(&ova_path, NM_TEST_FILE);

    if (mkdtemp(templ_path) == NULL)
        nm_bug("%s: mkdtemp error: %s", __func__, strerror(errno));

    nm_ovf_extract(&ova_path, templ_path, &files);

    if ((ovf_file = nm_find_ovf(&files)) == NULL)
    {
        nm_print_warn(3, 6, _("OVF file is not found!"));
        goto out;
    }

#if NM_DEBUG
    nm_debug("ova: ovf file found: %s\n", ovf_file);
#endif

    if ((doc = nm_ovf_open(templ_path, ovf_file)) == NULL)
    {
        nm_print_warn(3, 6, _("Cannot parse OVF file"));
        goto out;
    }

    if ((xpath_ctx = xmlXPathNewContext(doc)) == NULL)
    {
        nm_print_warn(3, 6, _("Cannot create new XPath context"));
        goto out;
    }

    if (nm_register_xml_ns(xpath_ctx) != NM_OK)
    {
        nm_print_warn(3, 6, _("Cannot register xml namespaces"));
        goto out;
    }

    nm_ovf_get_name(&vm.name, xpath_ctx);
    nm_ovf_get_ncpu(&vm.cpus, xpath_ctx);
    nm_ovf_get_mem(&vm.memo, xpath_ctx);
    nm_ovf_get_drives(&drives, xpath_ctx);
    nm_ovf_get_neth(xpath_ctx);

out:
    if (nm_clean_temp_dir(templ_path, &files) != NM_OK)
        nm_print_warn(3, 6, _("Some files was not deleted"));

    xmlXPathFreeContext(xpath_ctx);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    nm_str_free(&ova_path);
    nm_vect_free(&files, NULL);
    nm_vect_free(&drives, nm_drive_vect_free_cb);
    nm_vm_free(&vm);
}

static void nm_ovf_extract(const nm_str_t *ova_path, const char *tmp_dir,
                           nm_vect_t *files)
{
    nm_archive_t *in, *out;
    nm_archive_entry_t *ar_entry;
    char cwd[PATH_MAX] = {0};
    int rc;

    in = archive_read_new();
    out = archive_write_disk_new();

    archive_write_disk_set_options(out, 0);
    archive_read_support_format_tar(in);

    if (archive_read_open_filename(in, ova_path->data, NM_BLOCK_SIZE) != 0)
        nm_bug("%s: ovf read error: %s", __func__, archive_error_string(in));

    /* save current working directory */
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        nm_bug("%s: getcwd error: %s", __func__, strerror(errno));

    /* change current working directory to temporary */
    if (chdir(tmp_dir) == -1)
        nm_bug("%s: chdir error: %s", __func__, strerror(errno));

    for (;;)
    {
        const char *file;
        rc = archive_read_next_header(in, &ar_entry);

        if (rc == ARCHIVE_EOF)
            break;

        if (rc != ARCHIVE_OK)
            nm_bug("%s: bad archive: %s", __func__, archive_error_string(in));

        file = archive_entry_pathname(ar_entry);

        nm_vect_insert(files, file, strlen(file) + 1, NULL);
#ifdef NM_DEBUG
        nm_debug("ova: extract file: %s\n", file);
#endif
        rc = archive_write_header(out, ar_entry);
        if (rc != ARCHIVE_OK)
            nm_bug("%s: bad archive: %s", __func__, archive_error_string(in));
        else
        {
            nm_archive_copy_data(in, out);
            if (archive_write_finish_entry(out) != ARCHIVE_OK)
            {
                nm_bug("%s: archive_write_finish_entry: %s",
                       __func__, archive_error_string(out));
            }
        }
    }

    archive_read_free(in);
    archive_write_free(out);

    /* restore current working directory */
    if (chdir(cwd) == -1)
        nm_bug("%s: chdir error: %s", __func__, strerror(errno));
}

static void nm_archive_copy_data(nm_archive_t *in, nm_archive_t *out)
{
    int rc;
    const void *buf;
    size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
    int64_t offset;
#else
    off_t offset;
#endif

    for (;;)
    {
        rc = archive_read_data_block(in, &buf, &size, &offset);
        if (rc == ARCHIVE_EOF)
            break;
        if (rc != ARCHIVE_OK)
            nm_bug("%s: error read archive: %s", __func__, archive_error_string(in));

        if (archive_write_data_block(out, buf, size, offset) != ARCHIVE_OK)
            nm_bug("%s: error write archive: %s", __func__, archive_error_string(out));
    }
}

static int nm_clean_temp_dir(const char *tmp_dir, const nm_vect_t *files)
{
    nm_str_t path = NM_INIT_STR;
    int rc = NM_OK;

    for (size_t n = 0; n < files->n_memb; n++)
    {
        nm_str_format(&path, "%s/%s", tmp_dir, (char *) nm_vect_at(files, n));

        if (unlink(path.data) == -1)
            rc = NM_ERR;
#ifdef NM_DEBUG
        nm_debug("ova: clean file: %s\n", path.data);
#endif
        nm_str_trunc(&path, 0);
    }

    if (rc == NM_OK)
    {
        if (rmdir(tmp_dir) == -1)
            rc = NM_ERR;
    }

    nm_str_free(&path);

    return rc;
}

static const char *nm_find_ovf(const nm_vect_t *files)
{
    for (size_t n = 0; n < files->n_memb; n++)
    {
        const char *file = nm_vect_at(files, n);
        size_t len = strlen(file);

        if (len < 4)
            continue;

        if (nm_str_cmp_tt(file + (len - 4), ".ovf") == NM_OK)
            return file;
    }

    return NULL;
}

static nm_xml_doc_pt nm_ovf_open(const char *tmp_dir, const char *ovf_file)
{
    nm_xml_doc_pt doc = NULL;
    nm_str_t ovf_path = NM_INIT_STR;

    nm_str_format(&ovf_path, "%s/%s", tmp_dir, ovf_file);

    doc = xmlParseFile(ovf_path.data);

    nm_str_free(&ovf_path);

    return doc;
}

static int nm_register_xml_ns(nm_xml_xpath_ctx_pt ctx)
{
    if ((__nm_register_xml_ns(ctx, NM_XML_OVF_NS, NM_XML_OVF_HREF) == NM_ERR) ||
        (__nm_register_xml_ns(ctx, NM_XML_RASD_NS, NM_XML_RASD_HREF) == NM_ERR))
    {
        return NM_ERR;
    }

    return NM_OK;
}

static int __nm_register_xml_ns(nm_xml_xpath_ctx_pt ctx, const char *ns,
                                const char *href)
{
    int rc = NM_OK;

    nm_xml_char_t *xml_ns;
    nm_xml_char_t *xml_href;

    if ((xml_ns = xmlCharStrdup(ns)) == NULL)
        return NM_ERR;

    if ((xml_href = xmlCharStrdup(href)) == NULL)
    {
        xmlFree(xml_ns);
        return NM_ERR;
    }

    if (xmlXPathRegisterNs(ctx, xml_ns, xml_href) != 0)
        rc = NM_ERR;

    xmlFree(xml_ns);
    xmlFree(xml_href);

    return rc;
}

static nm_xml_xpath_obj_pt nm_exec_xpath(const char *xpath,
                                         nm_xml_xpath_ctx_pt ctx)
{
    nm_xml_char_t *xml_xpath;
    nm_xml_xpath_obj_pt obj = NULL;

    if ((xml_xpath = xmlCharStrdup(xpath)) == NULL)
        return NULL;

    obj = xmlXPathEvalExpression(xml_xpath, ctx);

    xmlFree(xml_xpath);

    return obj;
}

static void nm_ovf_get_mem(nm_str_t *mem, nm_xml_xpath_ctx_pt ctx)
{
    nm_ovf_get_text(mem, ctx, NM_XPATH_MEM, "memory");
}

static void nm_ovf_get_name(nm_str_t *name, nm_xml_xpath_ctx_pt ctx)
{
    nm_ovf_get_text(name, ctx, NM_XPATH_NAME, "name");
}

static void nm_ovf_get_ncpu(nm_str_t *ncpu, nm_xml_xpath_ctx_pt ctx)
{
    nm_ovf_get_text(ncpu, ctx, NM_XPATH_NCPU, "ncpu");
}

static void nm_ovf_get_drives(nm_vect_t *drives, nm_xml_xpath_ctx_pt ctx)
{
    nm_xml_xpath_obj_pt obj_id;
    size_t ndrives;

    if ((obj_id = nm_exec_xpath(NM_XPATH_DRIVE_ID, ctx)) == NULL)
        nm_bug("%s: cannot get drive_id from ovf file", __func__);

    if ((ndrives = obj_id->nodesetval->nodeNr) == 0)
        nm_bug("%s: no drives was found", __func__);

    for (size_t n = 0; n < ndrives; n++)
    {
        nm_xml_node_pt node_id = obj_id->nodesetval->nodeTab[n];

        if ((node_id->type == XML_TEXT_NODE) || (node_id->type == XML_ATTRIBUTE_NODE))
        {
            nm_xml_char_t *xml_id = xmlNodeGetContent(node_id);
            nm_str_t xpath = NM_INIT_STR;
            nm_str_t buf = NM_INIT_STR;
            nm_drive_t drive = NM_INIT_DRIVE;

            char *id = strrchr((char *) xml_id, '/');

            if (id == NULL)
                nm_bug("%s: NULL disk id", __func__);

            id++;
#ifdef NM_DEBUG
            nm_debug("ova: drive_id: %s\n", id);
#endif

            nm_str_format(&xpath, NM_XPATH_DRIVE_CAP, id);
            nm_ovf_get_text(&buf, ctx, xpath.data, "capacity");
            nm_str_copy(&drive.capacity, &buf);

            nm_str_trunc(&xpath, 0);
            nm_str_trunc(&buf, 0);

            nm_str_format(&xpath, NM_XPATH_DRIVE_REF, id);
            nm_ovf_get_text(&buf, ctx, xpath.data, "file ref");

            nm_str_trunc(&xpath, 0);

            nm_str_format(&xpath, NM_XPATH_DRIVE_HREF, buf.data);
            nm_str_trunc(&buf, 0);
            nm_ovf_get_text(&buf, ctx, xpath.data, "file href");
            nm_str_copy(&drive.file_name, &buf);

            nm_vect_insert(drives, &drive, sizeof(nm_drive_t),
                           nm_drive_vect_ins_cb);

            xmlFree(xml_id);
            nm_str_free(&xpath);
            nm_str_free(&buf);
            nm_drive_free(&drive);
        }
    }

    xmlXPathFreeObject(obj_id);
}

static uint32_t nm_ovf_get_neth(nm_xml_xpath_ctx_pt ctx)
{
    uint32_t neth = 0;
    nm_xml_xpath_obj_pt obj;

    if ((obj = nm_exec_xpath(NM_XPATH_NETH, ctx)) == NULL)
        nm_bug("%s: cannot get interfaces from ovf file", __func__);

    neth = obj->nodesetval->nodeNr;
#ifdef NM_DEBUG
    nm_debug("ova: eth num: %u\n", neth);
#endif

    xmlXPathFreeObject(obj);

    return neth;
}

static void nm_ovf_get_text(nm_str_t *res, nm_xml_xpath_ctx_pt ctx,
                            const char *xpath, const char *param)
{
    nm_xml_xpath_obj_pt obj;
    nm_xml_node_pt node;
    nm_xml_char_t *xml_text;

    if ((obj = nm_exec_xpath(xpath, ctx)) == NULL)
        nm_bug("%s: cannot get %s from ovf file", __func__, param);

    if (obj->nodesetval->nodeNr == 0)
        nm_bug("%s: xpath return zero result", __func__);

    node = obj->nodesetval->nodeTab[0];

    if ((node->type == XML_TEXT_NODE) || (node->type == XML_ATTRIBUTE_NODE))
    {
        xml_text = xmlNodeGetContent(node);
        nm_str_alloc_text(res, (char *) xml_text);
        xmlFree(xml_text);
    }

#ifdef NM_DEBUG
    nm_debug("ova: %s: %s\n", param, res->data);
#endif

    xmlXPathFreeObject(obj);
}

void nm_drive_vect_ins_cb(const void *unit_p, const void *ctx)
{
    nm_str_copy(&nm_drive_file(unit_p), &nm_drive_file(ctx));
    nm_str_copy(&nm_drive_size(unit_p), &nm_drive_size(ctx));
}

void nm_drive_vect_free_cb(const void *unit_p)
{
    nm_str_free(&nm_drive_file(unit_p));
    nm_str_free(&nm_drive_size(unit_p));
}

static inline void nm_drive_free(nm_drive_t *d)
{
    nm_str_free(&d->file_name);
    nm_str_free(&d->capacity);
}

#endif /* NM_WITH_OVF_SUPPORT */
/* vim:set ts=4 sw=4 fdm=marker: */
