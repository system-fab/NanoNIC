#!/bin/bash
# Execute Nanotube and LLVM steps to move code from an EBPF interface through
# the full Nanotube compiler flow.  For now, this is executable documentation,
# and should over time be merged into the respective tools.
#
###########################################################################
# Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
###########################################################################

set -eu

NT_BUILD_DIR=../../build
NT_OPT=${NT_BUILD_DIR}/nanotube_opt
NT_BE=${NT_BUILD_DIR}/nanotube_back_end
LOCATE_TOOL=../../scripts/locate_tool
APPLICATION=./xdp_drop_count_ICMP_nanotube.c
KATRAN=../../external/katran

expect() {
  THING=$1
  BASE=$(basename $THING)
  CODE=$2
  if [[ -z $THING || ! -e $THING ]]; then
    echo "Expecting $BASE in $THING but not finding it.  Please edit "\
         "the script or put $BASE in the right place."
    exit $CODE
  fi
}

expect $NT_OPT 2
expect $LOCATE_TOOL 3
expect $APPLICATION 4
expect $KATRAN 5

LLVM_LINK="${LOCATE_TOOL} --run llvm-link"
CLANG="${LOCATE_TOOL} --run clang"
INFILE="xdp_application.O3.bc"
NAME_BUS="open_nic"

# Build Katran packet kernel
$CLANG  -O2 -I $KATRAN/katran/lib/linux_includes \
            -I $KATRAN/katran/lib/bpf \
            -Wno-unused-value -Wno-pointer-sign \
            -Wno-compare-distinct-pointer-types \
            -fno-vectorize -fno-slp-vectorize \
            -D NANOTUBE_SIMPLE \
            $APPLICATION \
            -c -emit-llvm \
            -o $INFILE

# Front-end: Translate from EBPF to the high-level Nanotube interface
FEFILE=${INFILE/.bc/.nt.bc}
echo "$NT_OPT -ebpf2nanotube $INFILE -o $FEFILE -bus=$NAME_BUS"
$NT_OPT -ebpf2nanotube $INFILE -o $FEFILE -bus=$NAME_BUS

F=$FEFILE
NEWF=${F/.bc/.req.bc}
echo "$NT_OPT -mem2req $F -o $NEWF -bus=$NAME_BUS"
$NT_OPT -mem2req $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.lower.bc}
echo "$LLVM_LINK -o $NEWF $F ${NT_BUILD_DIR}/libnt/nanotube_high_level.bc"
$LLVM_LINK -o $NEWF $F ${NT_BUILD_DIR}/libnt/nanotube_high_level.bc

F=$NEWF
NEWF=${F/.bc/.inline.bc}
echo "$NT_OPT -always-inline -constprop $F -o $NEWF -bus=$NAME_BUS"
$NT_OPT -always-inline -constprop $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.platform.bc}
echo "$NT_OPT -platform -always-inline -constprop $F -o $NEWF -bus=$NAME_BUS"
$NT_OPT -platform -always-inline -constprop $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.optreq.bc}
echo "$NT_OPT -nt-attributes -O2 -optreq -enable-loop-unroll -always-inline -constprop -loop-unroll -simplifycfg $F -o $NEWF -bus=$NAME_BUS"
$NT_OPT -nt-attributes -O2 -optreq -enable-loop-unroll -always-inline -constprop -loop-unroll -simplifycfg $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.converge.bc}
echo "$NT_OPT -compact-geps -converge_mapa $F -o $NEWF -bus=$NAME_BUS"
$NT_OPT -compact-geps -converge_mapa $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.pipeline.bc}
echo "$NT_OPT -compact-geps -basicaa -tbaa -nanotube-aa -pipeline $F -o $NEWF -bus=$NAME_BUS"
$NT_OPT -compact-geps -basicaa -tbaa -nanotube-aa -pipeline $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.link_taps.bc}
echo "$LLVM_LINK -o $NEWF $F ${NT_BUILD_DIR}/libnt/nanotube_low_level.bc"
$LLVM_LINK -o $NEWF $F ${NT_BUILD_DIR}/libnt/nanotube_low_level.bc

F=$NEWF
NEWF=${F/.bc/.inline_opt.bc}
echo "$NT_OPT $F -o $NEWF -always-inline -rewrite-setup -replace-malloc -thread-const -constprop -enable-loop-unroll -loop-unroll -move-alloca -simplifycfg -instcombine -thread-const -constprop -simplifycfg -bus=$NAME_BUS"
$NT_OPT $F -o $NEWF -always-inline -rewrite-setup -replace-malloc -thread-const -constprop -enable-loop-unroll -loop-unroll -move-alloca -simplifycfg -instcombine -thread-const -constprop -simplifycfg -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.hls}
echo "$NT_BE $F -o $NEWF -bus=$NAME_BUS --overwrite"
$NT_BE $F -o $NEWF -bus=$NAME_BUS --overwrite
