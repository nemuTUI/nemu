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

/* vim:set ts=4 sw=4 fdm=marker: */
