// Based on code from https://blog.masa23.jp/en/posts/2024/09/17/xdp/
// Original implementation dropped all ICMP packets and counted occurrences per source IP.
// This version adds a threshold check, dropping ICMP traffic only if the source IP exceeds 20 packets.

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <arpa/inet.h>
#include <linux/in.h>
#include <linux/ipv6.h>
#include <stddef.h>
#include <stdbool.h>

// KATRAN includes
#include "balancer_consts.h"
#include "balancer_helpers.h"
#include "balancer_structs.h"
#include "balancer_maps.h"
#include "bpf.h"
#include "bpf_helpers.h"
#include "jhash.h"
#include "pckt_encap.h"
#include "pckt_parsing.h"
#include "handle_icmp.h"

// Definition of eBPF Map
struct bpf_map_def SEC("maps") icmp_count_map = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(uint64_t),
    .max_entries = 1024,
};
BPF_ANNOTATE_KV_PAIR(icmp_count_map, uint32_t, uint64_t);

SEC("xdp")
int xdp_drop_icmp(struct xdp_md *ctx) {
    void *data_end = (void *)(unsigned long)ctx->data_end;
    void *data = (void *)(unsigned long)ctx->data;

    struct ethhdr *eth = data;
    if ((void *)eth + sizeof(*eth) > data_end)
        return XDP_DROP;

    if (eth->h_proto != __builtin_bswap16(ETH_P_IP))
        return XDP_PASS;

    struct iphdr *ip = data + sizeof(*eth);
    if ((void *)ip + sizeof(*ip) > data_end)
        return XDP_DROP;

    if (ip->protocol != IPPROTO_ICMP)
        return XDP_PASS;

    struct icmphdr *icmp = (void *)ip + sizeof(*ip);

    if ((void *)icmp + sizeof(*icmp) > data_end)
        return XDP_DROP;

    uint32_t src_ip = ip->saddr;

    uint64_t *count = bpf_map_lookup_elem(&icmp_count_map, &src_ip);

    if (count) {
        *count += 1;
    } else {
        // If NULL, add to icmp_count_map
        bpf_map_update_elem(&icmp_count_map, &src_ip, &(uint64_t){1}, BPF_ANY);
    }  

    count = bpf_map_lookup_elem(&icmp_count_map, &src_ip);

    if (count && *count > 20) {
        return XDP_DROP;
    }

    return XDP_PASS;
}

char _license[] SEC("license") = "MIT";
