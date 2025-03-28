#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>

#define SEC(NAME) __attribute__((section(NAME), used))

SEC("xdp_drop_IPv4")
int xdp_prog(struct xdp_md *ctx)
{
    //uint64_t start = bpf_ktime_get_ns();

    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;

    if (data + sizeof(*eth) > data_end)
        return XDP_DROP;

    if (eth->h_proto == htons(ETH_P_IP))
        return XDP_DROP;

    //uint64_t end = bpf_ktime_get_ns();
    //uint64_t diff = end - start;

    //bpf_printk("Latency: %llu ns\n", diff);

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
