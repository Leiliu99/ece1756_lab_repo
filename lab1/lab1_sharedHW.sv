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

// signal for enabling sequential circuit elements
logic enable;

// Signals for inputs that feeds in multplier and adder operator
logic [WIDTHOUT-1:0] m_in_a;
logic [WIDTHOUT-1:0] a_in_a;
logic [WIDTHIN-1:0] m_in_b;
logic [WIDTHIN-1:0] a_in_b;
// Signals for output from adder operator
logic [WIDTHOUT-1:0] y_D;
// Register to hold output y_D
logic [WIDTHOUT-1:0] y_store;	

// // Select signals for choosing inputs for operators
logic multsel_in;
logic [2:0] addsel_in;

// Enumeration for select signals' value
enum logic {A5_IN, STORE_IN} multsel_val;
enum logic [2:0] {A4_IN, A3_IN, A2_IN, A1_IN, A0_IN} addsel_val;
// Enumeration for FSM state value
enum logic [2:0] { PREPARE, ADD_MULT0, ADD_MULT1, ADD_MULT2, ADD_MULT3, ADD_MULT4, OUTPUT} state_val;
logic [2:0] state;
logic [2:0] next_state;

// Signals for output from FSM control
logic start_x;
logic occupied;
logic wb_y;
logic output_valid;

// Compute value using one multiplier and one adder
mult32x16 Mult (.i_dataa(m_in_a), 	.i_datab(m_in_b), 	.o_res(a_in_a));
addr32p16 Addr (.i_dataa(a_in_a), 	.i_datab(a_in_b), 	.o_res(y_D));

always_comb begin
	// signal for enable
	enable = i_ready;
end

// Combinational logic for signals
always_comb begin

	// Mux logic: choosing correct inputs
	case(multsel_in)
		A5_IN: m_in_a = {5'd0, A5, 11'd0};
		STORE_IN: m_in_a = y_store;
		default: m_in_a = '0;
	endcase
	case(addsel_in)
		A4_IN: a_in_b = A4;
		A3_IN: a_in_b = A3;
		A2_IN: a_in_b = A2;
		A1_IN: a_in_b = A1;
		A0_IN: a_in_b = A0;
		default: a_in_b = '0;
	endcase

	// fsm state transition
	case(state)
		PREPARE: if(i_valid) begin
			next_state = ADD_MULT0;
		end else begin
			next_state = PREPARE;
		end
		ADD_MULT0: next_state = ADD_MULT1;
		ADD_MULT1: next_state = ADD_MULT2;
		ADD_MULT2: next_state = ADD_MULT3;
		ADD_MULT3: next_state = ADD_MULT4;
		ADD_MULT4: next_state = OUTPUT;
		OUTPUT: if(enable)begin
			next_state = PREPARE;
		end else begin
			next_state = OUTPUT;
		end
		
		default: next_state = PREPARE;
	endcase
end

// Fsm control flow for signal ouputs of each state
always_comb begin
	  case(state)
		 PREPARE:begin
			occupied = 0;
			wb_y = 0;
			output_valid = 0;
			if(i_valid)begin
				start_x = 1;
			end else begin
				start_x = 0;
			end
			// start_x = 1;
			multsel_in = '0;
			addsel_in = '0;
		 end
		 ADD_MULT0: begin
			start_x = 0;
			multsel_in = A5_IN;
			addsel_in = A4_IN;
			occupied = 1;
			wb_y = 1;
			output_valid = 0;
		 end
		 ADD_MULT1: begin
			start_x = 0;
			multsel_in = STORE_IN;
			addsel_in = A3_IN;
			occupied = 1;
			wb_y = 1;
			output_valid = 0;
		 end
		 ADD_MULT2: begin
			start_x = 0;
			multsel_in = STORE_IN;
			addsel_in = A2_IN;
			occupied = 1;
			wb_y = 1;
			output_valid = 0;
		 end
		 ADD_MULT3: begin
			start_x = 0;
			multsel_in = STORE_IN;
			addsel_in = A1_IN;
			occupied = 1;
			wb_y = 1;
			output_valid = 0;
		 end
		 ADD_MULT4: begin
			start_x = 0;
			multsel_in = STORE_IN;
			addsel_in = A0_IN;
			occupied = 1;
			wb_y = 1;
			output_valid = 0;
		 end
		 OUTPUT:begin
			start_x = 0;
			multsel_in = STORE_IN;
			addsel_in = A0_IN;
			occupied = 1;
			wb_y = 0;
			output_valid = 1;
		 end
		 default:begin
			start_x = 0;
			occupied = 1;
			wb_y = 0;
			output_valid = 0;
			multsel_in = '0;
			addsel_in = '0;
		 end
	  endcase
end

// Infer the registers based on FSM control signals
always_ff @(posedge clk or posedge reset) begin
	if (reset) begin
		state <= PREPARE;
		m_in_b <= '0;
		y_store <= '0;
//		start_x <= 0;
//		occupied <= 0;
//		wb_y <= 0;
//		y_store <= 0;
//
//		o_valid <= 0;
//		o_ready <= 0;
	end else if (enable) begin
		//move to the next state
		state <= next_state;

		// read in new x value for multiplier input
		if (start_x) begin
			m_in_b <= i_x;
		end

		// write back the output from one round of mult+add
		if(wb_y) begin
			y_store <= y_D;
		end
	end
end

// assign outputs
assign o_y = y_store;
assign o_valid = output_valid;
// ready for inputs when receiver is ready for outputs and no more operator being occupied
assign o_ready = i_ready & ~occupied;   		

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
