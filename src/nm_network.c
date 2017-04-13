#include <nm_core.h>
#include <nm_utils.h>
#include <nm_network.h>

void nm_net_mac_to_str(uint64_t maddr, nm_str_t *res)
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
                netaddr.cidr = nm_str_stoui(&buf);
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

/* vim:set ts=4 sw=4 fdm=marker: */
