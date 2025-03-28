#include <linux/bpf.h>
#include <linux/in.h>

#define SEC(NAME) __attribute__((section(NAME), used))

SEC("xdp_pass_all")
int xdp_pass(struct xdp_md *ctx)
{
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
