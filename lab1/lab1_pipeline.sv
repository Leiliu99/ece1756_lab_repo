module lab1 #
(
	parameter WIDTHIN = 16,		// Input format is Q2.14 (2 integer bits + 14 fractional bits = 16 bits)
	parameter WIDTHOUT = 32,	// Intermediate/Output format is Q7.25 (7 integer bits + 25 fractional bits = 32 bits)
	// Taylor coefficients for the first five terms in Q2.14 format
	parameter [WIDTHIN-1:0] A0 = 16'b01_00000000000000, // a0 = 1
	parameter [WIDTHIN-1:0] A1 = 16'b01_00000000000000, // a1 = 1
	parameter [WIDTHIN-1:0] A2 = 16'b00_10000000000000, // a2 = 1/2
	parameter [WIDTHIN-1:0] A3 = 16'b00_00101010101010, // a3 = 1/6
	parameter [WIDTHIN-1:0] A4 = 16'b00_00001010101010, // a4 = 1/24
	parameter [WIDTHIN-1:0] A5 = 16'b00_00000010001000  // a5 = 1/120
)
(
	input clk,
	input reset,	
	
	input i_valid,
	input i_ready,
	output o_valid,
	output o_ready,
	
	input [WIDTHIN-1:0] i_x,
	output [WIDTHOUT-1:0] o_y
);
//Output value could overflow (32-bit output, and 16-bit inputs multiplied
//together repeatedly).  Don't worry about that -- assume that only the bottom
//32 bits are of interest, and keep them.
logic [WIDTHIN-1:0] x;	// Register to hold input X
logic [WIDTHOUT-1:0] y_Q;	// Register to hold output Y
logic valid_Q1;		// Output of register x is valid
logic valid_Q2;		// Output of register y is valid

// signal for enabling sequential circuit elements
logic enable;

// Signals for computing the y output
logic [WIDTHOUT-1:0] m0_out; // A5 * x
logic [WIDTHOUT-1:0] a0_out; // A5 * x + A4
logic [WIDTHOUT-1:0] m1_out; // (A5 * x + A4) * x
logic [WIDTHOUT-1:0] a1_out; // (A5 * x + A4) * x + A3
logic [WIDTHOUT-1:0] m2_out; // ((A5 * x + A4) * x + A3) * x
logic [WIDTHOUT-1:0] a2_out; // ((A5 * x + A4) * x + A3) * x + A2
logic [WIDTHOUT-1:0] m3_out; // (((A5 * x + A4) * x + A3) * x + A2) * x
logic [WIDTHOUT-1:0] a3_out; // (((A5 * x + A4) * x + A3) * x + A2) * x + A1
logic [WIDTHOUT-1:0] m4_out; // ((((A5 * x + A4) * x + A3) * x + A2) * x + A1) * x
logic [WIDTHOUT-1:0] a4_out; // ((((A5 * x + A4) * x + A3) * x + A2) * x + A1) * x + A0
logic [WIDTHOUT-1:0] y_D;

// Signals for pipeline registers
logic [WIDTHOUT-1:0] m0_out_reg; // pipeline registers for m0_out
logic [WIDTHOUT-1:0] a0_out_reg; // pipeline registers for a0_out
logic [WIDTHOUT-1:0] m1_out_reg; // pipeline registers for m1_out
logic [WIDTHOUT-1:0] a1_out_reg; // pipeline registers for a1_out
logic [WIDTHOUT-1:0] m2_out_reg; // pipeline registers for m2_out
logic [WIDTHOUT-1:0] a2_out_reg; // pipeline registers for a2_out
logic [WIDTHOUT-1:0] m3_out_reg; // pipeline registers for m3_out
logic [WIDTHOUT-1:0] a3_out_reg; // pipeline registers for a3_out
logic [WIDTHOUT-1:0] m4_out_reg; // pipeline registers for m4_out

logic [WIDTHIN-1:0] x_reg; // pipeline registers for x
logic [WIDTHIN-1:0] x_m1; //pipeline registers for x_reg (input to Mult1)
logic [WIDTHIN-1:0] x_m1_reg; // pipeline registers for x_m1
logic [WIDTHIN-1:0] x_m2; //pipeline registers for x_m1_reg (input to Mult2)
logic [WIDTHIN-1:0] x_m2_reg; // pipeline registers for x_m2
logic [WIDTHIN-1:0] x_m3; //pipeline registers for x_m2_reg (input to Mult3)
logic [WIDTHIN-1:0] x_m3_reg; // pipeline registers for x_m3
logic [WIDTHIN-1:0] x_m4; //pipeline registers for x_m3_reg (input to Mult4)

logic valid_reg;		// Output of register valid_Q1
logic valid_m1;			// Output of register valid_reg
logic valid_m1_reg;     // Output of register valid_m1
logic valid_m2;         // Output of register valid_m1_reg
logic valid_m2_reg;     // Output of register valid_m2
logic valid_m3;         // Output of register valid_m2_reg
logic valid_m3_reg;	    // Output of register valid_m3
logic valid_m4;	        // Output of register valid_m3_reg
logic valid_m4_reg;	        // Output of register valid_m4

// compute y value
mult16x16 Mult0 (.i_dataa(A5), 		.i_datab(x), 	.o_res(m0_out));
addr32p16 Addr0 (.i_dataa(m0_out_reg), 	.i_datab(A4), 	.o_res(a0_out));

mult32x16 Mult1 (.i_dataa(a0_out_reg), 	.i_datab(x_m1), 	.o_res(m1_out));
addr32p16 Addr1 (.i_dataa(m1_out_reg), 	.i_datab(A3), 	.o_res(a1_out));

