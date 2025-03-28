# Custom Applications for Nanotube

This directory contains custom applications to use with the Nanotube project, enabling the creation of custom pipelines.

Each application folder is structured as follows:

```
.
├── name_of_the_application
│   ├── application.c
│   └── compile.sh
```

More support files can be present, depending on the application requirements.

### File Descriptions

- **`application.c`**: Contains the source code for the application.
- **`compile.sh`**: Contains the commands to compile the application. You can also specify the name of the bus that the application will use.

### Supported Buses

The following buses are currently supported:

- **`sb`**: Simple Bus
- **`shb`**: SoftHub Bus
- **`x3rx`**: X3RX Bus (X3522 SmartNICs)
- **`open_nic`**: OpenNIC Bus

### Compilation Instructions

To compile an application:

1. Copy the entire `Custom_applications` folder into the Nanotube project directory.
2. Open a terminal and navigate to the folder of the application you want to compile.
3. Run the following command:

   ```bash
   ./compile.sh
   ```

4. Some applications may require the Katran library. Follow the instructions in the README file present in the Nanotube repository to download and patch Katran prior to compiling the application.

### Notes

- Ensure the **bus name** and **application name** are correctly specified in the `compile.sh` file.
- If the compilation is successful, a folder ending with `_hls` will be created inside the application folder. This folder contains the Vivado IPs cc codes required to create a custom pipeline with HLS.

### How to execute the XDP application locally on your machine

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
