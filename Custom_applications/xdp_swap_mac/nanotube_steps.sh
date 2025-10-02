#!/bin/bash
#
###########################################################################
# Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
# Modifications (C) 2025 Simone Mannarino
# Licensed under the MIT License.
###########################################################################

set -eu

NT_BUILD_DIR=../../build
NT_OPT=${NT_BUILD_DIR}/nanotube_opt
NT_BE=${NT_BUILD_DIR}/nanotube_back_end
LOCATE_TOOL=../../scripts/locate_tool
APPLICATION=./xdp_swap_mac.c
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
            -fno-builtin-bswap64 \
            -D NANOTUBE_SIMPLE \
            $APPLICATION \
            -c -emit-llvm \
            -o $INFILE

# Front-end: Translate from EBPF to the high-level Nanotube interface
FEFILE=${INFILE/.bc/.nt.bc}
echo "Front-end: Converting EBPF to Nanotube; $INFILE to $FEFILE -bus=$NAME_BUS"
$NT_OPT -codegenprepare -instsimplify -dce -ebpf2nanotube $INFILE -o $FEFILE -bus=$NAME_BUS

F=$FEFILE
NEWF=${F/.bc/.req.bc}
echo "Back-end: Converting loads / stores to Nanotube requests; $F to $NEWF -bus=$NAME_BUS"
$NT_OPT -instsimplify -mem2req $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.lower.bc}
echo "back-end: Lowering the Nanotube interface closer to hardware; "\
     "$F to $NEWF"
$LLVM_LINK -o $NEWF $F \
           ${NT_BUILD_DIR}/libnt/nanotube_high_level.bc

F=$NEWF
NEWF=${F/.bc/.inline.bc}
echo "Front-end: Inlining adapter functions; $F to $NEWF -bus=$NAME_BUS"
$NT_OPT -always-inline -instsimplify $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.platform.bc}
echo "Front-end: Performing platform conversion; $F to $NEWF -bus=$NAME_BUS"
$NT_OPT -platform -always-inline -instsimplify $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.optreq.bc}
echo "back-end: Optimising packet accesses; $F to $NEWF -bus=$NAME_BUS"
$NT_OPT -mergereturn -optreq -enable-loop-unroll -always-inline -instsimplify -loop-unroll -simplifycfg $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.converge.bc}
echo "back-end: Converging the control flow; $F to $NEWF -bus=$NAME_BUS"
$NT_OPT -move-alloca -compact-geps -converge_mapa $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.pipeline.bc}
echo "back-end: Splitting code into pipeline stages; $F to $NEWF -bus=$NAME_BUS"
$NT_OPT -compact-geps -basic-aa -tbaa -nanotube-aa -pipeline $F -o $NEWF -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.link_taps.bc}
echo "back-end: Link the map/packet taps; $F to $NEWF"
$LLVM_LINK -o $NEWF $F \
           ${NT_BUILD_DIR}/libnt/nanotube_low_level.bc

F=$NEWF
NEWF=${F/.bc/.inline_opt.bc}
echo "back-end: Inline the map/packet taps; $F to $NEWF -bus=$NAME_BUS"
$NT_OPT $F -o $NEWF \
         -always-inline \
         -rewrite-setup -replace-malloc \
         -thread-const -instsimplify \
         -enable-loop-unroll -loop-unroll \
         -move-alloca -simplifycfg -instcombine \
         -thread-const -instsimplify -simplifycfg -bus=$NAME_BUS

F=$NEWF
NEWF=${F/.bc/.hls}
echo "back-end: Writing out HLS C; $F to $NEWF -bus=$NAME_BUS"
$NT_BE $F -o $NEWF --overwrite -bus=$NAME_BUS
