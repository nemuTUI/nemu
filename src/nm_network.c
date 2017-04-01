#include <nm_core.h>
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

/* vim:set ts=4 sw=4 fdm=marker: */
