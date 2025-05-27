# NanoNIC

## Project Goal

The **NanoNIC** project integrates **Nanotube**, a compiler for translating eBPF programs into FPGA-friendly hardware descriptions, with **OpenNIC**, an open-source framework for programmable network interface cards. The goal is to enable hardware-accelerated, high-performance eBPF-based packet processing.

This project provides a proof-of-concept system demonstrating how to:

1. Translate eBPF programs into FPGA bitstreams using **Nanotube** and **High-Level Synthesis (HLS)**.
2. Utilize **OpenNIC** to interface with network traffic and manage FPGA-host communication.
3. Compare the performance of FPGA-accelerated eBPF programs with general-purpose CPU execution.

## Project Description

### Components

This project includes the following components:

1. **Bus Implementation:**

   - Nanotube doesn't official support the OpenNIC bus interface so we added it to the official repository.

2. **Wrapper Implementation:**

   - The wrapper connects **OpenNIC** and **Nanotube**-generated IPs.

3. **eBPF Programs:**
   - A set of eBPF programs for testing the system. Inside the Custom_applications folder you can find the eBPF programs that we used for testing. Plus also a script for compiling every eBPF program with Nanotube and a guide that describes how to compile, execute and test each program before deploying it on the FPGA.

### Bugs fixed

