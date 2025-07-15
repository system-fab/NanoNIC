The following steps show how to compile the C code and attach it to a network interface. If you're running Ubuntu >= 22.04, execute:

```bash
clang -target bpf -O2 -g -c xdp_drop_count_ICMP_original.c -o xdp_drop_count_ICMP_original.o

llvm-strip -g xdp_drop_count_ICMP_original.o

sudo ip link set dev <interface> xdp obj xdp_drop_count_ICMP_original.o sec xdp
```

If you're running Ubuntu < 22.04, you may need to install the `clang-18` and `bpftool` packages first, then execute:

```bash
clang-18 -target bpf -O2 -g -c xdp_drop_count_ICMP_original.c -o xdp_drop_count_ICMP_original.o

llvm-strip-18 -g xdp_drop_count_ICMP_original.o

sudo bpftool prog load xdp_drop_count_ICMP_original.o /sys/fs/bpf/xdp_drop_count_ICMP_original type xdp

sudo bpftool net attach xdp pinned /sys/fs/bpf/xdp_drop_count_ICMP_original dev <interface>
```

Be aware that using wlo1 maybe interrupt the internet connection to your machine. If that happens, you can remove the XDP program by executing this command for Ubuntu >= 22.04:

```bash
sudo ip link set dev wlo1 xdp off
```

Or for Ubuntu < 22.04:

```bash
sudo bpftool net detach xdp dev wlo1
```
