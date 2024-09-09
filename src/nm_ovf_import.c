#include <nm_core.h>

#if defined(NM_WITH_OVF_SUPPORT)
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_menu.h>
#include <nm_add_vm.h>
#include <nm_cfg_file.h>
#include <nm_ovf_import.h>

#include <limits.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libxml2 */
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

const char *nm_ovf_version[] = {
    "1.0",
    "2.0",
    NULL
};

enum {NM_BLOCK_SIZE = 10240};
static const char NM_OVA_TMP_DIR[] = ".tmp_ova";
static const char NM_OVA_DIR_TEMPL[] = "extract_XXXXXX";

static const char NM_LC_OVF_FORM_PATH[]       = "Path to OVA";
static const char NM_LC_OVF_FORM_ARCH[]       = "Architecture";
static const char NM_LC_OVF_FORM_NAME[]       = "Name (optional)";
static const char NM_LC_OVF_FORM_VER[]        = "OVF version";
static const char NM_LC_OVF_FORM_DRV_FORMAT[] = "Disk image format";
static const char NM_XML_OVF_NS[]    = "ovf";
static const char NM_XML_RASD_NS[]   = "rasd";
static const char NM_XML_SASD_NS[]   = "sasd";
static const char NM_XML_OVF1_HREF[] = "http://schemas.dmtf.org/ovf/envelope/1";
static const char NM_XML_OVF2_HREF[] = "http://schemas.dmtf.org/ovf/envelope/2";

