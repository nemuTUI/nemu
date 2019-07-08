#include <nm_core.h>
#include <nm_utils.h>
#include <nm_network.h>

#include <sys/ioctl.h>
#include <net/if.h>

#if defined (NM_OS_LINUX)
#include <time.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define NM_TUNDEV "/dev/net/tun"
#define NM_NET_MACVTAP "macvtap"
#define NM_NET_VETH    "veth"
#define NM_NET_VETH_INFO_PEER 1

#define NLMSG_TAIL(n) \
    ((struct rtattr *) (((char *) (n)) + NLMSG_ALIGN((n)->nlmsg_len)))

enum {
    NM_MACVTAP_BRIDGE = 1,
    NM_MACVTAP_PRIVATE = 2
};

enum {
    NM_MACVTAP_PRIVATE_MODE = 1,
    NM_MACVTAP_BRIDGE_MODE = 4
};

struct iplink_req {
    struct nlmsghdr n;
    struct ifinfomsg i;
};

struct ipaddr_req {
    struct nlmsghdr n;
    struct ifaddrmsg i;
};

struct rtnl_handle {
    int32_t sd;
    uint32_t seq;
    struct sockaddr_nl sa;
};

static void nm_net_set_link_status(const nm_str_t *name, int action);
static void nm_net_rtnl_open(struct rtnl_handle *rth);
static void nm_net_rtnl_talk(struct rtnl_handle *rth, struct nlmsghdr *n,
                             struct nlmsghdr *res, size_t res_len);
static int nm_net_add_attr(struct nlmsghdr *n, size_t mlen,
                           int type, const void *data, size_t dlen);
static struct rtattr *nm_net_add_attr_nest(struct nlmsghdr *n, size_t mlen,
                                           int type);
static int nm_net_add_attr_nest_end(struct nlmsghdr *n, struct rtattr *nest);
#endif /* NM_OS_LINUX */

enum tap_on_off {
    NM_TAP_OFF = 0,
    NM_TAP_ON
};

enum action {
    NM_SET_LINK_UP,
    NM_SET_LINK_DOWN,
    NM_SET_LINK_ADDR
};

#if defined (NM_OS_LINUX)
static size_t nm_net_mac_a2n(const nm_str_t *addr, char *res, size_t len);
#endif
static void nm_net_manage_tap(const nm_str_t *name, int on_off);
static void nm_net_addr_change(const nm_str_t *name, const nm_str_t *net,
                               int action);

int nm_net_iface_exists(const nm_str_t *name)
{
    if (if_nametoindex(name->data) == 0)
        return NM_ERR;

    return NM_OK;
}

uint32_t nm_net_iface_idx(const nm_str_t *name)
{
    return if_nametoindex(name->data);
}

void nm_net_add_tap(const nm_str_t *name)
{
    nm_net_manage_tap(name, NM_TAP_ON);
}

void nm_net_del_tap(const nm_str_t *name)
{
    nm_net_manage_tap(name, NM_TAP_OFF);
}

#if defined (NM_OS_LINUX)
void nm_net_add_macvtap(const nm_str_t *name, const nm_str_t *parent,
                        const nm_str_t *maddr, int type)
{
    struct iplink_req req;
    struct rtnl_handle rth;
    struct rtattr *linkinfo, *data;
    uint32_t dev_index, mode;
    size_t mac_len;
    char macn[32] = {0};

    memset(&req, 0, sizeof(req));

    if ((dev_index = if_nametoindex(parent->data)) == 0)
        nm_bug("%s: if_nametoindex: %s", __func__, strerror(errno));

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    req.n.nlmsg_type = RTM_NEWLINK;

    req.i.ifi_family = AF_UNSPEC;
    req.i.ifi_flags |= IFF_UP;

    mac_len = nm_net_mac_a2n(maddr, macn, sizeof(macn));
    if ((nm_net_add_attr(&req.n, sizeof(req), IFLA_ADDRESS,
            macn, mac_len) != NM_OK))
    {
        nm_bug("%s: Error add_attr", __func__);
    }

    if ((nm_net_add_attr(&req.n, sizeof(req), IFLA_LINK,
            &dev_index, sizeof(dev_index)) != NM_OK))
    {
        nm_bug("%s: Error add_attr", __func__);
    }

    if ((nm_net_add_attr(&req.n, sizeof(req), IFLA_IFNAME,
            name->data, name->len) != NM_OK))
    {
        nm_bug("%s: Error add_attr", __func__);
    }

    linkinfo = nm_net_add_attr_nest(&req.n, sizeof(req), IFLA_LINKINFO);
    if ((nm_net_add_attr(&req.n, sizeof(req), IFLA_INFO_KIND,
            NM_NET_MACVTAP, strlen(NM_NET_MACVTAP)) != NM_OK))
    {
        nm_bug("%s: Error add_attr", __func__);
    }

    switch (type) {
    case NM_MACVTAP_BRIDGE:
        mode = NM_MACVTAP_BRIDGE_MODE;
        break;
    case NM_MACVTAP_PRIVATE:
        mode = NM_MACVTAP_PRIVATE_MODE;
        break;
    }

    data = nm_net_add_attr_nest(&req.n, sizeof(req), IFLA_INFO_DATA);
    if ((nm_net_add_attr(&req.n, sizeof(req), IFLA_MACVLAN_MODE,
            &mode, sizeof(mode)) != NM_OK))
    {
        nm_bug("%s: Error add_attr", __func__);
    }

    nm_net_add_attr_nest_end(&req.n, data);
    nm_net_add_attr_nest_end(&req.n, linkinfo);

    nm_net_rtnl_open(&rth);
    nm_net_rtnl_talk(&rth, &req.n, NULL, 0);
    close(rth.sd);
}

