#!/usr/bin/env python3
import sys
import os
import re

def format_stage_name(name):
    """
    Convert stage name to proper Vivado BD format.
    Example: stage_1.port0 -> stage_1_0/port0
    """
    # Pattern to match stage_X.portY
    pattern = r'(stage_\d+)\.(.+)'
    match = re.match(pattern, name)
    
    if match:
        stage_name = match.group(1)  # e.g., "stage_1"
        port_name = match.group(2)   # e.g., "port0"
        return f"{stage_name}_0/{port_name}"
    
    # If no pattern match, return as is
    return name

def classify_connection(src, dst):
    """
    Classify the type of connection for better organization.
    Returns: (category, priority) where priority determines order
    """
    # Main pipeline chain (port1 -> port0)
    if "/port1" in src and "/port0" in dst:
        return ("main_pipeline", 1)
    
    # Secondary sideband chain (port2 -> port3)
    elif "/port2" in src and "/port3" in dst:
        return ("sideband_chain", 2)
    
    # Feedback/loopback connections with stage_0
    elif "stage_0" in src or "stage_0" in dst:
        return ("feedback_loops", 3)
    
    # Other connections
    else:
        return ("other", 4)

def process_vitis_opts_file(input_file, output_file="connections_out.tcl"):
    """
    Read vitis_opts.ini file and translate connections to TCL commands.
    
    Args:
        input_file: Path to vitis_opts.ini file
        output_file: Output TCL file path
    """
    
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found!")
        return False
    
    connections = {
        "main_pipeline": [],
        "sideband_chain": [],
        "feedback_loops": [],
        "other": []
    }
    filtered_count = 0
    
    with open(input_file, "r") as fin:
        for line_num, line in enumerate(fin, 1):
            line = line.strip()
            
            # Skip empty lines and comments
            if not line or line.startswith("#") or line.startswith("["):
                continue
            
            # Only process lines starting with sc=
            if not line.startswith("sc="):
                continue
            
            # Skip unwanted kernel connections
            if "mae2p_kernel0" in line or "p2vnr_kernel0" in line:
                filtered_count += 1
                continue
            
            # Remove "sc=" prefix
            conn = line[3:]
            
            # Split into parts (src, dst, optional width)
            parts = conn.split(":")
            if len(parts) < 2:
                print(f"Warning: Invalid connection format at line {line_num}: {line}")
                continue
            
            src = parts[0]
            dst = parts[1]
            
            # Format stage names
            src = format_stage_name(src)
            dst = format_stage_name(dst)
            
            # Classify and store connection
            category, _ = classify_connection(src, dst)
            
            connections[category].append((src, dst))
    
    # Write organized output
    with open(output_file, "w") as fout:
        # Write header comment
        fout.write("# Auto-generated TCL connections from vitis_opts.ini\n")
        fout.write(f"# Source file: {input_file}\n")
        fout.write(f"# Filtered out {filtered_count} kernel connections (mae2p_kernel0, p2vnr_kernel0)\n\n")
        
        # Write main pipeline connections
        if connections["main_pipeline"]:
            fout.write("# Main pipeline chain (port1 -> port0)\n")
            for src, dst in connections["main_pipeline"]:
                fout.write(f"connect_bd_intf_net [get_bd_intf_pins {src}] [get_bd_intf_pins {dst}]\n")
            fout.write("\n")
        
        # Write sideband connections
        if connections["sideband_chain"]:
            fout.write("# Secondary sideband chain (port2 -> port3)\n")
            for src, dst in connections["sideband_chain"]:
                fout.write(f"connect_bd_intf_net [get_bd_intf_pins {src}] [get_bd_intf_pins {dst}]\n")
            fout.write("\n")
        
        # Write feedback/loopback connections
        if connections["feedback_loops"]:
            fout.write("# Feedback / loopback connections with stage_0\n")
            for src, dst in connections["feedback_loops"]:
                fout.write(f"connect_bd_intf_net [get_bd_intf_pins {src}] [get_bd_intf_pins {dst}]\n")
            fout.write("\n")
        
        # Write other connections
        if connections["other"]:
            fout.write("# Other connections\n")
            for src, dst in connections["other"]:
                fout.write(f"connect_bd_intf_net [get_bd_intf_pins {src}] [get_bd_intf_pins {dst}]\n")
            fout.write("\n")
    
    total_connections = sum(len(conn_list) for conn_list in connections.values())
    
    print(f"Translation completed successfully!")
    print(f"- Input file: {input_file}")
    print(f"- Output file: {output_file}")
    print(f"- Connections processed: {total_connections}")
    print(f"- Connections filtered: {filtered_count}")
    print(f"- Main pipeline: {len(connections['main_pipeline'])}")
    print(f"- Sideband chain: {len(connections['sideband_chain'])}")
    print(f"- Feedback loops: {len(connections['feedback_loops'])}")
    print(f"- Other: {len(connections['other'])}")
    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 get_connections.py <vitis_opts.ini_file> [output_file]")
        print("\nExample:")
        print("  python3 get_connections.py testing/hls_out_tests/golden/simple/vitis_opts.ini")
        print("  python3 get_connections.py my_project/vitis_opts.ini custom_connections.tcl")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else "connections_out.tcl"
    
    success = process_vitis_opts_file(input_file, output_file)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
