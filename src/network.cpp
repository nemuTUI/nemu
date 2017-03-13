#include <string>

#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "window.h"

#define TUNDEV "/dev/net/tun"

namespace QManager {

enum tap_on_off {
    TAP_OFF = 0,
    TAP_ON
};

enum action {
    SET_LINK_UP,
    SET_LINK_ADDR
};

struct iplink_req {
    struct nlmsghdr n;
    struct ifinfomsg i;
    char buf[1024];
};

struct ipaddr_req {
    struct nlmsghdr n;
    struct ifaddrmsg i;
    char buf[256];
};

struct rtnl_handle {
    int32_t sd;
    uint32_t seq;
    struct sockaddr_nl sa;
};

struct nlmsgerr {
    int error;
    struct nlmsghdr msg;
};

static bool manage_tap(const std::string &name, int on_off);
static void link_change(const std::string &name, int action);
static void addr_change(const std::string &name, int action, const std::string &data);
static uint32_t get_tap_index(const std::string name);
static void rtnl_open(struct rtnl_handle *rth);
static void rtnl_talk(struct rtnl_handle *rth, struct nlmsghdr *n);
static bool add_attr(struct nlmsghdr *n, size_t mlen, int type, const void *data, size_t dlen);

bool net_add_tap(const std::string &name)
{
    return manage_tap(name, TAP_ON);
}

bool net_del_tap(const std::string &name)
{
    return manage_tap(name, TAP_OFF);
}

bool net_set_ipaddr(const std::string &name, const std::string &addr)
{
    bool rc = true;
    try
    {
        addr_change(name, SET_LINK_ADDR, addr);
    }
    catch (const std::runtime_error &e)
    {
        std::unique_ptr<PopupWarning> Warn(new PopupWarning(e.what(), 3, 60, 7));
        Warn->Init();
        Warn->Print(Warn->window);
        rc = false;
    }

    return rc;
}

static inline uint32_t get_tap_index(const std::string name)
{
    return if_nametoindex(name.c_str());
}

static bool add_attr(struct nlmsghdr *n, size_t mlen, int type, const void *data, size_t dlen)
{
    size_t len = RTA_LENGTH(dlen);
    struct rtattr *rta;

    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > mlen)
    {
        return false;
    }

    rta = (struct rtattr *) (((void *) n) + NLMSG_ALIGN(n->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = len;
    memcpy(RTA_DATA(rta), data, dlen);
    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);

    return true;
}

static void rtnl_open(struct rtnl_handle *rth)
{
    memset(rth, 0, sizeof(*rth));

    rth->sa.nl_family = AF_NETLINK;
    rth->sa.nl_groups = 0;

    if ((rth->sd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE)) == -1)
    {
        std::string err = _("Cannot open netlink socket: ") + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    if (bind(rth->sd, (struct sockaddr *) &rth->sa, sizeof(rth->sa)) == -1)
    {
        close(rth->sd);
        std::string err = _("Cannot bind netlink socket: ") + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    rth->seq = time(NULL);
}

static void rtnl_talk(struct rtnl_handle *rth, struct nlmsghdr *n)
{
    ssize_t len;
    struct sockaddr_nl sa;
    struct iovec iov;
    struct msghdr msg;
    struct nlmsghdr *nh;
    char buf[4096];

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
        throw std::runtime_error(_("Cannot talk to rtnetlink"));

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
            std::string err = _("RTNETLINK answers: ") +
                std::string(strerror(-nlerr->error));
            throw std::runtime_error(err);
        }
    }
}