void nm_net_add_veth(const nm_str_t *l_name, const nm_str_t *r_name)
{
    struct iplink_req req;
    struct rtnl_handle rth;
    struct rtattr *linkinfo, *data;

    memset(&req, 0, sizeof(req));

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    req.n.nlmsg_type = RTM_NEWLINK;

    req.i.ifi_family = AF_UNSPEC;
    req.i.ifi_index = 0;

    if ((nm_net_add_attr(&req.n, sizeof(req), IFLA_IFNAME,
            l_name->data, l_name->len) != NM_OK))
    {
        nm_bug("%s: Error add_attr", __func__);
    }

    linkinfo = nm_net_add_attr_nest(&req.n, sizeof(req), IFLA_LINKINFO);
    if ((nm_net_add_attr(&req.n, sizeof(req), IFLA_INFO_KIND,
            NM_NET_VETH, strlen(NM_NET_VETH)) != NM_OK))
    {
        nm_bug("%s: Error add_attr", __func__);
    }

    data = nm_net_add_attr_nest(&req.n, sizeof(req), IFLA_INFO_DATA);

    {
        struct rtattr *data;
        uint32_t ifi_flags, ifi_change;
        struct ifinfomsg *ifm, *peer_ifm;

        ifm = NLMSG_DATA(&req.n);
        ifi_flags = ifm->ifi_flags;
        ifi_change = ifm->ifi_change;
        ifm->ifi_flags = 0;
        ifm->ifi_change = 0;

        data = NLMSG_TAIL(&req.n);
        if ((nm_net_add_attr(&req.n, sizeof(req), NM_NET_VETH_INFO_PEER,
                NULL, 0) != NM_OK))
        {
            nm_bug("%s: Error add_attr", __func__);
        }

        req.n.nlmsg_len += sizeof(struct ifinfomsg);
        if ((nm_net_add_attr(&req.n, sizeof(req), IFLA_IFNAME,
                r_name->data, r_name->len) != NM_OK))
        {
            nm_bug("%s: Error add_attr", __func__);
        }

        peer_ifm = RTA_DATA(data);
        peer_ifm->ifi_index = 0;
        peer_ifm->ifi_flags = ifm->ifi_flags;
        peer_ifm->ifi_change = ifm->ifi_change;
        ifm->ifi_flags = ifi_flags;
        ifm->ifi_change = ifi_change;

        data->rta_len = (char *) NLMSG_TAIL(&req.n) - (char *) data;
    }

    nm_net_add_attr_nest_end(&req.n, data);
    nm_net_add_attr_nest_end(&req.n, linkinfo);

    nm_net_rtnl_open(&rth);
    nm_net_rtnl_talk(&rth, &req.n, NULL, 0);
    close(rth.sd);
}

void nm_net_del_iface(const nm_str_t *name)
{
    struct iplink_req req;
    struct rtnl_handle rth;
    uint32_t dev_index;

    memset(&req, 0, sizeof(req));

    if ((dev_index = if_nametoindex(name->data)) == 0)
        nm_bug("%s: if_nametoindex: %s", __func__, strerror(errno));

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.n.nlmsg_flags = NLM_F_REQUEST;
    req.n.nlmsg_type = RTM_DELLINK;

    req.i.ifi_family = AF_UNSPEC;
    req.i.ifi_index = dev_index;

    nm_net_rtnl_open(&rth);
    nm_net_rtnl_talk(&rth, &req.n, NULL, 0);
    close(rth.sd);
}
#endif /* NM_OS_LINUX */

