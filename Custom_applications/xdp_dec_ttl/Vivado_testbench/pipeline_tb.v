`timescale 1ns / 1ps

module Nanotube_pipeline_tb;

  reg ap_clk_0;
  reg ap_rst_n_0;
  reg [511:0] port0_0_tdata;
  reg [63:0] port0_0_tkeep;
  reg port0_0_tlast;
  reg [47:0] port0_0_tuser;
  reg port0_0_tvalid;
  wire port0_0_tready;
  wire [511:0] port1_0_tdata;
  wire [63:0] port1_0_tkeep;
  wire port1_0_tlast;
  reg port1_0_tready;
  wire [47:0] port1_0_tuser;
  wire port1_0_tvalid;
  
  integer start_time, end_time;
  reg [7:0] input_ttl_1, output_ttl_1;
  reg [7:0] input_ttl_2, output_ttl_2;


  // Instantiate the wrapper
  Nanotube_pipeline_wrapper uut (
    .ap_clk_0(ap_clk_0),
    .ap_rst_n_0(ap_rst_n_0),
    .port0_0_tdata(port0_0_tdata),
    .port0_0_tkeep(port0_0_tkeep),
    .port0_0_tlast(port0_0_tlast),
    .port0_0_tready(port0_0_tready),
    .port0_0_tuser(port0_0_tuser),
    .port0_0_tvalid(port0_0_tvalid),
    .port1_0_tdata(port1_0_tdata),
    .port1_0_tkeep(port1_0_tkeep),
    .port1_0_tlast(port1_0_tlast),
    .port1_0_tready(port1_0_tready),
    .port1_0_tuser(port1_0_tuser),
    .port1_0_tvalid(port1_0_tvalid)
  );
  
  // Clock generation (250 MHz = 4 ns period)
  always #2 ap_clk_0 = ~ap_clk_0;
  
  // Handshake happens between master and slave
  wire port0_handshake;
  assign port0_handshake = port0_0_tvalid & port0_0_tready;
  
  wire port1_handshake;
  assign port1_handshake = port1_0_tvalid & port1_0_tready;
  
  initial begin
      ap_clk_0 = 0;
      ap_rst_n_0 = 0;
      port0_0_tdata = 0;
      port0_0_tkeep = 0;
      port0_0_tlast = 0;
      port0_0_tuser = 0;
      port0_0_tvalid = 0;
      port1_0_tready = 1;
    
      #20;
      ap_rst_n_0 = 1;
    
      wait(port0_0_tready);
      #2;
    
      // === Packet 1 ===
      // Beat 1
      port0_0_tdata = 512'h14131211100000000000091ba000000000635114e20300526843f400080301a8c00101a8c088c501400040cbf154000045000801010000000203010000000200;
      port0_0_tkeep = 64'hFFFFFFFFFFFFFFFF;
      port0_0_tuser = 48'h000000000062;
      port0_0_tvalid = 1;
      port0_0_tlast = 0;
      input_ttl_1 = port0_0_tdata[191:184]; 
      #4;
    
      // Beat 2
      port0_0_tdata = 512'h00000000000000000000000000000000000000000000000000000000000037363534333231302f2e2d2c2b2a292827262524232221201f1e1d1c1b1a1918171615;
      port0_0_tkeep = 64'h00000007ffffffff;
      port0_0_tvalid = 1;
      port0_0_tlast = 1;
      #4;
    
      // === Packet 2 ===
      // Beat 1
      port0_0_tdata = 512'h010000207672a500800200000000000000011000000105fe3fda8005feff860002010000000705fe3f403a100000000060dd86da8005860000ea690797600000;
      port0_0_tkeep = 64'hFFFFFFFFFFFFFFFF;
      port0_0_tuser = 48'h000000000046;
      port0_0_tvalid = 1;
      port0_0_tlast = 0;
      input_ttl_2 = port0_0_tdata[183:176];
      #4;

      // Beat 2
      port0_0_tdata = 512'h000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000639d336e7c9;
      port0_0_tkeep = 64'h000000000000007F;
      port0_0_tvalid = 1;
      port0_0_tlast = 1;
      #4;
    
      // Deassert valid after all packets sent
      port0_0_tvalid = 0;
      
      wait(port1_handshake);
      output_ttl_1 = port1_0_tdata[191:184];
      
      wait(!port1_0_tvalid);
      wait(port1_0_tvalid);
      output_ttl_2 = port1_0_tdata[183:176];
      
      $display("First Packet TTL comparison: input TTL = %0d, output TTL = %0d", input_ttl_1, output_ttl_1);
      $display("Second Packet TTL comparison: input TTL = %0d, output TTL = %0d", input_ttl_2, output_ttl_2);

    
      // Finish after some cycles
      #100;
      $finish;
    end
    
endmodule