static const char NM_XML_RASD_HREF[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_ResourceAllocationSettingData";

static const char NM_XML_SASD_HREF[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_StorageAllocationSettingData.xsd";

static const char NM_XPATH_NAME[] =
    "/ovf:Envelope/ovf:VirtualSystem/ovf:Name/text()";

static const char NM_XPATH_MEM[] =
    "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/"
    "ovf:Item[rasd:ResourceType/text()=4]/rasd:VirtualQuantity/text()";

static const char NM_XPATH_NCPU[] =
    "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/"
    "ovf:Item[rasd:ResourceType/text()=3]/rasd:VirtualQuantity/text()";

static const char NM_XPATH_DRIVE_ID1[] =
    "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/"
    "ovf:Item[rasd:ResourceType/text()=17]/rasd:HostResource/text()";

static const char NM_XPATH_DRIVE_ID2[] =
    "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/"
    "ovf:StorageItem[sasd:ResourceType/text()=17]/sasd:HostResource/text()";

static const char NM_XPATH_DRIVE_REF[] =
    "/ovf:Envelope/ovf:DiskSection/"
    "ovf:Disk[@ovf:diskId=\"%s\"]/@ovf:fileRef";

static const char NM_XPATH_DRIVE_CAP[] =
    "/ovf:Envelope/ovf:DiskSection/"
    "ovf:Disk[@ovf:diskId=\"%s\"]/@ovf:capacity";

static const char NM_XPATH_DRIVE_HREF[] =
    "/ovf:Envelope/ovf:References/"
    "ovf:File[@ovf:id=\"%s\"]/@ovf:href";

static const char NM_XPATH_NETH[] =
    "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/"
    "ovf:Item[rasd:ResourceType/text()=10]";

static const char NM_XPATH_USB_EHCI[] =
    "/ovf:Envelope/ovf:VirtualSystem/ovf:VirtualHardwareSection/"
    "ovf:Item[rasd:ResourceType/text()=23]";

typedef struct archive nm_archive_t;
typedef struct archive_entry nm_archive_entry_t;

typedef xmlChar nm_xml_char_t;
typedef xmlDocPtr nm_xml_doc_pt;
typedef xmlNodePtr nm_xml_node_pt;
typedef xmlNodeSetPtr nm_xml_node_set_pt;
typedef xmlXPathObjectPtr nm_xml_xpath_obj_pt;
typedef xmlXPathContextPtr nm_xml_xpath_ctx_pt;

void nm_drive_vect_ins_cb(void *unit_p, const void *ctx);
void nm_drive_vect_free_cb(void *unit_p);

static void nm_ovf_init_windows(nm_form_t *form);
static void nm_ovf_fields_setup(void);
static size_t nm_ovf_labels_setup(void);
static int nm_clean_temp_dir(const char *tmp_dir, const nm_vect_t *files);
static void nm_archive_copy_data(nm_archive_t *in, nm_archive_t *out);
static void nm_ovf_extract(const nm_str_t *ova_path, const char *tmp_dir,
                           nm_vect_t *files);
static const char *nm_find_ovf(const nm_vect_t *files);
static nm_xml_doc_pt nm_ovf_open(const char *tmp_dir, const char *ovf_file);
static int nm_register_xml_ns(nm_xml_xpath_ctx_pt ctx, int version);
static int __nm_register_xml_ns(nm_xml_xpath_ctx_pt ctx, const char *ns,
                                const char *href);
static nm_xml_xpath_obj_pt nm_exec_xpath(const char *xpath,
                                         nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_name(nm_str_t *name, nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_ncpu(nm_str_t *ncpu, nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_mem(nm_str_t *mem, nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_drives(nm_vect_t *drives, nm_xml_xpath_ctx_pt ctx,
        int version);
static uint32_t nm_ovf_get_neth(nm_xml_xpath_ctx_pt ctx);
static uint32_t nm_ovf_get_usb(nm_xml_xpath_ctx_pt ctx);
static void nm_ovf_get_text(nm_str_t *res, nm_xml_xpath_ctx_pt ctx,
                            const char *xpath, const char *param);
static inline void nm_drive_free(nm_drive_t *d);
static void nm_ovf_convert_drives(const nm_vect_t *drives, const nm_str_t *name,
        const char *templ_path, const nm_str_t *format);
static void nm_ovf_to_db(nm_vm_t *vm, const nm_vect_t *drives);
static int nm_ova_get_data(nm_vm_t *vm, int *version);

enum {
    NM_OVA_LBL_SRC = 0, NM_OVA_FLD_SRC,
    NM_OVA_LBL_ARCH, NM_OVA_FLD_ARCH,
    NM_OVA_LBL_NAME, NM_OVA_FLD_NAME,
    NM_OVA_LBL_VER, NM_OVA_FLD_VER,
    NM_OVA_LBL_FMT, NM_OVA_FLD_FMT,
    NM_FLD_COUNT
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static void nm_ovf_init_windows(nm_form_t *form)
{
    if (form) {
        nm_form_window_init();
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);

        if (form_data) {
            form_data->parent_window = action_window;
        }
    } else {
        werase(action_window);
        werase(help_window);
    }

    nm_init_side();
    nm_init_action(_(NM_MSG_OVA_HEADER));
    nm_init_help_import();

    nm_print_vm_menu(NULL);
}

/*
 * Create a temporary directory for OVA unpacking. During OVA processing,
 * nEMU may crash and leave garbage. Therefore, at startup, nEMU will
 * clear the contents of this directory.
 */
void nm_ovf_init(void)
{
    nm_str_t ova_tmpdir = NM_INIT_STR;
    struct stat info;

    nm_str_format(&ova_tmpdir, "%s/%s",
            nm_cfg_get()->vm_dir.data, NM_OVA_TMP_DIR);

    if (stat(ova_tmpdir.data, &info) != 0) {
        if (nm_mkdir_parent(&ova_tmpdir,
                    S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != NM_OK) {
            nm_bug(_("%s: error"), __func__);
        }
    } else { /* cleanup content */
        if (nm_cleanup_dir(&ova_tmpdir) != NM_OK) {
            nm_bug(_("%s: %s"), __func__, strerror(errno));
        }
    }

    nm_str_free(&ova_tmpdir);
}

void nm_ovf_import(void)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_str_t templ_path = NM_INIT_STR;
    const char *ovf_file;
    nm_vect_t files = NM_INIT_VECT;
    nm_vect_t drives = NM_INIT_VECT;
    nm_vm_t vm = NM_INIT_VM;
    nm_xml_doc_pt doc = NULL;
    nm_xml_xpath_ctx_pt xpath_ctx = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    size_t msg_len;
    pthread_t spin_th = 0;
    int done = 0, version = 0;

    nm_str_format(&templ_path, "%s/%s/%s",
            nm_cfg_get()->vm_dir.data, NM_OVA_TMP_DIR, NM_OVA_DIR_TEMPL);

    nm_ovf_init_windows(NULL);

    msg_len = nm_ovf_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_ovf_init_windows, msg_len, NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        goto out;
    }

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_OVA_FLD_SRC:
            fields[n] = nm_field_regexp_new(n / 2, form_data, "^/.*");
            break;
        case NM_OVA_FLD_ARCH:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_cfg_get_arch(), false, false);
            break;
        case NM_OVA_FLD_NAME:
            fields[n] = nm_field_regexp_new(
                n / 2, form_data, "^[a-zA-Z0-9_-]{1,30} *$");
            break;
        case NM_OVA_FLD_VER:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_ovf_version, false, false);
            break;
        case NM_OVA_FLD_FMT:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_drive_fmt, false, false);
            break;
        default:
            fields[n] = nm_field_label_new(n / 2, form_data);
            break;
        }
    }
    fields[NM_FLD_COUNT] = NULL;

    nm_ovf_labels_setup();
    nm_ovf_fields_setup();
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        goto cancel;
    }

    if (nm_ova_get_data(&vm, &version) != NM_OK) {
        goto cancel;
    }

    sp_data.stop = &done;

    if (pthread_create(&spin_th, NULL, nm_progress_bar,
                (void *) &sp_data) != 0) {
        nm_bug(_("%s: cannot create thread"), __func__);
    }

    if (mkdtemp(templ_path.data) == NULL) {
        nm_bug("%s: mkdtemp error: %s", __func__, strerror(errno));
    }

    nm_ovf_extract(&vm.srcp, templ_path.data, &files);

    if ((ovf_file = nm_find_ovf(&files)) == NULL) {
        nm_warn(_(NM_MSG_OVF_MISS));
        goto out;
    }

    nm_debug("ova: ovf file found: %s\n", ovf_file);

    if ((doc = nm_ovf_open(templ_path.data, ovf_file)) == NULL) {
        nm_warn(_(NM_MSG_OVF_EPAR));
        goto out;
    }

    if ((xpath_ctx = xmlXPathNewContext(doc)) == NULL) {
        nm_warn(_(NM_MSG_XPATH_ERR));
        goto out;
    }

    if (nm_register_xml_ns(xpath_ctx, version) != NM_OK) {
        nm_warn(_(NM_MSG_ANY_KEY));
        goto out;
    }

    if (vm.name.len == 0) {
        nm_ovf_get_name(&vm.name, xpath_ctx);
    }
    nm_ovf_get_ncpu(&vm.cpus, xpath_ctx);
    nm_ovf_get_mem(&vm.memo, xpath_ctx);
    nm_ovf_get_drives(&drives, xpath_ctx, version);
    vm.ifs.count = nm_ovf_get_neth(xpath_ctx);
    if (nm_ovf_get_usb(xpath_ctx)) {
        vm.usb_enable = 1;
    }

    if (nm_form_name_used(&vm.name) != NM_OK) {
        goto out;
    }

    nm_ovf_convert_drives(&drives, &vm.name, templ_path.data, &vm.drive.format);
    nm_ovf_to_db(&vm, &drives);