void nm_net_set_ipaddr(const nm_str_t *name, const nm_str_t *addr)
{
    nm_net_addr_change(name, addr, NM_SET_LINK_ADDR);
}

int nm_net_verify_mac(const nm_str_t *mac)
{
    int rc = NM_ERR;
    const char *regex = "^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$";
    regex_t reg;

    if (regcomp(&reg, regex, REG_EXTENDED) != 0)
    {
        nm_bug("%s: regcomp failed", __func__);
    }

    if (regexec(&reg, mac->data, 0, NULL, 0) == 0)
        rc = NM_OK;

    regfree(&reg);

    return rc;
}

int nm_net_verify_ipaddr4(const nm_str_t *src, nm_net_addr_t *net,
                          nm_str_t *err)
{
    int rc = NM_OK;
    nm_str_t buf = NM_INIT_STR;
    nm_str_t addr = NM_INIT_STR;
    char *token, *saveptr;
    nm_net_addr_t netaddr = NM_INIT_NETADDR;
    int n = 0;

    nm_str_copy(&buf, src);
    saveptr = buf.data;

    if (src->data[src->len - 1] == '/')
    {
        nm_str_alloc_text(err, _("Invalid address format: expected IPv4/CIDR"));
        rc = NM_ERR;
        goto out;
    }

    while ((token = strtok_r(saveptr, "/", &saveptr)))
    {
        switch (n) {
        case 0:
            nm_str_alloc_text(&addr, token);
            break;
        case 1:
            {
                nm_str_t buf = NM_INIT_STR;
                nm_str_alloc_text(&buf, token);
                netaddr.cidr = nm_str_stoui(&buf, 10);
                nm_str_free(&buf);
            }
            break;
        default:
            nm_str_alloc_text(err, _("Invalid address format: expected IPv4/CIDR"));
            rc = NM_ERR;
            goto out;
        }
        n++;
    }

    if ((addr.len == 0) ||
        (inet_pton(AF_INET, addr.data, &netaddr.addr.s_addr) != 1))
    {
        nm_str_alloc_text(err, _("Invalid IPv4 address"));
        rc = NM_ERR;
        goto out;
    }

    if ((netaddr.cidr > 32) || (netaddr.cidr == 0))
    {
        nm_str_alloc_text(err, _("Invalid CIDR: expected [1-32]"));
        rc = NM_ERR;
        goto out;
    }

    if (net != NULL)
    {
        net->cidr = netaddr.cidr;
        memcpy(&net->addr, &netaddr.addr, sizeof(struct in_addr));
    }

out:
    nm_str_free(&addr);
    nm_str_free(&buf);
    return rc;
}

void nm_net_fix_tap_name(nm_str_t *name, const nm_str_t *maddr)
{
    nm_str_t maddr_copy = NM_INIT_STR;

    if (name->len <= 15) /* Linux tap iface max name len */
        return;

    nm_str_copy(&maddr_copy, maddr);
    nm_str_remove_char(&maddr_copy, ':');

    nm_str_trunc(name, 0);
    nm_str_format(name, "vm-%s", maddr_copy.data);

    nm_str_free(&maddr_copy);
}

void nm_net_mac_n2a(uint64_t maddr, nm_str_t *res)
{
    char buf[64] = {0};
    int pos = 0;

    for (int byte = 0; byte < 6; byte++)
    {
        uint32_t octet = ((maddr >> 40) & 0xff);

        pos += snprintf(buf + pos, sizeof(buf) - pos, "%02x:", octet);
        maddr <<= 8;
    }

    buf[--pos] = '\0';

    nm_str_alloc_text(res, buf);
}

#if defined (NM_OS_LINUX)
static size_t nm_net_mac_a2n(const nm_str_t *addr, char *res, size_t len)
{
    nm_str_t copy = NM_INIT_STR;
    size_t n = 0;
    char *savep = NULL;

    nm_str_copy(&copy, addr);
    savep = copy.data;

    for (; n < len; n++)
    {
        char *cp = strchr(copy.data, ':');

        if (cp)
        {
            *cp = '\0';
            cp++;
        }

        res[n] = nm_str_stoui(&copy, 16);

        if (!cp)
            break;
        copy.data = cp;
    }

    copy.data = savep;
    nm_str_free(&copy);

    return n + 1;
}
#endif /* NM_OS_LINUX */

