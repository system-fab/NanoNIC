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

// Specific IP to monitor (in network byte order)
#define MONITOR_IP 0x6401A8C0  // 192.168.1.100

// Packet counter map
struct bpf_map_def SEC("maps") packet_count_map = {
    .type = BPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(__u32),
    .value_size = sizeof(__u64),
    .max_entries = 1,
};
BPF_ANNOTATE_KV_PAIR(packet_count_map, __u32, __u64);

SEC("xdp")
int xdp_pass_all_counter(struct xdp_md *ctx)
{
    void *data_end = (void *)(unsigned long)ctx->data_end;
    void *data = (void *)(unsigned long)ctx->data;
    struct ethhdr *eth = data;
    struct iphdr *ip;
    struct icmphdr *icmp;
    __u32 map_key = 0;
    __u64 *counter_value;
    __u64 new_count = 1;

    ip = (struct iphdr *)(eth + 1);
    
    // Only process ICMP packets from monitored IP
    if (ip->saddr == MONITOR_IP && ip->protocol == IPPROTO_ICMP) {

        // Get current counter value and increment
        counter_value = bpf_map_lookup_elem(&packet_count_map, &map_key);
        if (counter_value) {
            new_count = *counter_value + 100;
        }
    
        // Get ICMP header (handle variable IP header length)
        icmp = (struct icmphdr *)((void *)ip + (ip->ihl * 4));
        
        // Get pointer to first 8 bytes of ICMP payload
        __u8 *payload = (__u8 *)((void *)icmp + sizeof(*icmp));
        
        // Save original 8 bytes for checksum calculation
        __u8 original_bytes[8];
        #pragma unroll
        for (int i = 0; i < 8; i++) {
            original_bytes[i] = payload[i];
        }
        
        // Split 64-bit counter into high and low 32-bit parts
        __u32 counter_high = (__u32)(new_count >> 32);
        __u32 counter_low  = (__u32)(new_count & 0xFFFFFFFF);
        
        // Write counter in big-endian format (most significant byte first)
        // Bytes 0-3: high 32 bits
        payload[0] = (counter_high >> 24) & 0xFF;  // Most significant byte
        payload[1] = (counter_high >> 16) & 0xFF;
        payload[2] = (counter_high >> 8)  & 0xFF;
        payload[3] = (counter_high >> 0)  & 0xFF;
        
        // Bytes 4-7: low 32 bits
        payload[4] = (counter_low >> 24) & 0xFF;
        payload[5] = (counter_low >> 16) & 0xFF;
        payload[6] = (counter_low >> 8)  & 0xFF;
        payload[7] = (counter_low >> 0)  & 0xFF;   // Least significant byte
        
        // === ICMP Checksum Recalculation ===
        
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
    }
    
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";