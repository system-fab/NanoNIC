#!/bin/bash

CUSTOM_APPS_DIR=Custom_applications
HLS_BUILD_DIR=HLS_build
JOBS=6
LIB_PATHS=(/usr/lib/x86_64-linux-gnu /usr/lib/gcc/x86_64-linux-gnu/9)

# Find all directories matching the pattern inside Custom_applications
find "$CUSTOM_APPS_DIR" -type d -name "*.hls" | while read -r hls_dir; do
    # Extract the base directory name
    base_name=$(basename "$hls_dir")
    parent_dir=$(dirname "$hls_dir")
    app_name=$(basename "$parent_dir")
    
    # Define corresponding output directory
    output_dir="$HLS_BUILD_DIR/$app_name"
    
    # Construct the command
    cmd="scripts/hls_build -j$JOBS --clock 4.0,0.0 -p xcu250-figd2104-2L-e $hls_dir/ $output_dir/"
    
    # Append library paths
    for lib in "${LIB_PATHS[@]}"; do
        cmd+=" -L$lib"
    done
    
    cmd+=" -v"
    
    # Print the command for debugging
    echo "$cmd"
    
    # Execute the command
    eval "$cmd"
done
