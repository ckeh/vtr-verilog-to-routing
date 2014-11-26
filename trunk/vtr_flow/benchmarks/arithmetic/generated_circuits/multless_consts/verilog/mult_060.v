/*------------------------------------------------------------------------------
 * This code was generated by Spiral Multiplier Block Generator, www.spiral.net
 * Copyright (c) 2006, Carnegie Mellon University
 * All rights reserved.
 * The code is distributed under a BSD style license
 * (see http://www.opensource.org/licenses/bsd-license.php)
 *------------------------------------------------------------------------------ */
/* ./multBlockGen.pl 11405 -fractionalBits 0*/
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
    w4096,
    w4095,
    w256,
    w3839,
    w15356,
    w11517,
    w16,
    w11533,
    w128,
    w11405;

  assign w1 = i_data0;
  assign w11405 = w11533 - w128;
  assign w11517 = w15356 - w3839;
  assign w11533 = w11517 + w16;
  assign w128 = w1 << 7;
  assign w15356 = w3839 << 2;
  assign w16 = w1 << 4;
  assign w256 = w1 << 8;
  assign w3839 = w4095 - w256;
  assign w4095 = w4096 - w1;
  assign w4096 = w1 << 12;

  assign o_data0 = w11405;

  //multiplier_block area estimate = 9024.52305565;
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
