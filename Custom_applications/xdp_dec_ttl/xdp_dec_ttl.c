#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <arpa/inet.h>

#define SEC(NAME) __attribute__((section(NAME), used))

static __always_inline __u16 csum16_add(__u16 csum, __u16 add)
{
    csum += add;
    return csum + (csum < add);
}

SEC("xdp_dec_ttl")
int xdp_prog(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;

    if (data + sizeof(*eth) > data_end)
        return XDP_DROP;

    // Check if it's an IPv4 packet
    if (eth->h_proto == htons(ETH_P_IP))
    {
        struct iphdr *ip = (struct iphdr *)(eth + 1);

        // Ensure the packet is large enough to contain an IPv4 header
        if ((void *)(ip + 1) > data_end)
            return XDP_DROP;

        // Decrement TTL
        __u8 old_ttl = ip->ttl;
        __u8 new_ttl = old_ttl - 1;

        if (new_ttl == 0)
        {
            return XDP_DROP; // Drop if TTL expires
        }
        ip->ttl = new_ttl;

        __u16 old_check = ntohs(ip->check);

        // Calculate the difference in the 16-bit word containing TTL and Protocol
        __u16 old_word = (old_ttl << 8) | ip->protocol; // TTL is high byte, Protocol is low byte
        __u16 new_word = (new_ttl << 8) | ip->protocol; // TTL is high byte, Protocol is low byte

        // Update checksum: add the difference between old_word and new_word
        __u16 diff = old_word - new_word;
        __u16 new_check = csum16_add(old_check, diff);
        ip->check = htons(new_check);

        return XDP_PASS;
    }

    // Check if it's an IPv6 packet
    if (eth->h_proto == htons(ETH_P_IPV6))
    {
        struct ipv6hdr *ip6 = (struct ipv6hdr *)(eth + 1);

        // Ensure the packet is large enough to contain an IPv6 header
        if ((void *)(ip6 + 1) > data_end)
            return XDP_DROP;

        // Decrement Hop Limit (no checksum update needed)
        ip6->hop_limit -= 1;

        return XDP_PASS;
    }

    return XDP_DROP;
}

char _license[] SEC("license") = "GPL";
