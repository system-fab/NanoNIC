#include <linux/bpf.h>
#include <linux/in.h>

#define SEC(NAME) __attribute__((section(NAME), used))

SEC("xdp_drop_all")
int filter(struct xdp_md *ctx)
{
    return XDP_DROP;
}

char _license[] SEC("license") = "GPL";
