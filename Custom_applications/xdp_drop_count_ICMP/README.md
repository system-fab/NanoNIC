The following steps show how to compile the C code and attach it to a network interface. Execute:

```bash

clang -target bpf -O2 -g -c xdp_drop_count_ICMP_original.c -o xdp_drop_count_ICMP_original.o

llvm-strip -g xdp_drop_count_ICMP_original.o

sudo ip link set dev wlo1 xdp obj xdp_drop_count_ICMP_original.o sec xdp

```

Be aware that using wlo1 maybe interrupt the internet connection to your machine. If that happens, you can remove the XDP program by executing:

```bash
sudo ip link set dev wlo1 xdp off
```