static void nm_net_manage_tap(const nm_str_t *name, int on_off)
{
    int fd = -1;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
#if defined (NM_OS_LINUX)
    ifr.ifr_flags |= (IFF_NO_PI | IFF_TAP);
    nm_strlcpy(ifr.ifr_name, name->data, IFNAMSIZ);

    if ((fd = open(NM_TUNDEV, O_RDWR)) < 0)
        nm_bug(_("%s: cannot open TUN device: %s"), __func__, strerror(errno));

    if (ioctl(fd, TUNSETIFF, &ifr) == -1)
        nm_bug("%s: ioctl(TUNSETIFF): %s", __func__, strerror(errno));

    if (ioctl(fd, TUNSETPERSIST, on_off) == -1)
        nm_bug("%s: ioctl(TUNSETPERSIST): %s", __func__, strerror(errno));

    if (on_off == NM_TAP_ON)
        nm_net_link_up(name);
#elif defined (NM_OS_FREEBSD)
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        nm_bug("%s: socket: %s", __func__, strerror(errno));

    strlcpy(ifr.ifr_name, name->data, sizeof(ifr.ifr_name));

    if (on_off == NM_TAP_OFF)
    {
        if (ioctl(sock, SIOCIFDESTROY, &ifr) == -1)
            nm_bug("%s: ioctl(SIOCIFDESTROY): %s", __func__, strerror(errno));
    }
    close(sock);
#endif /* NM_OS_LINUX */
    if (fd > 0)
        close(fd);
}

#if defined (NM_OS_LINUX)
static void nm_net_rtnl_open(struct rtnl_handle *rth)
{
    memset(rth, 0, sizeof(*rth));

    rth->sa.nl_family = AF_NETLINK;
    rth->sa.nl_groups = 0;

    if ((rth->sd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE)) == -1)
        nm_bug("%s: cannot open netlink socket: %s", __func__, strerror(errno));

    if (bind(rth->sd, (struct sockaddr *) &rth->sa, sizeof(rth->sa)) == -1)
    {
        close(rth->sd);
        nm_bug("%s: cannot bind netlink socket: %s", __func__, strerror(errno));
    }

    rth->seq = time(NULL);
}

static int nm_net_add_attr(struct nlmsghdr *n, size_t mlen,
                           int type, const void *data, size_t dlen)
{
    size_t len = RTA_LENGTH(dlen);
    struct rtattr *rta;

    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > mlen)
    {
        return NM_ERR;
    }

    rta = (struct rtattr *) (((char *) n) + NLMSG_ALIGN(n->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = len;
    memcpy(RTA_DATA(rta), data, dlen);
    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);

    return NM_OK;
}

static struct rtattr *nm_net_add_attr_nest(struct nlmsghdr *n, size_t mlen,
                                           int type)
{
    struct rtattr *nest = NLMSG_TAIL(n);

    if (nm_net_add_attr(n, mlen, type, NULL, 0) != NM_OK)
        nm_bug("%s: Error add_attr", __func__);

    return nest;
}

static int nm_net_add_attr_nest_end(struct nlmsghdr *n, struct rtattr *nest)
{
    nest->rta_len = (char *) NLMSG_TAIL(n) - (char *) nest;

    return n->nlmsg_len;
}

static void nm_net_rtnl_talk(struct rtnl_handle *rth, struct nlmsghdr *n,
                             struct nlmsghdr *res, size_t res_len)
{
    ssize_t len;
    struct sockaddr_nl sa;
    struct iovec iov;
    struct msghdr msg;
    struct nlmsghdr *nh;
    char buf[16384] = {0};

    iov.iov_base = (void *) n;
    iov.iov_len = n->nlmsg_len;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;

    n->nlmsg_flags |= NLM_F_ACK;
    n->nlmsg_seq = ++rth->seq;

    if ((len = sendmsg(rth->sd, &msg, 0)) < 0)
        nm_bug("%s: cannot talk to rtnetlink", __func__);

    memset(buf, 0, sizeof(buf));

    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);

    len = recvmsg(rth->sd, &msg, 0);

    for (nh = (struct nlmsghdr *) buf; NLMSG_OK(nh, len);
         nh = NLMSG_NEXT(nh, len))
    {
        if (nh->nlmsg_type == NLMSG_DONE)
            return;
        if (nh->nlmsg_type == NLMSG_ERROR)
        {
            struct nlmsgerr *nlerr = (struct nlmsgerr *) NLMSG_DATA(nh);
            if (!nlerr->error)
                return;
            nm_bug("%s: RTNETLINK answers: %s",
                __func__, strerror(-nlerr->error));
        }
        if (res)
            memcpy(res, nh, NM_MIN(res_len, nh->nlmsg_len));
    }
}