out:
    done = 1;
    if (spin_th && (pthread_join(spin_th, NULL) != 0)) {
        nm_bug(_("%s: cannot join thread"), __func__);
    }

    if (nm_clean_temp_dir(templ_path.data, &files) != NM_OK) {
        nm_warn(_(NM_MSG_INC_DEL));
    }

    xmlXPathFreeContext(xpath_ctx);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    nm_str_free(&templ_path);
    nm_vect_free(&files, NULL);
    nm_vect_free(&drives, nm_drive_vect_free_cb);

cancel:
    NM_FORM_EXIT();
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_vm_free(&vm);
}

static void nm_ovf_fields_setup(void)
{
    set_field_buffer(fields[NM_OVA_FLD_ARCH], 0,
            *nm_cfg_get()->qemu_targets.data);
    set_field_buffer(fields[NM_OVA_FLD_VER], 0, *nm_ovf_version);
    set_field_buffer(fields[NM_OVA_FLD_FMT], 0, NM_DEFAULT_DRVFMT);
    field_opts_off(fields[NM_OVA_FLD_SRC], O_STATIC);
    field_opts_off(fields[NM_OVA_FLD_NAME], O_STATIC);
}

static size_t nm_ovf_labels_setup(void)
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_OVA_LBL_SRC:
            nm_str_format(&buf, "%s", _(NM_LC_OVF_FORM_PATH));
            break;
        case NM_OVA_LBL_ARCH:
            nm_str_format(&buf, "%s", _(NM_LC_OVF_FORM_ARCH));
            break;
        case NM_OVA_LBL_NAME:
            nm_str_format(&buf, "%s", _(NM_LC_OVF_FORM_NAME));
            break;
        case NM_OVA_LBL_VER:
            nm_str_format(&buf, "%s", _(NM_LC_OVF_FORM_VER));
            break;
        case NM_OVA_LBL_FMT:
            nm_str_format(&buf, "%s", _(NM_LC_OVF_FORM_DRV_FORMAT));
            break;
        default:
            continue;
        }

        msg_len = mbstowcs(NULL, buf.data, buf.len);
        if (msg_len > max_label_len) {
            max_label_len = msg_len;
        }

        if (fields[n]) {
            set_field_buffer(fields[n], 0, buf.data);
        }
    }

    nm_str_free(&buf);
    return max_label_len;
}

