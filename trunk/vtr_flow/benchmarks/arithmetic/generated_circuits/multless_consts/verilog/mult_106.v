/*------------------------------------------------------------------------------
 * This code was generated by Spiral Multiplier Block Generator, www.spiral.net
 * Copyright (c) 2006, Carnegie Mellon University
 * All rights reserved.
 * The code is distributed under a BSD style license
 * (see http://www.opensource.org/licenses/bsd-license.php)
 *------------------------------------------------------------------------------ */
/* ./multBlockGen.pl 8862 -fractionalBits 0*/
module multiplier_block (
    i_data0,
    o_data0
);

  // Port mode declarations:
  input   [31:0] i_data0;
  output  [31:0]
    o_data0;

  //Multipliers:

  wire [31:0]
    w1,
    w256,
    w257,
    w4112,
    w4369,
    w2,
    w4367,
    w64,
    w4431,
    w8862;

  assign w1 = i_data0;
  assign w2 = w1 << 1;
  assign w256 = w1 << 8;
  assign w257 = w1 + w256;
  assign w4112 = w257 << 4;
  assign w4367 = w4369 - w2;
  assign w4369 = w257 + w4112;
  assign w4431 = w4367 + w64;
  assign w64 = w1 << 6;
  assign w8862 = w4431 << 1;

  assign o_data0 = w8862;

  //multiplier_block area estimate = 7095.2112472853;
endmodule //multiplier_block

module surround_with_regs(
	i_data0,
	o_data0,
	clk
);

	// Port mode declarations:
	input   [31:0] i_data0;
	output  [31:0] o_data0;
	reg  [31:0] o_data0;
	input clk;

	reg [31:0] i_data0_reg;
	wire [30:0] o_data0_from_mult;

	always @(posedge clk) begin
		i_data0_reg <= i_data0;
		o_data0 <= o_data0_from_mult;
	end

	multiplier_block mult_blk(
		.i_data0(i_data0_reg),
		.o_data0(o_data0_from_mult)
	);

endmodule
