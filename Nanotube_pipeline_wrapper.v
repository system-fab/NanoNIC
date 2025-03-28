//Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2022.1.2 (lin64)
//Date        :
//Host        : running 64-bit Ubuntu 20.04.6 LTS
//Command     : generate_target Nanotube_pipeline_wrapper.bd
//Design      : Nanotube_pipeline_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module Nanotube_pipeline_wrapper
   (ap_clk_0,
    ap_rst_n_0,
    port0_0_tdata,
    port0_0_tkeep,
    port0_0_tlast,
    port0_0_tready,
    port0_0_tuser,
    port0_0_tvalid,
    port1_0_tdata,
    port1_0_tkeep,
    port1_0_tlast,
    port1_0_tready,
    port1_0_tuser,
    port1_0_tvalid);
  input ap_clk_0;
  input ap_rst_n_0;
  input [511:0]port0_0_tdata;
  input [63:0]port0_0_tkeep;
  input [0:0]port0_0_tlast;
  output port0_0_tready;
  input [47:0]port0_0_tuser;
  input port0_0_tvalid;
  output [511:0]port1_0_tdata;
  output [63:0]port1_0_tkeep;
  output [0:0]port1_0_tlast;
  input port1_0_tready;
  output [47:0]port1_0_tuser;
  output port1_0_tvalid;

  wire ap_clk_0;
  wire ap_rst_n_0;
  wire [511:0]port0_0_tdata;
  wire [63:0]port0_0_tkeep;
  wire [0:0]port0_0_tlast;
  wire port0_0_tready;
  wire [63:0]port0_0_tuser;
  wire port0_0_tvalid;
  wire [511:0]port1_0_tdata;
  wire [63:0]port1_0_tkeep;
  wire [0:0]port1_0_tlast;
  wire port1_0_tready;
  wire [63:0]port1_0_tuser;
  wire port1_0_tvalid;
  
  wire [63:0] port0_0_tstrb = 64'hFFFFFFFFFFFFFFFF;
  wire [63:0] port1_0_tstrb = 64'hFFFFFFFFFFFFFFFF;

  Nanotube_pipeline Nanotube_pipeline_i
       (.ap_clk_0(ap_clk_0),
        .ap_rst_n_0(ap_rst_n_0),
        .port0_0_tdata(port0_0_tdata),
        .port0_0_tkeep(port0_0_tkeep),
        .port0_0_tlast(port0_0_tlast),
        .port0_0_tready(port0_0_tready),
        .port0_0_tstrb(port0_0_tstrb),
        .port0_0_tuser(port0_0_tuser),
        .port0_0_tvalid(port0_0_tvalid),
        .port1_0_tdata(port1_0_tdata),
        .port1_0_tkeep(port1_0_tkeep),
        .port1_0_tlast(port1_0_tlast),
        .port1_0_tready(port1_0_tready),
        .port1_0_tstrb(port1_0_tstrb),
        .port1_0_tuser(port1_0_tuser),
        .port1_0_tvalid(port1_0_tvalid));
endmodule