- Nanotube compiler only support a version of AXI4-Stream that comprend also the `tstrb` signal, which is not necessary for OpenNIC. The issue has been communicated to the Nanotube team and they will fix this issue ([Github Issue](https://github.com/Xilinx/nanotube/issues/2)). For now, the problem is solved by the `Nanotube_pipeline_wrapper.v` file, which handles the `tstrb` signal.
- The HLS script provided by Nanotube does not work with the xdp_drop_all application because it does not support applications that have no output. To solve this issue, follow the instruction in the [Github Issue](https://github.com/Xilinx/nanotube/issues/3).

### Prerequisites

To build and deploy the NanoNIC system, ensure the following are installed:

- **Xilinx Vivado** (v2022.1) for FPGA synthesis and implementation.
- **Ubuntu 20.04** with kernel development tools.

The building process often requires a great amount of RAM installed. If you don't have 64 GB available, make sure to dimension the swapfile in order to reach that amount. (Look [here](https://askubuntu.com/questions/178712/how-to-increase-swap-space) for a guide).

Both Nanotube and the OpenNIC shell are linked as submodules to this repository so just run this command in order to download them:

```shell
git submodule update --init --recursive
```

Now, you can apply the patch created for Nanotube repository. To do so, run the following command:

```shell
patch -d ./nanotube -p1 < nanotube.patch
```

For the OpenNIC shell, you need to execute the file named `synth_open-nic_project.sh` . This will create the necessary files for the Vivado project. Before,

```shell
./synth_open-nic_project.sh
```

The compilation is going to take a while so you can take a break and come back later. After the compilation is done, you can open the Vivado project and start working on the FPGA design.

### [Nanotube](nanotube/README.md)

Here is a simple representation of the Nanotube compiler:

![Nanotube](docs/Nanotube_chain.jpg)

Nanotube is created to translate eBPF programs into a series of C++ files that can be synthesized into hardware using High-Level Synthesis (HLS).

Once the patch is applied, you can follow the instructions in the Nanotube README to build the compiler. Then, look at the Custom_applications folder to find the eBPF programs that we used for testing. Each application has a `compile.sh` script that compiles the eBPF program into HLS C++ files.

To perform the HLS synthesis, run this command inside the Nanotube directory (N is the number of threads you want to use):

```shell
scripts/hls_build path_to_your_stages_cc_files/*.hls HLS_build/name_of_your_application -j N -L/usr/lib/x86_64-linux-gnu -L/usr/lib/gcc/x86_64-linux-gnu/9 -v
```

After the HLS synthesis is done, you can find the IPs in the HLS_build directory. You can now move the IPs to the OpenNIC shell directory and start building the FPGA design.

### [OpenNIC-shell](open-nic-shell/README.md)

The OpenNIC shell is a framework that allows the user to create a custom NIC with a custom pipeline. For more information please visit the OpenNIC repository.

In order to build the OpenNIC shell with our custom IP inside, some modifications are needed. First of all, execute the `synth_open-nic_project.sh` file and generate a Vivado project of the OpenNIC shell. Then, press Tools -> Settings -> IP -> Repository and add the path to the nanotube/HLS\*build/application directory. You should see some new IPs added to the project. After that, you can create a new block design and add all IP by pressing the "+" button and selecting all the IPs that are called stage\_{number}. Inside the .hls build directory, you will find a `vitis_opts.ini` file that contains all the necessary links that you need to reproduce inside the Block design. If a port is not connected to anything, right click on it and press "Make it external". At this point you should connect all the clock and reset ports together and the result should be a block design that looks like this:

![Block Design](docs/Nanotube_pipeline.png)

Now, you can let Vivado create the wrapper for you. This file is essential to connect the OpenNIC shell with the custom pipeline. Since the IPs generated by Nanotube include also a tstrb signal that is useless in the OpenNIC context, see how you can fix it by looking at the file named `Nanotube_pipeline_wrapper.v`. After that, you can connect the pipeline to the OpenNIC shell by changing the `p2p_250mhz.sv` file inside Vivado. Replace line 282 where the rx_ppl_inst is connected to an axi_stream_pipeline and use the custom pipeline that you created. Now, you need to set as global the file named `open_nic_shell_macros.vh`. The next step is to generate the bitstream by pressing the "Generate Bitstream" button.

## Warnings and Errors

Be careful when you try to emulate this project because a lot of things can go wrong. Here are some of the errors we found during the development:

- Sometimes in the Nanotube README file, the `nanotube-llvm` directory is missplelled. Make sure to correct it.
- There's also need to install python3-distutils package in order to make scons work.
- If you have problems with installling Vivado, here is a guide to do it (https://www.reddit.com/r/Xilinx/comments/s7lcgq/comment/ib643pc/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button)
- When you try to run the **Behavioral Simulation** in Vivado, you may encounter an error stating that files like `bd_7485_lmb_bram_0.mem` or `bd_7485_reg_map_bram_0.mem` are missing. If this happens, simply create an empty file with the same name to allow Vivado to initialize that memory region to zero. Credit: Thanks to **Francesco Maria Tranquillo** for this helpful insight.
- You may encounter an error saying "Vivado "$RDI_PROG" "$@". The problem is that you may not have enought ram to run the program. You can try to increase the swapfile size or try to run the program in a machine with more ram.
- Pay attention to how you insert the packet data inside the tdata signal in the Vivado testbench. The data should be inserted in the correct order, otherwise the simulation will fail. There is a python script called `reverse_pairs.py` that can help you with this task. It takes a packet formatted as a string as input and outputs a new string with the data reversed in pairs, which is the expected format for the tdata signal in the Vivado testbench.

## Limitations

Unfortunately, the current implementation has several limitations that you should be aware of:

1. The project is tested for the Xilinx Alveo U250 FPGA and may require modifications to support other FPGA platforms.

2. The Nanotube compiler currently supports only a subset of eBPF features (e.g. Per-cpu maps are not supported)

3. Big pipelines like the one of Katran fails to meet the timing constraints in the OpenNIC shell. As of today, we are still working on this issue.

## Roadmap

- [x] Integrate Nanotube with OpenNIC support and pass the repository tests.
- [x] Achieve a working implementation on FPGA.
- [ ] Test the hardware design in a real-world scenario.

## Acknowledgment

This project builds on the following:

- [AMD Nanotube](https://github.com/Xilinx/nanotube): A BPF-to-HDL compiler designed for hardware acceleration in networking applications.
- [OpenNIC](https://github.com/Xilinx/open-nic): A framework for building programmable Network Interface Cards (NICs), enabling advanced packet processing.
- [Xilinx Vivado](https://www.amd.com/en/products/software/adaptive-socs-and-fpgas/vivado.html): Development tools and platforms for implementing and testing FPGA-based designs.