mult32x16 Mult2 (.i_dataa(a1_out_reg), 	.i_datab(x_m2), 	.o_res(m2_out));
addr32p16 Addr2 (.i_dataa(m2_out_reg), 	.i_datab(A2), 	.o_res(a2_out));

mult32x16 Mult3 (.i_dataa(a2_out_reg), 	.i_datab(x_m3), 	.o_res(m3_out));
addr32p16 Addr3 (.i_dataa(m3_out_reg), 	.i_datab(A1), 	.o_res(a3_out));

mult32x16 Mult4 (.i_dataa(a3_out_reg), 	.i_datab(x_m4), 	.o_res(m4_out));
addr32p16 Addr4 (.i_dataa(m4_out_reg), 	.i_datab(A0), 	.o_res(a4_out));

assign y_D = a4_out;

// Combinational logic
always_comb begin
	// signal for enable
	enable = i_ready;
end

// Infer the registers
always_ff @(posedge clk or posedge reset) begin
	if (reset) begin
		valid_Q1 <= 1'b0;
		valid_reg <= 1'b0;
		valid_m1 <= 1'b0;
		valid_m1_reg <= 1'b0;
		valid_m2 <= 1'b0;
		valid_m2_reg <= 1'b0;
		valid_m3 <= 1'b0;
		valid_m3_reg <= 1'b0;
		valid_m4 <= 1'b0;
		valid_m4_reg <= 1'b0;
		valid_Q2 <= 1'b0;
		
		x <= 0;
		y_Q <= 0;

		m0_out_reg <= 'd0;
		a0_out_reg <= 'd0;
		m1_out_reg <= 'd0;
		a1_out_reg <= 'd0;
		m2_out_reg <= 'd0;
		a2_out_reg <= 'd0;
		m3_out_reg <= 'd0;
		a3_out_reg <= 'd0;
		m4_out_reg <= 'd0;

		x_reg <= 'd0;
		x_m1 <= 'd0;
		x_m1_reg <= 'd0;
		x_m2 <= 'd0;
		x_m2_reg <= 'd0;
		x_m3 <= 'd0;
		x_m3_reg <= 'd0;
		x_m4 <= 'd0;

	end else if (enable) begin

		// pipeline for mult-add data path
		m0_out_reg <= m0_out;
		a0_out_reg <= a0_out;
		m1_out_reg <= m1_out;
		a1_out_reg <= a1_out;
		m2_out_reg <= m2_out;
		a2_out_reg <= a2_out;
		m3_out_reg <= m3_out;
		a3_out_reg <= a3_out;
		m4_out_reg <= m4_out;
		// output computed y value
		y_Q <= y_D;

		// pipeline for carry-over x data path
		// read in new x value
		x <= i_x;
		x_reg <= x;
		x_m1 <= x_reg;
		x_m1_reg <= x_m1;
		x_m2 <= x_m1_reg;
		x_m2_reg <= x_m2;
		x_m3 <= x_m2_reg;
		x_m3_reg <= x_m3;
		x_m4 <= x_m3_reg;

		// pipeline for i_valid to o_valid data path
		// propagate the valid value
		valid_Q1 <= i_valid;
		valid_reg <= valid_Q1;
		valid_m1 <= valid_reg;
		valid_m1_reg <= valid_m1;
		valid_m2 <= valid_m1_reg;
		valid_m2_reg <= valid_m2;
		valid_m3 <= valid_m2_reg;
		valid_m3_reg <= valid_m3;
		valid_m4 <= valid_m3_reg;
		valid_m4_reg <= valid_m4
		valid_Q2 <= valid_m4_reg;

	end
end

// assign outputs
assign o_y = y_Q;
// ready for inputs as long as receiver is ready for outputs */
assign o_ready = i_ready;   		
// the output is valid as long as the corresponding input was valid and 
//	the receiver is ready. If the receiver isn't ready, the computed output
//	will still remain on the register outputs and the circuit will resume
//  normal operation when the receiver is ready again (i_ready is high)
assign o_valid = valid_Q2 & i_ready;	

endmodule

/*******************************************************************************************/

// Multiplier module for the first 16x16 multiplication
module mult16x16 (
	input  [15:0] i_dataa,
	input  [15:0] i_datab,
	output [31:0] o_res
);

logic [31:0] result;

always_comb begin
	result = i_dataa * i_datab;
end

// The result of Q2.14 x Q2.14 is in the Q4.28 format. Therefore we need to change it
// to the Q7.25 format specified in the assignment by shifting right and padding with zeros.
assign o_res = {3'b000, result[31:3]};

endmodule

/*******************************************************************************************/

// Multiplier module for all the remaining 32x16 multiplications
module mult32x16 (
	input  [31:0] i_dataa,
	input  [15:0] i_datab,
	output [31:0] o_res
);

logic [47:0] result;

always_comb begin
	result = i_dataa * i_datab;
end

// The result of Q7.25 x Q2.14 is in the Q9.39 format. Therefore we need to change it
// to the Q7.25 format specified in the assignment by selecting the appropriate bits
// (i.e. dropping the most-significant 2 bits and least-significant 14 bits).
assign o_res = result[45:14];

endmodule

/*******************************************************************************************/

// Adder module for all the 32b+16b addition operations 
module addr32p16 (
	input [31:0] i_dataa,
	input [15:0] i_datab,
	output [31:0] o_res
);

// The 16-bit Q2.14 input needs to be aligned with the 32-bit Q7.25 input by zero padding
assign o_res = i_dataa + {5'b00000, i_datab, 11'b00000000000};

endmodule

/*******************************************************************************************/
