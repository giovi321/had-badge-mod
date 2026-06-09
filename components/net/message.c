/* See net/message.h. */
#include "net/message.h"
#include <stdio.h>
#include <string.h>

void net_node_id_str(uint32_t node, char *buf)
{
    sprintf(buf, "!%08lx", (unsigned long)node);
}

void net_node_label(uint32_t node, const char *long_name, const char *short_name,
                    char *buf, int cap)
{
    if (long_name && long_name[0]) { snprintf(buf, cap, "%s", long_name); return; }
    if (short_name && short_name[0]) { snprintf(buf, cap, "%s", short_name); return; }
    char id[12];
    net_node_id_str(node, id);
    snprintf(buf, cap, "%s", id);
}
