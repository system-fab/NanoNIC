# Custom Applications for Nanotube

This directory contains custom applications to use with the Nanotube project, enabling the creation of custom pipelines.

Each application folder is structured as follows:

```
.
├── name_of_the_application/
│   ├── pcap_test_files/
|   |   ├── pcap.IN
│   |   └── pcap.OUT
│   ├── Vivado_testbench/
│   |   └── pipeline_tb.v
│   ├── xdp_application.c
│   ├── application.c
│   └── compile.sh
```

More support files can be present, depending on the application requirements.

### File Descriptions

- **`application.c`**: Contains the source code for the application.
- **`compile.sh`**: Contains the commands to compile the application. You can also specify the name of the bus that the application will use.
- **`pcap_test_files/pcap.IN`**: Contains the input packets for the eBPF application compiled with Nanotube.
- **`pcap_test_files/pcap.OUT`**: Contains the expected output packets for the eBPF application compiled with Nanotube.
- **`Vivado_testbench/pipeline_tb.v`**: Contains the testbench for the application, used to verify the functionality of the pipeline in Vivado.

### Supported Buses

The following buses are currently supported:

- **`sb`**: Simple Bus
- **`shb`**: SoftHub Bus
- **`x3rx`**: X3RX Bus (X3522 SmartNICs)
- **`open_nic`**: OpenNIC Bus

### Compilation & Simulation Instructions

To compile an application:

1. Copy the entire `Custom_applications` folder into the Nanotube project directory.
2. Open a terminal and navigate to the folder of the application you want to compile.
3. Run the following command:

   ```bash
   ./compile.sh
   ```

4. Some applications may require the Katran library. Follow the instructions in the README file present in the Nanotube repository to download and patch Katran prior to compiling the application.
5. If the compilation is successful, a folder ending with `_hls` will be created inside the application folder. This folder contains the C++ stage files required to create a custom pipeline with HLS.

To perform HLS synthesis, you can use the `scripts/hls_build` command provided by Nanotube. The command should look like this:

```bash
scripts/hls_build -j6 --clock 4.0 -- Custom_applications/xdp_drop_IPv4/xdp_drop_IPv4.ebpf2nt.mem2req.lower.inline.platform.ntattr.optreq.converge.pipeline.link_taps.inline_opt.hls/  HLS_build/xdp_drop_IPv4/ --pcap-in /Custom_applications/xdp_drop_IPv4/pcap_test_files/test_xdp_drop_IPv4.pcap.IN --pcap-exp Custom_applications/xdp_drop_IPv4/pcap_test_files/test_xdp_drop_IPv4.pcap.OUT
```

This command performs CSim and CoSim using the provided pcap files and compare the output with the expected output. If the comparison is successful, it will generate the Vivado IPs in the `HLS_build` directory. You can also specify the target board and other options as needed.

### Notes

- Ensure the **bus name** and **application name** are correctly specified in the `compile.sh` file.
- If you encounter any issues during the synthesis and simulation phase, check the stage log files inside the output directory to better understand the issue. Keep in mind that you can also modify the c++ initial files to print some debugging information inside the log files.

### How to execute the XDP application in Software

Before deploying the application on the FPGA, you can test it locally on your machine. To do so, follow these steps:

1. Install the required dependencies:

   ```bash
   sudo apt install -y clang llvm libbpf-dev libelf-dev iproute2
   sudo apt install -y linux-headers-$(uname -r)
   ```

2. Compile the application:

   ```bash
   clang -target bpf -O3 -c xdp_application.c -o xdp_application.o
   ```

3. Load the application using the `ip` command:

   ```bash
   sudo ip link set dev <interface> xdp obj xdp_application.o sec <xdp>
   ```

   Note: Replace `<interface>` with the name of the network interface you want to attach the XDP program to and `<xdp>` with the section name specified in the application.

4. To check if the application is loaded, run:

   ```bash
   sudo ip link show <interface>
   ```

5. Now, create a packet generator to send packets to the interface and observe the behavior of the application. Sometimes a simple ping command can be enough to test the application.

6. To unload the application, run:

   ```bash
   sudo ip link set dev <interface> xdp off
   ```

### How to simulate the pipeline in Vivado

To simulate the pipeline in Vivado, follow these steps:

1. Open Vivado and open the OpenNIC-shell project.
2. Inside the project, add the testbench from the `Vivado_testbench` folder of the application you want to simulate.
3. Make sure to set the correct simulation top module in the Vivado settings and to correctly target your pipeline.
4. Run the Behavioral Simulation.

The expected output should match the packets in the `pcap_test_files/pcap.OUT` file. If there are any discrepancies, check the testbench and the application code for potential issues. This is an example of the expected output:

![Behavioral Simulation](../docs/Working_testbench.png)
