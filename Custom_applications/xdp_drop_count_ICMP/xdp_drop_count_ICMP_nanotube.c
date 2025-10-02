// Fixed eBPF-safe version of your handler
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <arpa/inet.h>
#include <linux/in.h>
#include <linux/ipv6.h>
#include <stddef.h>
#include <stdbool.h>

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

struct bpf_map_def SEC("maps") icmp_count_map = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = sizeof(__u32),
    .value_size = sizeof(__u64),
    .max_entries = 1024,
};
BPF_ANNOTATE_KV_PAIR(icmp_count_map, __u32, __u64);

#define MONITOR_IP 0x6401A8C0  // Change to target IP (network byte order)

struct bpf_map_def SEC("maps") packet_count_map = {
    .type = BPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(__u32),
    .value_size = sizeof(__u64),
    .max_entries = 1,
};
BPF_ANNOTATE_KV_PAIR(packet_count_map, __u32, __u64);

SEC("xdp_drop_count_ICMP")
int xdp_drop_icmp(struct xdp_md *ctx)
{
    void *data_end = (void *)(unsigned long)ctx->data_end;
    void *data = (void *)(unsigned long)ctx->data;

    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;

    // Only handle IPv4
    if (eth->h_proto != htons(ETH_P_IP))
        return XDP_PASS;

    struct iphdr *ip = (struct iphdr *)(eth + 1);

    // Ensure the IP header (with options) is within the packet
    unsigned int ihl_bytes = ip->ihl * 4;

    // Safe read of src IP
    __u32 src_ip = ip->saddr;

    // Lookup current count (icmp_count_map)
    __u64 update_count = 1;
    __u64 *current_count = bpf_map_lookup_elem(&icmp_count_map, &src_ip);
    if (current_count) {
        __u64 tmp = *current_count + 1;
        if (tmp > 21)
            tmp = 21;   // cap at 21
        update_count = tmp;
    }

    // Only process ICMP from the monitored address
    if (ip->saddr == MONITOR_IP && ip->protocol == IPPROTO_ICMP) {

        // packet_count_map increment (example increments by 100 in your code)
        __u32 map_key = 0;
        __u64 *counter_value = bpf_map_lookup_elem(&packet_count_map, &map_key);
        __u64 new_count = 1;
        if (counter_value)
            new_count = *counter_value + 100;

        // ICMP header location and check bounds
        struct icmphdr *icmp = (struct icmphdr *)((void *)ip + ihl_bytes);

        // Ensure first 8 bytes of ICMP payload are present
        __u8 *payload = (__u8 *)((void *)icmp + sizeof(*icmp));

        // Save original 8 bytes (explicit loop avoids memcpy)
        __u8 original_bytes[8];
        #pragma unroll
        for (int i = 0; i < 8; i++) {
            original_bytes[i] = payload[i];
        }

        // Split 64-bit counter into high/low 32-bit parts
        __u32 counter_high = (__u32)(new_count >> 32);
        __u32 counter_low  = (__u32)(new_count & 0xFFFFFFFFU);

        // Write new counter into payload (big-endian)
        payload[0] = (counter_high >> 24) & 0xFF;
        payload[1] = (counter_high >> 16) & 0xFF;
        payload[2] = (counter_high >> 8)  & 0xFF;
        payload[3] = (counter_high >> 0)  & 0xFF;
        payload[4] = (counter_low  >> 24) & 0xFF;
        payload[5] = (counter_low  >> 16) & 0xFF;
        payload[6] = (counter_low  >> 8)  & 0xFF;
        payload[7] = (counter_low  >> 0)  & 0xFF;

        // === ICMP checksum incremental update ===
        // Convert original bytes back to 32-bit values (big-endian to host)
        __u32 old_high = (original_bytes[0] << 24) | (original_bytes[1] << 16) |
                        (original_bytes[2] << 8)  | (original_bytes[3]);
        __u32 old_low  = (original_bytes[4] << 24) | (original_bytes[5] << 16) |
                        (original_bytes[6] << 8)  | (original_bytes[7]);
        
        // Start with current checksum
        __u32 checksum = icmp->checksum;
        
        // Remove old values from checksum (treat each 32-bit word as two 16-bit words)
        checksum += (~old_high & 0xFFFF) + (~old_high >> 16);
        checksum += (~old_low  & 0xFFFF) + (~old_low  >> 16);
        
        // Add new values to checksum
        checksum += (counter_high & 0xFFFF) + (counter_high >> 16);
        checksum += (counter_low  & 0xFFFF) + (counter_low  >> 16);
        
        // Fold carries into 16-bit result
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
        
        // Update ICMP checksum
        icmp->checksum = (__u16)checksum;

        // Update counter in map
        bpf_map_update_elem(&packet_count_map, &map_key, &new_count, BPF_ANY);

        return XDP_PASS;
    }

    // Update the icmp_count_map with the new source count
    bpf_map_update_elem(&icmp_count_map, &src_ip, &update_count, BPF_ANY);

    // Drop when above threshold
    if (update_count >= 21)
        return XDP_DROP;

    return XDP_PASS;
}

char _license[] SEC("license") = "MIT";