static void nm_ovf_extract(const nm_str_t *ova_path, const char *tmp_dir,
                           nm_vect_t *files)
{
    nm_archive_t *in, *out;
    nm_archive_entry_t *ar_entry;
    char cwd[PATH_MAX] = {0};

    in = archive_read_new();
    out = archive_write_disk_new();

    archive_write_disk_set_options(out, 0);
    archive_read_support_format_tar(in);

    if (archive_read_open_filename(in, ova_path->data, NM_BLOCK_SIZE) != 0) {
        nm_bug("%s: ovf read error: %s", __func__, archive_error_string(in));
    }

    /* save current working directory */
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        nm_bug("%s: getcwd error: %s", __func__, strerror(errno));
    }

    /* change current working directory to temporary */
    if (chdir(tmp_dir) == -1) {
        nm_bug("%s: chdir error: %s", __func__, strerror(errno));
    }

    for (;;) {
        const char *file;
        int rc = archive_read_next_header(in, &ar_entry);

        if (rc == ARCHIVE_EOF) {
            break;
        }

        if (rc != ARCHIVE_OK) {
            nm_bug("%s: bad archive: %s", __func__, archive_error_string(in));
        }

        file = archive_entry_pathname(ar_entry);

        nm_vect_insert(files, file, strlen(file) + 1, NULL);
        nm_debug("ova: extract file: %s\n", file);

        rc = archive_write_header(out, ar_entry);
        if (rc != ARCHIVE_OK) {
            nm_bug("%s: bad archive: %s", __func__, archive_error_string(in));
        } else {
            nm_archive_copy_data(in, out);
            if (archive_write_finish_entry(out) != ARCHIVE_OK) {
                nm_bug("%s: archive_write_finish_entry: %s",
                       __func__, archive_error_string(out));
            }
        }
    }

    archive_read_free(in);
    archive_write_free(out);

    /* restore current working directory */
    if (chdir(cwd) == -1) {
        nm_bug("%s: chdir error: %s", __func__, strerror(errno));
    }
}

static void nm_archive_copy_data(nm_archive_t *in, nm_archive_t *out)
{
    const void *buf;
    size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
    int64_t offset;
#else
    off_t offset;
#endif

    for (;;) {
        int rc = archive_read_data_block(in, &buf, &size, &offset);

        if (rc == ARCHIVE_EOF) {
            break;
        }
        if (rc != ARCHIVE_OK) {
            nm_bug("%s: error read archive: %s",
                    __func__, archive_error_string(in));
        }

        if (archive_write_data_block(out, buf, size, offset) != ARCHIVE_OK) {
            nm_bug("%s: error write archive: %s",
                    __func__, archive_error_string(out));
        }
    }
}