static void link_change(const std::string &name, int action)
{
    struct iplink_req req;
    struct rtnl_handle rth;
    uint32_t dev_index;

    memset(&req, 0, sizeof(req));

    if ((dev_index = get_tap_index(name)) == 0)
    {
        std::string err = _("if_nametoindex: ") + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.n.nlmsg_flags = NLM_F_REQUEST;
    req.n.nlmsg_type = RTM_NEWLINK;

    req.i.ifi_family = AF_UNSPEC;
    req.i.ifi_index = dev_index;

    switch (action) {
    case SET_LINK_UP:
        req.i.ifi_change |= IFF_UP;
        req.i.ifi_flags |= IFF_UP;
        break;
    }

    rtnl_open(&rth);
    rtnl_talk(&rth, &req.n);
    close(rth.sd);
}

static void addr_change(const std::string &name, int action, const std::string &data)
{
    struct ipaddr_req req;
    struct rtnl_handle rth;
    uint32_t dev_index;

    memset(&req, 0, sizeof(req));

    if ((dev_index = get_tap_index(name)) == 0)
    {
        std::string err = _("if_nametoindex: ") + std::string(strerror(errno));
        throw std::runtime_error(err);
    }

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    req.n.nlmsg_type = RTM_NEWADDR;

    req.i.ifa_family = AF_INET;
    req.i.ifa_index = dev_index;
    req.i.ifa_scope = 0;

    switch (action) {
    case SET_LINK_ADDR:
        {
            struct in_addr addr;
            struct in_addr brd;
            struct in_addr mask;
            uint32_t cidr;
            VectorString vsaddr;
            char *token;
            char *saveptr = const_cast<char *>(data.c_str());

            while ((token = strtok_r(saveptr, "/", &saveptr)))
                vsaddr.push_back(token);

            if (vsaddr.size() != 2)
                throw std::runtime_error(_("Invalid address format: expected IPv4/CIDR"));

            if (inet_pton(AF_INET, vsaddr[0].c_str(), &addr.s_addr) != 1)
                throw std::runtime_error(_("Invalid IPv4 address"));

            try {
                cidr = std::stoi(vsaddr[1]);
            }
            catch (const std::out_of_range &e) {
                throw std::runtime_error(_("Out of range"));
            }

            if (cidr > 32 || !cidr)
                throw std::runtime_error(_("Invalid CIDR: expected [1-32]"));

            mask.s_addr = 0xfffffff;
            mask.s_addr <<= 32 - cidr;
            mask.s_addr = htonl(mask.s_addr);
            brd.s_addr = addr.s_addr | (~mask.s_addr);
            req.i.ifa_prefixlen = cidr;

            if (!add_attr(&req.n, sizeof(req), IFA_LOCAL, &addr.s_addr, sizeof(addr.s_addr)) ||
                !add_attr(&req.n, sizeof(req), IFA_BROADCAST, &brd.s_addr, sizeof(brd.s_addr)))
            {
                throw std::runtime_error(_("Error add_attr"));
            }
        }
        break;
    }

    rtnl_open(&rth);
    rtnl_talk(&rth, &req.n);
    close(rth.sd);
}

static bool manage_tap(const std::string &name, int on_off)
{
    int fd;
    struct ifreq ifr;
    bool rc = true;
    std::string err;

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags |= (IFF_NO_PI | IFF_TAP);
    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

    try
    {
        if ((fd = open(TUNDEV, O_RDWR)) < 0)
        {
            std::string err = _("Cannot open TUN device: ") + std::string(strerror(errno));
            throw std::runtime_error(err);
        }

        if (ioctl(fd, TUNSETIFF, &ifr) == -1)
        {
            std::string err = _("ioctl(TUNSETIFF): ") + std::string(strerror(errno));
            throw std::runtime_error(err);
        }

        if (ioctl(fd, TUNSETPERSIST, on_off) == -1)
        {
            std::string err = _("ioctl(TUNSETPERSIST): ") + std::string(strerror(errno));
            throw std::runtime_error(err);
        }

        if (on_off == TAP_ON)
            link_change(name, SET_LINK_UP);
    }
    catch (const std::runtime_error &e)
    {
        std::unique_ptr<PopupWarning> Warn(new PopupWarning(e.what(), 3, 60, 7));
        Warn->Init();
        Warn->Print(Warn->window);
        if (fd > 0)
            close(fd);
        rc = false;
    }
    
    close(fd);

    return rc;
}

} /* namespace QManager */
/* vim:set ts=4 sw=4 fdm=marker: */
