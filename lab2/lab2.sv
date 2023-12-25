// This module implements 2D covolution between a 3x3 filter and a 512-pixel-wide image of any height.
// It is assumed that the input image is padded with zeros such that the input and output images have
// the same size. The filter coefficients are symmetric in the x-direction (i.e. f[0][0] = f[0][2], 
// f[1][0] = f[1][2], f[2][0] = f[2][2] for any filter f) and their values are limited to integers
// (but can still be positive of negative). The input image is grayscale with 8-bit pixel values ranging
// from 0 (black) to 255 (white).
module lab2 (
	input  clk,			// Operating clock
	input  reset,			// Active-high reset signal (reset when set to 1)
	input  [71:0] i_f,		// Nine 8-bit signed convolution filter coefficients in row-major format (i.e. i_f[7:0] is f[0][0], i_f[15:8] is f[0][1], etc.)
	input  i_valid,			// Set to 1 if input pixel is valid
	input  i_ready,			// Set to 1 if consumer block is ready to receive a new pixel
	input  [7:0] i_x,		// Input pixel value (8-bit unsigned value between 0 and 255)
	output o_valid,			// Set to 1 if output pixel is valid
	output o_ready,			// Set to 1 if this block is ready to receive a new pixel
	output [7:0] o_y		// Output pixel value (8-bit unsigned value between 0 and 255)
);

localparam FILTER_SIZE = 3;	// Convolution filter dimension (i.e. 3x3)
localparam PIXEL_DATAW = 8;	// Bit width of image pixels and filter coefficients (i.e. 8 bits)

// The following code is intended to show you an example of how to use paramaters and
// for loops in SytemVerilog. It also arrages the input filter coefficients for you
// into a nicely-arranged and easy-to-use 2D array of registers. However, you can ignore
// this code and not use it if you wish to.

logic signed [PIXEL_DATAW-1:0] r_f [FILTER_SIZE-1:0][FILTER_SIZE-1:0]; // 2D array of registers for filter coefficients
integer col, row, buffer_i; // variables to use in the for loop


// Start of your code
localparam IMAGE_SIZE = 514;	//size of the image plus padding(512 + 2)
// We need a buffer to store all the input pixels before the first convolution start
// The first pixel can only be convoluted after two full rows + 3 pixels at the third row
localparam BUFFER_SIZE = IMAGE_SIZE*2 + FILTER_SIZE;
logic signed [PIXEL_DATAW-1:0] input_buffer [BUFFER_SIZE];
// We need a counter to record how many pixels have been added to the buffer
// We use this to tell: convolution begin valid
// The full size could be 514*514 so we need 20 bits for counter
logic [19:0]pixel_counter;
// We also need a row counter to deal with the offset of adding padding into pixel_counter
// Valid need to be stalled for two cycles for every row (due to additional two paddings)
logic [9:0]row_counter;
// Once the counter reach to the BUFFER_SIZE, buffer is enough to perform the first pixel
// Due to the pipelining during convolution, we need to pipeline the valid signal
// In order to match the output cycle
logic valid_stage0;
logic valid_stage1;
logic valid_stage2;
logic valid_stage3;
logic valid_stage4;

// Value to store the output from multiplier and adder
logic signed [15:0] mult_0, mult_1, mult_2, mult_3, mult_4, mult_5, mult_6, mult_7, mult_8;
logic signed [18:0] firstadd_0, firstadd_1, firstadd_2, firstadd_3;
logic signed [18:0] secondadd_0, secondadd_1;
logic signed [18:0] thirdadd_0;
// The maximum bits from the last adder will be 20 bits
logic signed [19:0] finaladd_out;
// Cap output to 8 bits
logic unsigned [7:0] finalcap_out;