static int nm_clean_temp_dir(const char *tmp_dir, const nm_vect_t *files)
{
    nm_str_t path = NM_INIT_STR;
    int rc = NM_OK;

    for (size_t n = 0; n < files->n_memb; n++) {
        nm_str_format(&path, "%s/%s", tmp_dir, (char *) nm_vect_at(files, n));

        if (unlink(path.data) == -1) {
            rc = NM_ERR;
        }

        nm_debug("ova: clean file: %s\n", path.data);
    }

    if (rc == NM_OK) {
        if (rmdir(tmp_dir) == -1) {
            rc = NM_ERR;
        }
    }

    nm_str_free(&path);

    return rc;
}

static const char *nm_find_ovf(const nm_vect_t *files)
{
    for (size_t n = 0; n < files->n_memb; n++) {
        const char *file = nm_vect_at(files, n);
        size_t len = strlen(file);

        if (len < 4) {
            continue;
        }

        if (nm_str_cmp_tt(file + (len - 4), ".ovf") == NM_OK) {
            return file;
        }
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

static int nm_register_xml_ns(nm_xml_xpath_ctx_pt ctx, int version)
{
    if (version == 1) {
        if ((__nm_register_xml_ns(ctx, NM_XML_OVF_NS,
                        NM_XML_OVF1_HREF) == NM_ERR) ||
                (__nm_register_xml_ns(ctx, NM_XML_RASD_NS,
                                      NM_XML_RASD_HREF) == NM_ERR)) {
            return NM_ERR;
        }
    } else if (version == 2) {
        if ((__nm_register_xml_ns(ctx, NM_XML_OVF_NS,
                        NM_XML_OVF2_HREF) == NM_ERR) ||
                (__nm_register_xml_ns(ctx, NM_XML_SASD_NS,
                                      NM_XML_SASD_HREF)) ||
                (__nm_register_xml_ns(ctx, NM_XML_RASD_NS,
                                      NM_XML_RASD_HREF) == NM_ERR)) {
            return NM_ERR;
        }
    } else {
        nm_warn(_(NM_MSG_BAD_OVF));
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

    if ((xml_ns = xmlCharStrdup(ns)) == NULL) {
        return NM_ERR;
    }

    if ((xml_href = xmlCharStrdup(href)) == NULL) {
        xmlFree(xml_ns);
        return NM_ERR;
    }

    if (xmlXPathRegisterNs(ctx, xml_ns, xml_href) != 0) {
        rc = NM_ERR;
    }

    xmlFree(xml_ns);
    xmlFree(xml_href);

    return rc;
}

static nm_xml_xpath_obj_pt nm_exec_xpath(const char *xpath,
                                         nm_xml_xpath_ctx_pt ctx)
{
    nm_xml_char_t *xml_xpath;
    nm_xml_xpath_obj_pt obj = NULL;

    if ((xml_xpath = xmlCharStrdup(xpath)) == NULL) {
        return NULL;
    }

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

static void nm_ovf_get_drives(nm_vect_t *drives, nm_xml_xpath_ctx_pt ctx,
        int version)
{
    nm_xml_xpath_obj_pt obj_id;
    nm_str_t xpath = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;
    nm_drive_t drive = NM_INIT_DRIVE;
    size_t ndrives;

    if (version == 1) {
        if ((obj_id = nm_exec_xpath(NM_XPATH_DRIVE_ID1, ctx)) == NULL) {
            nm_bug("%s: cannot get drive_id from ovf file", __func__);
        }
    } else {
        if ((obj_id = nm_exec_xpath(NM_XPATH_DRIVE_ID2, ctx)) == NULL) {
            nm_bug("%s: cannot get drive_id from ovf file", __func__);
        }
    }

    if (!obj_id->nodesetval || ((ndrives = obj_id->nodesetval->nodeNr) == 0)) {
        nm_bug("%s: no drives was found", __func__);
    }

    for (size_t n = 0; n < ndrives; n++) {
        nm_xml_node_pt node_id = obj_id->nodesetval->nodeTab[n];

        if ((node_id->type == XML_TEXT_NODE) ||
                (node_id->type == XML_ATTRIBUTE_NODE)) {
            nm_xml_char_t *xml_id = xmlNodeGetContent(node_id);

            char *id = strrchr((char *) xml_id, '/');
            uint64_t capacity = 0;

            if (id == NULL) {
                nm_bug("%s: NULL disk id", __func__);
            }

            id++;
            nm_debug("ova: drive_id: %s\n", id);

            nm_str_format(&xpath, NM_XPATH_DRIVE_CAP, id);
            nm_ovf_get_text(&buf, ctx, xpath.data, "capacity");
            capacity = nm_str_stoul(&buf, 10);
            nm_str_format(&drive.capacity, "%g",
                    (double) capacity / 1073741824);

            nm_str_format(&xpath, NM_XPATH_DRIVE_REF, id);
            nm_ovf_get_text(&buf, ctx, xpath.data, "file ref");
            nm_str_format(&xpath, NM_XPATH_DRIVE_HREF, buf.data);
            nm_ovf_get_text(&buf, ctx, xpath.data, "file href");
            nm_str_copy(&drive.file_name, &buf);

            nm_vect_insert(drives, &drive, sizeof(nm_drive_t),
                           nm_drive_vect_ins_cb);

            xmlFree(xml_id);
        }
    }

    nm_str_free(&xpath);
    nm_str_free(&buf);
    nm_drive_free(&drive);

    xmlXPathFreeObject(obj_id);
}

static uint32_t nm_ovf_get_neth(nm_xml_xpath_ctx_pt ctx)
{
    uint32_t neth = 0;
    nm_xml_xpath_obj_pt obj;

    if ((obj = nm_exec_xpath(NM_XPATH_NETH, ctx)) == NULL) {
        nm_bug("%s: cannot get interfaces from ovf file", __func__);
    }

    neth = obj->nodesetval->nodeNr;
    nm_debug("ova: eth num: %u\n", neth);

    xmlXPathFreeObject(obj);

    return neth;
}

static uint32_t nm_ovf_get_usb(nm_xml_xpath_ctx_pt ctx)
{
    uint32_t node_num = 0;
    nm_xml_xpath_obj_pt obj;

    if ((obj = nm_exec_xpath(NM_XPATH_USB_EHCI, ctx)) == NULL) {
        nm_bug("%s: cannot get usb settings from ovf file", __func__);
    }

    node_num = obj->nodesetval->nodeNr;
    if (node_num) {
        nm_debug("ova: USB Controller (EHCI) enabled\n");
    }

    xmlXPathFreeObject(obj);

    return node_num;
}

static void nm_ovf_get_text(nm_str_t *res, nm_xml_xpath_ctx_pt ctx,
                            const char *xpath, const char *param)
{
    nm_xml_xpath_obj_pt obj;
    nm_xml_node_pt node;

    if ((obj = nm_exec_xpath(xpath, ctx)) == NULL) {
        nm_bug("%s: cannot get %s from ovf file", __func__, param);
    }

    if (!obj->nodesetval || !obj->nodesetval->nodeNr) {
        nm_bug("%s: xpath return zero result", __func__);
    }

    node = obj->nodesetval->nodeTab[0];

    if ((node->type == XML_TEXT_NODE) || (node->type == XML_ATTRIBUTE_NODE)) {
        nm_xml_char_t *xml_text = xmlNodeGetContent(node);

        nm_str_alloc_text(res, (char *) xml_text);
        xmlFree(xml_text);
    }

    nm_debug("ova: %s: %s\n", param, res->data);

    xmlXPathFreeObject(obj);
}

void nm_drive_vect_ins_cb(void *unit_p, const void *ctx)
{
    nm_str_copy(nm_drive_file(unit_p), nm_drive_file(ctx));
    nm_str_copy(nm_drive_size(unit_p), nm_drive_size(ctx));
}

void nm_drive_vect_free_cb(void *unit_p)
{
    nm_str_free(nm_drive_file(unit_p));
    nm_str_free(nm_drive_size(unit_p));
}

static inline void nm_drive_free(nm_drive_t *d)
{
    nm_str_free(&d->file_name);
    nm_str_free(&d->capacity);
}

static void nm_ovf_convert_drives(const nm_vect_t *drives, const nm_str_t *name,
        const char *templ_path, const nm_str_t *format)
{
    nm_str_t vm_dir = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t argv = NM_INIT_VECT;

    nm_str_format(&vm_dir, "%s/%s", nm_cfg_get()->vm_dir.data, name->data);

    if (mkdir(vm_dir.data, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
        nm_bug(_("%s: cannot create VM directory %s: %s"),
               __func__, vm_dir.data, strerror(errno));
    }

    for (size_t n = 0; n < drives->n_memb; n++) {
        nm_str_format(&buf, "%s/qemu-img", nm_cfg_get()->qemu_bin_path.data);
        nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

        nm_vect_insert_cstr(&argv, "convert");
        nm_vect_insert_cstr(&argv, "-O");
        nm_vect_insert_cstr(&argv, format->data);

        nm_str_format(&buf, "%s/%s",
            templ_path, (nm_drive_file(drives->data[n]))->data);
        nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

        nm_str_format(&buf, "%s/%s",
            vm_dir.data, (nm_drive_file(drives->data[n]))->data);
        nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

        nm_cmd_str(&buf, &argv);
        nm_debug("ova: exec: %s\n", buf.data);

        nm_vect_end_zero(&argv);
        if (nm_spawn_process(&argv, NULL) != NM_OK) {
            rmdir(vm_dir.data);
            nm_bug(_("%s: cannot create image file"), __func__);
        }

        nm_vect_free(&argv, NULL);
    }

    nm_str_free(&vm_dir);
    nm_str_free(&buf);
}

static void nm_ovf_to_db(nm_vm_t *vm, const nm_vect_t *drives)
{
    uint64_t last_mac;
    uint32_t last_vnc;

    nm_str_alloc_text(&vm->ifs.driver, NM_DEFAULT_NETDRV);

    last_mac = nm_form_get_last_mac();
    last_vnc = nm_form_get_free_vnc();

    nm_str_format(&vm->vncp, "%u", last_vnc);

    nm_add_vm_to_db(vm, last_mac, NM_IMPORT_VM, drives);
}

static int nm_ova_get_data(nm_vm_t *vm, int *version)
{
    int rc;
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t ver = NM_INIT_STR;

    nm_get_field_buf(fields[NM_OVA_FLD_SRC], &vm->srcp);
    nm_get_field_buf(fields[NM_OVA_FLD_ARCH], &vm->arch);
    nm_get_field_buf(fields[NM_OVA_FLD_VER], &ver);
    nm_get_field_buf(fields[NM_OVA_FLD_FMT], &vm->drive.format);
    if (field_status(fields[NM_OVA_FLD_NAME])) {
        nm_get_field_buf(fields[NM_OVA_FLD_NAME], &vm->name);
        nm_form_check_data(_(NM_LC_OVF_FORM_NAME), vm->name, err);
    }

    nm_form_check_data(_(NM_LC_OVF_FORM_PATH), vm->srcp, err);
    nm_form_check_data(_(NM_LC_OVF_FORM_ARCH), vm->arch, err);
    nm_form_check_data(_(NM_LC_OVF_FORM_DRV_FORMAT), vm->drive.format, err);
    nm_form_check_data(_(NM_LC_OVF_FORM_VER), ver, err);

    if (nm_str_cmp_st(&ver, nm_ovf_version[0]) == NM_OK) {
        *version = 1;
    } else if (nm_str_cmp_st(&ver, nm_ovf_version[1]) == NM_OK) {
        *version = 2;
    }

    rc = nm_print_empty_fields(&err);

    nm_vect_free(&err, NULL);
    nm_str_free(&ver);

    return rc;
}

#endif /* NM_WITH_OVF_SUPPORT */
/* vim:set ts=4 sw=4: */