void nm_net_link_up(const nm_str_t *name)
{
    nm_net_set_link_status(name, NM_SET_LINK_UP);
}

void nm_net_link_down(const nm_str_t *name)
{
    nm_net_set_link_status(name, NM_SET_LINK_DOWN);
}

int nm_net_link_status(const nm_str_t *name)
{
    struct iplink_req req;
    struct rtnl_handle rth;
    struct {
        struct nlmsghdr n;
        char buf[16384];
    } result;

    memset(&req, 0, sizeof(req));

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.n.nlmsg_flags = NLM_F_REQUEST;
    req.n.nlmsg_type = RTM_GETLINK;
    req.i.ifi_family = AF_UNSPEC;

    if ((nm_net_add_attr(&req.n, sizeof(req), IFLA_IFNAME,
            name->data, name->len) != NM_OK))
    {
        nm_bug("%s: Error add_attr", __func__);
    }

    nm_net_rtnl_open(&rth);
    nm_net_rtnl_talk(&rth, &req.n, &result.n, sizeof(result));
    close(rth.sd);

    {
        struct ifinfomsg *ifi = NLMSG_DATA(&result.n);
        if (!(ifi->ifi_flags & IFF_UP))
            return NM_ERR;
    }

    return NM_OK;
}

static void nm_net_set_link_status(const nm_str_t *name, int action)
{
    struct iplink_req req;
    struct rtnl_handle rth;
    uint32_t dev_index;

    memset(&req, 0, sizeof(req));

    if ((dev_index = if_nametoindex(name->data)) == 0)
        nm_bug("%s: if_nametoindex: %s", __func__, strerror(errno));

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.n.nlmsg_flags = NLM_F_REQUEST;
    req.n.nlmsg_type = RTM_NEWLINK;

    req.i.ifi_family = AF_UNSPEC;
    req.i.ifi_index = dev_index;
    req.i.ifi_change |= IFF_UP;

    switch (action) {
    case NM_SET_LINK_UP:
        req.i.ifi_flags |= IFF_UP;
        break;
    case NM_SET_LINK_DOWN:
        req.i.ifi_flags &= ~IFF_UP;
        break;
    }

    nm_net_rtnl_open(&rth);
    nm_net_rtnl_talk(&rth, &req.n, NULL, 0);
    close(rth.sd);
}
#endif /* NM_OS_LINUX */

static void nm_net_addr_change(const nm_str_t *name, const nm_str_t *src,
                               int action)
{
#if defined (NM_OS_LINUX)
    struct ipaddr_req req;
    struct rtnl_handle rth;
    uint32_t dev_index;

    memset(&req, 0, sizeof(req));

    if ((dev_index = if_nametoindex(name->data)) == 0)
        nm_bug("%s: if_nametoindex: %s", __func__, strerror(errno));

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    req.n.nlmsg_type = RTM_NEWADDR;

    req.i.ifa_family = AF_INET;
    req.i.ifa_index = dev_index;
    req.i.ifa_scope = 0;

    switch (action) {
    case NM_SET_LINK_ADDR:
        {
            struct in_addr brd;
            struct in_addr mask;
            nm_net_addr_t net = NM_INIT_NETADDR;
            nm_str_t errmsg = NM_INIT_STR;

            if (nm_net_verify_ipaddr4(src, &net, &errmsg) != NM_OK)
                nm_bug("%s: %s", __func__, errmsg.data);

            mask.s_addr = ~((1 << (32 - net.cidr)) - 1);
            mask.s_addr = htonl(mask.s_addr);
            brd.s_addr = net.addr.s_addr | (~mask.s_addr);
            req.i.ifa_prefixlen = net.cidr;

            if ((nm_net_add_attr(&req.n, sizeof(req), IFA_LOCAL,
                    &net.addr.s_addr, sizeof(net.addr.s_addr)) != NM_OK) ||
                (nm_net_add_attr(&req.n, sizeof(req), IFA_BROADCAST,
                    &brd.s_addr, sizeof(brd.s_addr)) != NM_OK))
            {
                nm_bug("%s: Error add_attr", __func__);
            }

            nm_str_free(&errmsg);
        }
        break;
    }

    nm_net_rtnl_open(&rth);
    nm_net_rtnl_talk(&rth, &req.n, NULL, 0);
    close(rth.sd);
#else
    (void) name;
    (void) action;
    (void) src;
#endif /* NM_OS_LINUX */
}

/* vim:set ts=4 sw=4 fdm=marker: */
