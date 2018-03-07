// prco_core

`include "inc/prco_constants.v"
`include "inc/prco_misc.v"

module prco_core(
    input           i_clk,
    input           i_en,
    input           i_reset,
    
    output reg      q_debug_instr_clk,
    output [7:0]    q_debug
);

    // program counter
    reg [15:0]  pc = 0;
    reg         pc_branch = 0;

    wire [5:0]  r_dec_op;
    wire [2:0]  r_dec_seld;
    wire [2:0]  r_dec_sela;
    wire        r_dec_we;
    wire [15:0] r_dec_imm8;
    wire [4:0]  r_dec_simm5;

    wire [15:0] r_reg_doutd;
    wire [15:0] r_reg_douta;
    reg         r_reg_en = 1;
    reg [15:0]  r_reg_dina;

    // Tempory registers for storing register select paths
    // for switching between type 2 and 3 instructions.
    reg [2:0]   r_reg_sela;
    reg [2:0]   r_reg_selb;
    
    reg         r_dec_en = 1;

    // Only write to a register when the decode instr requires it
    // and: 
    //   - We are at the write-back stage
    wire        r_reg_we = r_dec_we & (r_reg_q_ce_fetch);
    wire        r_lmem_we = r_dec_req_ram_we & (r_mem_i_ce);
    
    wire [15:0] r_alu_result;
    wire [15:0] r_mem_douta;

    reg [15:0]  r_mem_addr;


    // Offset the ALU -> RAM request by 1 clock cycle
    reg         r_mem_int_i_ce = 0;
    reg         r_mem_i_ce = 0;

    // Pipeline signals
    // Jump-start the CPU into running
    reg           r_cpu_init;
    wire          i_ce = r_cpu_init || (r_reg_q_ce_fetch || r_dec_q_fetch);
    reg           q_ce;

    always @(posedge i_clk) begin
        if(r_alu_q_ce_ram) begin
            r_mem_addr <= r_alu_result;
            r_mem_int_i_ce <= 1;
        end else if (r_mem_int_i_ce) begin
            r_mem_i_ce <= 1;
            r_mem_int_i_ce <= 0;
        end else begin
            r_mem_int_i_ce <= 0;
            r_mem_i_ce <= 0;
            r_mem_addr <= pc;
        end
    end

    always @(posedge i_clk) begin
        if(r_mem_q_ce_reg) begin
            r_reg_dina <= r_mem_douta;
        end else begin
            r_reg_dina <= r_alu_result;
        end
    end

    always @(posedge i_clk, posedge i_reset) begin
        if (i_reset == 1) begin
            pc <= 0;
            q_ce <= 0;
            r_cpu_init <= 1;
        end else begin
            `PULSE_SIGNAL(q_ce);
            r_cpu_init <= 0;
            
            if(r_alu_q_should_branch) begin
                pc_branch <= 1;
            end
            if(q_debug_instr_clk) begin
                q_debug_instr_clk <= 0;
            end

            if(i_ce) begin
                q_debug_instr_clk <= 1;

                if(pc_branch) begin
                    $display("Jumping to %d", r_alu_result);
                    pc <= r_alu_result;
                    pc_branch <= 0;
                end else begin
                    pc <= pc + 1;
                end

                q_ce <= 1;
            end else begin
                q_ce <= 0;
            end
        end
    end    

    always @(posedge i_clk) begin
        
    end

    // Swap the register select lines if type 3 instruction
    // (3 reg selects)
    always @(posedge i_clk) begin
        if (r_dec_q_third_sel) begin
            $display("Decoder CMP... Swapping register inputs...");
            r_reg_sela <= r_dec_sela;
            r_reg_selb <= r_dec_selb;
        end else begin
            r_reg_sela <= r_dec_seld;
            r_reg_selb <= r_dec_sela;
        end
    end

    // Instantiate the module
    prco_lmem inst_lmem (
        .i_clk(i_clk), 

        .i_ce_fetch(q_ce),
        .i_ce_alu(r_mem_i_ce),

        .q_ce_dec(r_mem_q_ce_decode),
        .q_ce_reg(r_mem_q_ce_reg),
        
        .i_mem_we(r_lmem_we), 
        .i_mem_addr(r_mem_addr), 
        .i_mem_dina(r_reg_doutd), 
        .q_mem_douta(r_mem_douta)
    );

    // Instantiate the module
    prco_decoder inst_decoder (
        .i_clk(i_clk), 
        .i_en(r_dec_en), 

        .i_ce(r_mem_q_ce_decode),
        .q_ce(r_dec_q_ce),
        .q_fetch(r_dec_q_fetch),

        .i_instr(r_mem_douta), 
        .q_op(r_dec_op), 

        .q_seld(r_dec_seld), 
        .q_sela(r_dec_sela),
        .q_selb(r_dec_selb),
        .q_third_sel(r_dec_q_third_sel),

        .q_imm8(r_dec_imm8),
        .q_simm5(r_dec_simm5),
        .q_reg_we(r_dec_we),

        .q_req_ram(r_dec_ram_req),
        .q_req_ram_we(r_dec_req_ram_we)
    );

    // Instantiate the module
    prco_regs inst_regs (
        .i_clk(i_clk), 
        .i_en(r_reg_en), 

        .i_ce_ram   (r_mem_q_ce_reg),
        .i_ce_dec   (r_dec_q_ce),
        .i_ce_alu   (r_alu_q_ce_reg),
        .q_ce_alu   (r_reg_q_ce_alu),
        .q_ce_fetch (r_reg_q_ce_fetch),

        .i_reset(i_reset), 

        .i_sela(r_reg_sela), 
        .i_selb(r_reg_selb), 

        .q_data(r_reg_doutd), 
        .q_datb(r_reg_douta), 

        .i_we(r_reg_we), 
        .i_seld(r_dec_seld), 
        .i_datd(r_reg_dina)
    );
    
    // Instantiate the module
    prco_alu inst_alu (
        .i_clk(i_clk), 

        .i_ce(r_reg_q_ce_alu),
        .i_dec_req_ram(r_dec_ram_req),
        .q_ce_ram(r_alu_q_ce_ram),
        .q_ce_reg(r_alu_q_ce_reg),

        .i_op(r_dec_op), 

        .i_data(r_reg_douta), 
        .i_datb(r_reg_doutd), 

        .i_imm8(r_dec_imm8), 
        .i_simm5(r_dec_simm5), 
        .q_result(r_alu_result),
        .q_should_branch(r_alu_q_should_branch)
    );
endmodule