// Pipeline registers to propogate the value 
logic signed [18:0] mult_0_reg, mult_1_reg, mult_2_reg, mult_3_reg, mult_4_reg, mult_5_reg, mult_6_reg, mult_7_reg, mult_8_reg;
logic signed [18:0] firstadd_0_reg, firstadd_1_reg, firstadd_2_reg, firstadd_3_reg, firstadd_wait;
logic signed [18:0] secondadd_0_reg, secondadd_1_reg, secondadd_wait;
logic signed [18:0] thirdadd_0_reg, thirdadd_wait;

always_ff @ (posedge clk) begin
	// If reset signal is high, set all the filter coefficient registers to zeros
	// We're using a synchronous reset, which is recommended style for recent FPGA architectures
	if(reset)begin

		for(row = 0; row < FILTER_SIZE; row = row + 1) begin
			for(col = 0; col < FILTER_SIZE; col = col + 1) begin
				r_f[row][col] <= 0;
			end
		end
		// Clean out all the pixels stored in the buffer
		for(buffer_i =0; buffer_i < BUFFER_SIZE; buffer_i = buffer_i + 1)begin
			input_buffer[buffer_i] <= '0;
		end
		
		// Reset the counters 
		pixel_counter <= '0;
		row_counter <= '0;
		// Reset the valid signal
		valid_stage0 <= 0;
		
	// Otherwise, register the input filter coefficients into the 2D array signal
	end else begin
		for(row = 0; row < FILTER_SIZE; row = row + 1) begin
			for(col = 0; col < FILTER_SIZE; col = col + 1) begin
				// Rearrange the 72-bit input into a 3x3 array of 8-bit filter coefficients.
				// signal[a +: b] is equivalent to signal[a+b-1 : a]. You can try to plug in
				// values for col and row from 0 to 2, to understand how it operates.
				// For example at row=0 and col=0: r_f[0][0] = i_f[0+:8] = i_f[7:0]
				//	       at row=0 and col=1: r_f[0][1] = i_f[8+:8] = i_f[15:8]
				r_f[row][col] <= i_f[(row * FILTER_SIZE * PIXEL_DATAW)+(col * PIXEL_DATAW) +: PIXEL_DATAW];
			
			end
		end
		
		if(i_valid) begin
			// Prepare for the input buffer
			// The newest input will be put in the first index of buffer
			// All the rest will be shifted by 1
			input_buffer[0] <= i_x;
			pixel_counter <= pixel_counter + 1;
			for(buffer_i=0; buffer_i < BUFFER_SIZE - 1; buffer_i = buffer_i + 1)begin
				// Shift all pixels by 1 in the buffer
				input_buffer[buffer_i + 1] <= input_buffer[buffer_i];
			end

			// Continously record row_counter
			// To know which row we are currently at
			row_counter <= row_counter + 1;

			// Propogate the output from multiplier/adder
			// Using the pipeline registers
			mult_8_reg <= mult_8;
			mult_7_reg <= mult_7;
			mult_6_reg <= mult_6;
			mult_5_reg <= mult_5;
			mult_4_reg <= mult_4;
			mult_3_reg <= mult_3;
			mult_2_reg <= mult_2;
			mult_1_reg <= mult_1;
			mult_0_reg <= mult_0;

			firstadd_0_reg <= firstadd_0;
			firstadd_1_reg <= firstadd_1;
			firstadd_2_reg <= firstadd_2;
			firstadd_3_reg <= firstadd_3;
			firstadd_wait <= mult_0_reg;

			secondadd_0_reg <= secondadd_0;
			secondadd_1_reg <= secondadd_1;
			secondadd_wait <= firstadd_wait;

			thirdadd_0_reg <= thirdadd_0;
			thirdadd_wait <= secondadd_wait;

			// Cap the final output to 8 bits
			if (finaladd_out > 20'sd255) begin
				finalcap_out <= 8'd255;
			end
			else if (finaladd_out < 20'sd0)begin
				finalcap_out <= 8'd0;
			end
			else begin
				finalcap_out <= finaladd_out[7:0];
			end

			// Propogate the valid signal
			// Using the pipeline registers
			valid_stage1 <= valid_stage0;
			valid_stage2 <= valid_stage1;
			valid_stage3 <= valid_stage2;
			valid_stage4 <= valid_stage3;

		end

		// We have reached enough buffer to start a valid calculation
		if(pixel_counter >= BUFFER_SIZE) begin
			// Additional output caused by two paddings when transfering
			// Rows, should stall valid signal for two cycles
			if(row_counter == 'd1) begin
				valid_stage0 = 1'b0;
			end
			else if(row_counter == 'd2)begin
				valid_stage0 = 1'b0;
			end
			else begin
				valid_stage0 <= 1'b1;
			end
		end

		if(row_counter == IMAGE_SIZE - 1)begin
			// Reset row counter for new rows
			row_counter <= 'd0;
		end
	end
end


mult8x8 mult_layer0_8 (.i_dataa(input_buffer[0]), .i_datab(r_f[2][2]), .o_res(mult_8));
mult8x8 mult_layer0_7 (.i_dataa(input_buffer[1]), .i_datab(r_f[2][1]), .o_res(mult_7));
mult8x8 mult_layer0_6 (.i_dataa(input_buffer[2]), .i_datab(r_f[2][0]), .o_res(mult_6));
mult8x8 mult_layer0_5 (.i_dataa(input_buffer[IMAGE_SIZE]), .i_datab(r_f[1][2]), .o_res(mult_5));
mult8x8 mult_layer0_4 (.i_dataa(input_buffer[IMAGE_SIZE + 1]), .i_datab(r_f[1][1]), .o_res(mult_4));
mult8x8 mult_layer0_3 (.i_dataa(input_buffer[IMAGE_SIZE + 2]), .i_datab(r_f[1][0]), .o_res(mult_3));
mult8x8 mult_layer0_2 (.i_dataa(input_buffer[IMAGE_SIZE*2]), .i_datab(r_f[0][2]), .o_res(mult_2));
mult8x8 mult_layer0_1 (.i_dataa(input_buffer[IMAGE_SIZE*2 + 1]), .i_datab(r_f[0][1]), .o_res(mult_1));
mult8x8 mult_layer0_0 (.i_dataa(input_buffer[IMAGE_SIZE*2 + 2]), .i_datab(r_f[0][0]), .o_res(mult_0));

add19x19 add_layer1_3 (.i_dataa(mult_8_reg), .i_datab(mult_7_reg), .o_res(firstadd_3));
add19x19 add_layer1_2 (.i_dataa(mult_6_reg), .i_datab(mult_5_reg), .o_res(firstadd_2));
add19x19 add_layer1_1 (.i_dataa(mult_4_reg), .i_datab(mult_3_reg), .o_res(firstadd_1));
add19x19 add_layer1_0 (.i_dataa(mult_2_reg), .i_datab(mult_1_reg), .o_res(firstadd_0));

add19x19 add_layer2_1 (.i_dataa(firstadd_3_reg), .i_datab(firstadd_2_reg), .o_res(secondadd_1));
add19x19 add_layer2_0 (.i_dataa(firstadd_1_reg), .i_datab(firstadd_0_reg), .o_res(secondadd_0));

add19x19 add_layer3_0 (.i_dataa(secondadd_1_reg), .i_datab(secondadd_0_reg), .o_res(thirdadd_0));

add19x19 add_layer4_0 (.i_dataa(thirdadd_0_reg), .i_datab(thirdadd_wait), .o_res(finaladd_out));


assign o_y = finalcap_out;
assign o_valid = valid_stage4;
assign o_ready = i_ready;
// End of your code

endmodule

module mult8x8 (
	input unsigned [7:0] i_dataa,
	input signed [7:0] i_datab,
	output signed[15:0] o_res
);
	assign o_res = $signed({1'b0, i_dataa}) * i_datab;

endmodule

module add19x19(
	input  signed [18:0] i_dataa,
	input  signed [18:0] i_datab,
	output signed [19:0] o_res
);
	assign o_res = i_dataa + i_datab;
endmodule



