export PATH=/home/simo/Desktop/Tesi/nanotube-llvm/build/bin:$PATH
export LLVM_CONFIG=/home/simo/Desktop/Tesi/nanotube-llvm/build/bin/llvm-config
export LD_LIBRARY_PATH=/home/simo/Desktop/Tesi/nanotube-llvm/build/lib:$LD_LIBRARY_PATH

name_file=xdp_swap_mac

name_bus=open_nic

clang -o $name_file.bc -c -emit-llvm -Wall -Werror -O2 -g -Wno-comment -Wno-gcc-compat -Iinclude $name_file.c -fno-vectorize -fno-slp-vectorize

../../build/nanotube_opt -o $name_file.ebpf2nt.bc $name_file.bc -ebpf2nanotube -bus=$name_bus

../../build/nanotube_opt -o $name_file.ebpf2nt.mem2req.bc $name_file.ebpf2nt.bc -mem2req -bus=$name_bus

llvm-link -o $name_file.ebpf2nt.mem2req.lower.bc $name_file.ebpf2nt.mem2req.bc ../../build/libnt/nanotube_high_level.bc

../../build/nanotube_opt -o $name_file.ebpf2nt.mem2req.lower.inline.bc $name_file.ebpf2nt.mem2req.lower.bc -always-inline -constprop -bus=$name_bus

../../build/nanotube_opt -o $name_file.ebpf2nt.mem2req.lower.inline.platform.bc $name_file.ebpf2nt.mem2req.lower.inline.bc -platform -always-inline -constprop -bus=$name_bus

../../build/nanotube_opt -o $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.bc $name_file.ebpf2nt.mem2req.lower.inline.platform.bc -nt-attributes -O2 -bus=$name_bus

../../build/nanotube_opt -o $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.bc $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.bc -optreq -enable-loop-unroll -always-inline -constprop -loop-unroll -simplifycfg -bus=$name_bus

../../build/nanotube_opt -o $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.bc $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.bc -compact-geps -converge_mapa -bus=$name_bus

../../build/nanotube_opt -o $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.pipeline.bc $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.bc -compact-geps -basicaa -tbaa -nanotube-aa -pipeline -bus=$name_bus

llvm-link -o $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.pipeline.link_taps.bc $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.pipeline.bc ../../build/libnt/nanotube_low_level.bc

../../build/nanotube_opt -o $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.pipeline.link_taps.inline_opt.bc $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.pipeline.link_taps.bc -always-inline -rewrite-setup -replace-malloc -thread-const -constprop -codegenprepare -constprop -dce -enable-loop-unroll -loop-unroll -move-alloca -simplifycfg -instcombine -thread-const -constprop -simplifycfg -bus=$name_bus

../../build/nanotube_back_end --overwrite $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.pipeline.link_taps.inline_opt.bc -o $name_file.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.pipeline.link_taps.inline_opt.hls