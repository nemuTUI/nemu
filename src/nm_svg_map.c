#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_network.h>
#include <nm_svg_map.h>
#include <nm_database.h>
#include <nm_lan_settings.h>

#if defined (NM_WITH_NETWORK_MAP)
#include <gvc.h>

static char NM_EMPTY_STR[] = "";
static char NM_GV_LABEL[]  = "label";
static char NM_GV_STYLE[]  = "style";
static char NM_GV_SHAPE[]  = "shape";
static char NM_GV_FILL[]   = "filled";
static char NM_GV_FCOL[]   = "fillcolor";
static char NM_GV_RECT[]   = "rect";
static char NM_GV_SVG[]    = "svg";
static char NM_VM_COLOR[]  = "#4fbcdd";
static char NM_VE_COLOR[]  = "#59e088";

typedef Agraph_t nm_gvgraph_t;
typedef Agnode_t nm_gvnode_t;
typedef Agedge_t nm_gvedge_t;
typedef GVC_t    nm_gvctx_t;

void nm_svg_map(const char *path, const nm_vect_t *veths,
        int state, const nm_str_t *layout, const nm_str_t *group)
{
    nm_gvctx_t *gvc;
    nm_gvgraph_t *graph;

    if ((gvc = gvContext()) == NULL)
        nm_bug(_("%s: cannot create graphviz ctx"), __func__);

    graph = agopen("network_map", Agundirected, NULL);
    agattr(graph, AGRAPH, "overlap", "scalexy");
    agattr(graph, AGRAPH, "splines", "true");
    agattr(graph, AGRAPH, "sep", "+25,25");

    //@TODO Some of variables may be moved outside, and freed only once
    for (size_t v = 0; v < veths->n_memb; v++) {
        nm_vect_t vms = NM_INIT_VECT;
        nm_str_t lname = NM_INIT_STR;
        nm_str_t rname = NM_INIT_STR;
        nm_str_t query = NM_INIT_STR;
        nm_gvnode_t *vnode;
        size_t vms_count;

        nm_lan_parse_name(nm_vect_str(veths, v), &lname, &rname);

        if (!group->len) {
            switch (state) {
                case NM_SVG_STATE_UP:
                    if (nm_net_link_status(&lname) != NM_OK)
                        goto next;
                    break;
                case NM_SVG_STATE_DOWN:
                    if (nm_net_link_status(&lname) == NM_OK)
                        goto next;
                    break;
                default:
                    break;
            }
        }

        if (group->len) {
            nm_str_format(&query, NM_GET_IFMAPGR_SQL, group->data, lname.data, rname.data);
        } else {
            nm_str_format(&query, NM_GET_IFMAP_SQL, lname.data, rname.data);
        }
        nm_db_select(query.data, &vms);

        if (group->len && !vms.n_memb) {
            goto next;
        }

        vnode = agnode(graph, nm_vect_str_ctx(veths, v), NM_TRUE);
        agsafeset(vnode, NM_GV_STYLE, NM_GV_FILL, NM_EMPTY_STR);
        agsafeset(vnode, NM_GV_FCOL, NM_VE_COLOR, NM_EMPTY_STR);
        agsafeset(vnode, NM_GV_SHAPE, NM_GV_RECT, NM_EMPTY_STR);

        vms_count = vms.n_memb / 2;
        for (size_t n = 0; n < vms_count; n++) {
            size_t idx_shift = 2 * n;
            nm_gvnode_t *node = agnode(graph, nm_vect_str_ctx(&vms, idx_shift), NM_TRUE);
            nm_gvedge_t *edge = agedge(graph, node, vnode, NULL, NM_TRUE);

            agsafeset(edge, NM_GV_LABEL, nm_vect_str_ctx(&vms, idx_shift + 1), NM_EMPTY_STR);
            agsafeset(node, NM_GV_STYLE, NM_GV_FILL, NM_EMPTY_STR);
            agsafeset(node, NM_GV_FCOL, NM_VM_COLOR, NM_EMPTY_STR);
        }

        nm_vect_free(&vms, nm_str_vect_free_cb);
        nm_str_free(&query);
next:
        nm_str_free(&lname);
        nm_str_free(&rname);
    }

    gvLayout(gvc, graph, layout->data);
    gvRenderFilename(gvc, graph, NM_GV_SVG, path);

    gvFreeLayout(gvc, graph);
    agclose(graph);
    gvFreeContext(gvc);
}
#endif /* NM_WITH_NETWORK_MAP */
/* vim:set ts=4 sw=4: */
