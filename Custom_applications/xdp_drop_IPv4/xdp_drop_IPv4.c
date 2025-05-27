#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <stddef.h>
#include <stdbool.h>

#define BE_ETH_P_IP 8
#define BE_ETH_P_IPV6 56710
#define ETH_ALEN	6		/* Octets in one ethernet addr	 */

struct eth_hdr {
    unsigned char eth_dest[ETH_ALEN];
    unsigned char eth_source[ETH_ALEN];
    unsigned short  eth_proto;
};

#define SEC(NAME) __attribute__((section(NAME), used))

SEC("xdp_drop_IPv4")
int xdp_prog(struct xdp_md *ctx)
{
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;
    struct eth_hdr *eth = data;

    /* Check if the packet is large enough to contain an Ethernet header */
    if (data + sizeof(struct eth_hdr) > data_end)
        return XDP_DROP;

    if (eth->eth_proto == BE_ETH_P_IP)
        return XDP_DROP;

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";