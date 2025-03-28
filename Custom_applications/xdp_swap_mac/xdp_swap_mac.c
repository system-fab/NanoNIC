#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <arpa/inet.h>

#define SEC(NAME) __attribute__((section(NAME), used))

SEC("xdp_swap_mac")
int xdp_prog(struct xdp_md *ctx)
{
    //uint64_t start = bpf_ktime_get_ns();
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;

    // Ensure the packet is large enough to contain an Ethernet header
    if ((void *)(eth + 1) > data_end)
        return XDP_DROP;

    // Swap MAC addresses
    __u8 tmp_mac[ETH_ALEN];

// 1- Copy source MAC to temporary buffer
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        tmp_mac[i] = eth->h_source[i];

// Move destination MAC into source MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_source[i] = eth->h_dest[i];

// Move original source MAC (from tmp buffer) into destination MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_dest[i] = tmp_mac[i];

// 2- Copy source MAC to temporary buffer
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        tmp_mac[i] = eth->h_source[i];

// Move destination MAC into source MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_source[i] = eth->h_dest[i];

// Move original source MAC (from tmp buffer) into destination MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_dest[i] = tmp_mac[i];

// 3- Copy source MAC to temporary buffer
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        tmp_mac[i] = eth->h_source[i];

// Move destination MAC into source MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_source[i] = eth->h_dest[i];

// Move original source MAC (from tmp buffer) into destination MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_dest[i] = tmp_mac[i];

// 4- Copy source MAC to temporary buffer
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        tmp_mac[i] = eth->h_source[i];

// Move destination MAC into source MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_source[i] = eth->h_dest[i];

// Move original source MAC (from tmp buffer) into destination MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_dest[i] = tmp_mac[i];

// 5- Copy source MAC to temporary buffer
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        tmp_mac[i] = eth->h_source[i];

// Move destination MAC into source MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_source[i] = eth->h_dest[i];

// Move original source MAC (from tmp buffer) into destination MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_dest[i] = tmp_mac[i];

// 6- Copy source MAC to temporary buffer
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        tmp_mac[i] = eth->h_source[i];

// Move destination MAC into source MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_source[i] = eth->h_dest[i];

// Move original source MAC (from tmp buffer) into destination MAC field
#pragma unroll
    for (int i = 0; i < ETH_ALEN; i++)
        eth->h_dest[i] = tmp_mac[i];

    //uint64_t end = bpf_ktime_get_ns();
    //uint64_t diff = end - start;

    //bpf_printk("Latency MAC: %llu ns\n", diff);

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
