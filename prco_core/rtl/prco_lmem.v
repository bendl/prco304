// prco_lmem.v

`include "inc/prco_constants.v"

module prco_lmem (
    input           i_clk,
    input           i_reset,
    
    // Pipeline signals
    input           i_ce_fetch,
    input           i_ce_alu,
    output reg      q_ce_dec,
    output reg      q_ce_reg,

    input           i_mem_we,
    input [15:0]    i_mem_addr,
    input [15:0]    i_mem_dina,

    output reg          q_mem_done,
    output reg [15:0]   q_mem_douta
);
    /// Define the on chip memory size
    parameter P_LMEM_DEPTH = 255;
    
    reg [`REG_WIDTH:0] r_lmem[0:P_LMEM_DEPTH];
    integer i = 0;

    initial begin
        // Load compiled program into memory
        //*
        $readmemh(
            //"/home/ise/XilinxVM/prco304/prco_compiler/test/verilog_memh.txt",
            "Z:\\uni\\prco304\\prco_compiler\\test\\verilog_memh.txt",
            r_lmem);
        //*/
        
        /*
        r_lmem[0] = 16'h2002; // MOVI   0b000, 2
        r_lmem[1] = 16'h2102; // MOVI   0b001, 1
        r_lmem[2] = 16'h6820; // CMP    Ax, Bx
        r_lmem[3] = 16'ha805;
        //*/
        
    
        // Write initial memory contents for debug
        $display("r_lmem:");
        for (i = 0; i < 10; i = i + 1) begin
            //r_lmem[i] = 0;
            $display("RAM[0x%h] = %h", i, r_lmem[i]);
        end
        // Print out top value
        $display("RAM[0x%h] = %h", 
            P_LMEM_DEPTH, r_lmem[P_LMEM_DEPTH]);
        
        // Debug instructions
        /*
        r_lmem[0] = 16'h20ab; // MOVI   0b000, ab
        r_lmem[1] = 16'h21cd; // MOVI   0b001, cd
        r_lmem[3] = 16'h0B00; // LW     0b011, DX +0(AX)
        r_lmem[3] = 16'h0B00; // LW     0b100, DX -1(AX)
        r_lmem[5] = 16'h22ef; // MOVI   0b010, ef
        r_lmem[6] = 16'h24aa; // MOVI   0b100, aa
        //

        // Test SW
        r_lmem[0] = 16'h24ab; // MOVI   0b100, ab
        r_lmem[1] = 16'h23cd; // MOVI   0b011, cd
        r_lmem[2] = 16'h1460; // SW     0b100, +0(0b011)
        r_lmem[3] = 16'h1461; // SW     0b100, +1(0b011)

        r_lmem[4] = 16'h6400; // JMP(UN)0b100

        // Test LW
        r_lmem[5] = 16'h24aa; // MOVI   0b100, aa
        r_lmem[6] = 16'h0880; // LW     0b000, +0(b100)
        */

        // Test LW instruction
        r_lmem[16'h00aa] = 16'hCA;
        r_lmem[16'h00ab] = 16'hFE;
        r_lmem[16'h00ac] = 16'hBA;
        r_lmem[16'h00ad] = 16'hBE;

        // Test CMP
        /*
        r_lmem[1] = 16'h2011;
        r_lmem[2] = 16'h2110;
        r_lmem[3] = 16'h6820;
        r_lmem[4] = 16'h2200;
        r_lmem[5] = 16'h6200;
        //*/

        // Test UART
        /*
        r_lmem[0] = 16'h2042;
        r_lmem[1] = 16'h9800;
        r_lmem[2] = 16'h2045;
        r_lmem[3] = 16'h9800;
        r_lmem[4] = 16'h204E;
        r_lmem[5] = 16'h9800;
        r_lmem[6] = 16'h2020;
        r_lmem[7] = 16'h9800;
        r_lmem[8] = 16'h2100;
        r_lmem[9] = 16'h6100;
        //*/
		
		// Test Compiler
        /*
r_lmem[0] = 16'h4fff;
r_lmem[1] = 16'h16e0;
r_lmem[2] = 16'h1ee0;
r_lmem[3] = 16'h2020;
r_lmem[4] = 16'h2209;
r_lmem[5] = 16'h4fff;
r_lmem[6] = 16'h12e0;
r_lmem[7] = 16'h220d;
r_lmem[8] = 16'h6200;
r_lmem[9] = 16'h1fc0;
r_lmem[10] = 16'h0ee0;
r_lmem[11] = 16'h4f01;
r_lmem[12] = 16'h9000;
r_lmem[13] = 16'h4fff;
r_lmem[14] = 16'h16e0;
r_lmem[15] = 16'h1ee0;
r_lmem[16] = 16'h200a;
r_lmem[17] = 16'h4fff;
r_lmem[18] = 16'h10e0;
r_lmem[19] = 16'h2001;
r_lmem[20] = 16'h0ae0;
r_lmem[21] = 16'h4f01;
r_lmem[22] = 16'h4040;
r_lmem[23] = 16'h1fc0;
r_lmem[24] = 16'h0ee0;
r_lmem[25] = 16'h4f01;
r_lmem[26] = 16'h0ae0;
r_lmem[27] = 16'h4f01;
r_lmem[28] = 16'h6200;
		//*/
        

        // TODO: Move to microcode or to instruction data?
        //r_lmem[P_LMEM_DEPTH] = 16'h00ff; // MOVI 2, ef
    end

    always @(posedge i_clk) begin
		if(i_reset) begin
			// Do nothing
			q_ce_dec <= 0;
			q_ce_reg <= 0;
		end else if(i_ce_fetch || i_ce_alu) begin
            if (i_mem_we == 1) begin
                $display("Writing 0x%h to RAM[0x%h]", 
                    i_mem_dina, i_mem_addr);
                r_lmem[i_mem_addr] <= i_mem_dina;
            end

            if(i_ce_fetch) begin
                q_ce_dec <= 1;
                q_ce_reg <= 0;
            end else if (i_ce_alu) begin
                q_ce_dec <= 0;
                q_ce_reg <= 1;
            end else begin
                q_ce_dec <= 0;
                q_ce_reg <= 0;
            end

            $display("MEM: Reading %h", i_mem_addr);
            q_mem_douta <= r_lmem[i_mem_addr];
        end

        if(q_ce_dec || q_ce_reg) begin
            q_ce_dec <= 0;
            q_ce_reg <= 0;
        end
    end
    
    //assign q_mem_douta = r_lmem[i_mem_addr];
    
endmodule