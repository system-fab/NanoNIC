// Based on code from https://blog.masa23.jp/en/posts/2024/09/17/xdp/
// Original implementation dropped all ICMP packets and counted occurrences per source IP.
// This version adds a threshold check, dropping ICMP traffic only if the source IP exceeds 20 packets.

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <arpa/inet.h>

// Definition of eBPF Map
struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, uint32_t);
    __type(value, uint64_t);
    __uint(max_entries, 1024);
} icmp_count_map SEC(".maps");

SEC("xdp")
int xdp_drop_icmp(struct xdp_md *ctx)
{
    void *data_end = (void *)(unsigned long)ctx->data_end;
    void *data = (void *)(unsigned long)ctx->data;

    struct ethhdr *eth = data;
    if ((void *)eth + sizeof(*eth) > data_end)
        return XDP_DROP;

    if (eth->h_proto != __builtin_bswap16(ETH_P_IP))
        return XDP_PASS;

    struct iphdr *ip = (void *)eth + sizeof(*eth);
    if ((void *)ip + sizeof(*ip) > data_end)
        return XDP_DROP;

    if (ip->protocol != IPPROTO_ICMP)
        return XDP_PASS;

    void *icmp_ptr = (void *)ip + sizeof(*ip);
    if (icmp_ptr + sizeof(struct icmphdr) > data_end)
        return XDP_DROP;

    struct icmphdr *icmp = icmp_ptr;

    uint32_t src_ip = ip->saddr;
    uint64_t init_val = 1;
    uint64_t *count = bpf_map_lookup_elem(&icmp_count_map, &src_ip);

    if (!count) {
        bpf_map_update_elem(&icmp_count_map, &src_ip, &init_val, BPF_ANY);
        return XDP_PASS;
    }

    __sync_fetch_and_add(count, 1);

    if (*count > 20)
        return XDP_DROP;

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";