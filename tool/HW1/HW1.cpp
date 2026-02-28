#include "pin.H"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unordered_set>
#include <iomanip>

using std::cerr;
using std::endl;
using std::string;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "hw1.out", "specify output file name");
KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "number of billions of instructions to fast-forward");

std::ofstream OutFile;

UINT64 icount = 0;
UINT64 ff_target = 0;
UINT64 end_target = 0;

/**
 * For part A & B: instruction profile and CPI calculation
 */
 UINT64 count_loads = 0, count_stores = 0, count_nops = 0, count_direct_calls = 0;
UINT64 count_indirect_calls = 0, count_returns = 0, count_uncond_br = 0, count_cond_br = 0;
UINT64 count_logical = 0, count_rotate_shift = 0, count_flag = 0, count_vector = 0;
UINT64 count_cmov = 0, count_mmx_sse = 0, count_syscall = 0, count_fp = 0, count_others = 0;

/** To track unique 32-byte chunks touched by instructions and data
 * For Part C Memory Footprint
 * @brief We use unordered_set to store unique chunk indices (addr / 32).
 */
std::unordered_set<ADDRINT> instr_chunks;
std::unordered_set<ADDRINT> data_chunks;

UINT64 instr_single_chunk = 0, instr_multi_chunk = 0;
UINT64 data_single_chunk = 0, data_multi_chunk = 0;

/**
 * @brief Various counters and trackers for ISA properties
 * For Part D: ISA Properties
 * We maintain various counters and trackers for the required properties.
 */
UINT64 len_dist[20] = {0};
UINT64 op_dist[20] = {0};
UINT64 rregs_2 = 0;
UINT64 wregs_1 = 0;
UINT64 mem_ops_3 = 0;
UINT64 mem_reads_1 = 0;
UINT64 mem_writes_2 = 0;

UINT64 total_mem_bytes = 0;
UINT64 total_mem_instrs = 0;
UINT32 max_mem_bytes = 0;

bool first_imm = true;
INT32 max_imm = 0, min_imm = 0;

bool first_disp = true;
ADDRDELTA max_disp = 0, min_disp = 0;

VOID InsCount() { icount++; }
ADDRINT FastForward() { return (icount >= ff_target && icount < end_target); }
ADDRINT TerminateCheck() { return (icount >= end_target); }

VOID IncCounter(UINT64* counter, UINT32 amount) { *counter += amount; }

VOID RecordInstrAccess(ADDRINT addr, UINT32 size) {
    ADDRINT start_chunk = addr / 32;
    ADDRINT end_chunk = (addr + size - 1) / 32;
    for (ADDRINT c = start_chunk; c <= end_chunk; c++) instr_chunks.insert(c);
    if (start_chunk == end_chunk) instr_single_chunk++; else instr_multi_chunk++;
}

VOID RecordDataAccess(ADDRINT addr, UINT32 size) {
    ADDRINT start_chunk = addr / 32;
    ADDRINT end_chunk = (addr + size - 1) / 32;
    for (ADDRINT c = start_chunk; c <= end_chunk; c++) data_chunks.insert(c);
    if (start_chunk == end_chunk) data_single_chunk++; else data_multi_chunk++;
}

VOID RecordMemBytes(UINT32 bytes) {
    total_mem_instrs++;
    total_mem_bytes += bytes;
    if (bytes > max_mem_bytes) max_mem_bytes = bytes;
}

VOID RecordImmediate(ADDRINT imm_val) {
    INT32 imm = (INT32)imm_val; // Recover sign as per Prof's instruction
    if (first_imm) { max_imm = min_imm = imm; first_imm = false; }
    else {
        if (imm > max_imm) max_imm = imm;
        if (imm < min_imm) min_imm = imm;
    }
}

VOID RecordDisplacement(ADDRINT disp_val) {
    ADDRDELTA disp = (ADDRDELTA)disp_val; // Recover sign
    if (first_disp) { max_disp = min_disp = disp; first_disp = false; }
    else {
        if (disp > max_disp) max_disp = disp;
        if (disp < min_disp) min_disp = disp;
    }
}

/**
 * Print the output before exiting
 */
VOID PrintStatsAndExit() {
    UINT64 total_type_A = count_nops + count_direct_calls + count_indirect_calls + count_returns +
                          count_uncond_br + count_cond_br + count_logical + count_rotate_shift +
                          count_flag + count_vector + count_cmov + count_mmx_sse + count_syscall +
                          count_fp + count_others;
    UINT64 total_instructions = total_type_A + count_loads + count_stores;

    // Part B: CPI
    UINT64 total_cycles = ((count_loads + count_stores) * 70) + (total_type_A * 1);
    double cpi = (total_instructions > 0) ? (double)total_cycles / total_instructions : 0.0;

    OutFile << "===============================================\n";
    OutFile << "PART A & B: INSTRUCTION PROFILE & CPI\n";
    OutFile << "===============================================\n";
    OutFile << "Total Profiled (Denominator): " << total_instructions << "\n";
    OutFile << "Calculated CPI: " << std::fixed << std::setprecision(4) << cpi << "\n";
    OutFile << "-----------------------------------------------\n";

    auto print_row = [&](string name, UINT64 count) {
        double pct = (total_instructions > 0) ? (count * 100.0 / total_instructions) : 0.0;
        OutFile << std::left << std::setw(25) << name << ": " << std::setw(12) << count
                << " (" << std::fixed << std::setprecision(2) << pct << "%)\n";
    };

    print_row("1. Loads", count_loads);
    print_row("2. Stores", count_stores);
    print_row("3. NOPs", count_nops);
    print_row("4. Direct calls", count_direct_calls);
    print_row("5. Indirect calls", count_indirect_calls);
    print_row("6. Returns", count_returns);
    print_row("7. Uncond branches", count_uncond_br);
    print_row("8. Cond branches", count_cond_br);
    print_row("9. Logical ops", count_logical);
    print_row("10. Rotate/Shift", count_rotate_shift);
    print_row("11. Flag ops", count_flag);
    print_row("12. Vector (AVX)", count_vector);
    print_row("13. Cond moves", count_cmov);
    print_row("14. MMX/SSE", count_mmx_sse);
    print_row("15. System calls", count_syscall);
    print_row("16. Floating-point", count_fp);
    print_row("17. The rest", count_others);

    OutFile << "\n===============================================\n";
    OutFile << "PART C: MEMORY FOOTPRINT (32-BYTE CHUNKS)\n";
    OutFile << "===============================================\n";
    OutFile << "Instruction Footprint (Bytes) : " << instr_chunks.size() * 32 << "\n";
    OutFile << "Instr Accesses (Single Chunk) : " << instr_single_chunk << "\n";
    OutFile << "Instr Accesses (Multi Chunk)  : " << instr_multi_chunk << "\n";
    OutFile << "-----------------------------------------------\n";
    OutFile << "Data Footprint (Bytes)        : " << data_chunks.size() * 32 << "\n";
    OutFile << "Data Accesses (Single Chunk)  : " << data_single_chunk << "\n";
    OutFile << "Data Accesses (Multi Chunk)   : " << data_multi_chunk << "\n";

    OutFile << "\n===============================================\n";
    OutFile << "PART D: ia32 ISA PROPERTIES\n";
    OutFile << "===============================================\n";

    OutFile << "[D.1] Instruction Length Distribution:\n";
    for (int i = 1; i < 20; i++) {
        if (len_dist[i] > 0) OutFile << "  " << i << " bytes: " << len_dist[i] << "\n";
    }

    OutFile << "\n[D.2] Operand Count Distribution:\n";
    for (int i = 0; i < 20; i++) {
        if (op_dist[i] > 0) OutFile << "  " << i << " operands: " << op_dist[i] << "\n";
    }

    OutFile << "\n[D.3] Instrs with 2 Reg Read Operands : " << rregs_2 << "\n";
    OutFile << "[D.4] Instrs with 1 Reg Write Operand : " << wregs_1 << "\n";
    OutFile << "[D.5] Instrs with 3 Memory Operands   : " << mem_ops_3 << "\n";
    OutFile << "[D.6] Instrs with 1 Memory Read Op    : " << mem_reads_1 << "\n";
    OutFile << "[D.7] Instrs with 2 Memory Write Ops  : " << mem_writes_2 << "\n";

    double avg_mem = (total_mem_instrs > 0) ? ((double)total_mem_bytes / total_mem_instrs) : 0.0;
    OutFile << "[D.8] Max Mem Bytes Touched per Instr : " << max_mem_bytes << "\n";
    OutFile << "[D.8] Avg Mem Bytes Touched per Instr : " << std::fixed << std::setprecision(2) << avg_mem << "\n";

    OutFile << "[D.9] Max Immediate Value             : " << max_imm << "\n";
    OutFile << "[D.9] Min Immediate Value             : " << min_imm << "\n";
    OutFile << "[D.10] Max Displacement Value         : " << max_disp << "\n";
    OutFile << "[D.10] Min Displacement Value         : " << min_disp << "\n";
    OutFile << "===============================================\n";

    OutFile.close();
    exit(0);
}

/*
 * Instrumentation Function
 * /
VOID Instruction(INS ins, VOID *v) {

    /**
     * Terminate the program after we reach the end target instruction count, and print stats.
     */
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)TerminateCheck, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)PrintStatsAndExit, IARG_END);

    /**
     * Increment instruction count
     */
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_END);

    /**
     * Non Predicated Calls
     */
    UINT32 size = INS_Size(ins);
    UINT32 op_count = INS_OperandCount(ins);

    // Part C: Instruction Footprint — not predicated (per assignment spec)
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordInstrAccess,
                       IARG_INST_PTR, IARG_UINT32, (UINT32)size, IARG_END);

    // Part D.1: Instruction length distribution — not predicated
    UINT32 l_idx = (size < 20) ? size : 19;
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                       IARG_PTR, &len_dist[l_idx], IARG_UINT32, (UINT32)1, IARG_END);

    // Part D.2: Operand count distribution — not predicated
    UINT32 o_idx = (op_count < 20) ? op_count : 19;
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                       IARG_PTR, &op_dist[o_idx], IARG_UINT32, (UINT32)1, IARG_END);

    // Part D.3: Instructions with exactly 2 register read operands — not predicated
    if (INS_MaxNumRRegs(ins) == 2) {
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                           IARG_PTR, &rregs_2, IARG_UINT32, (UINT32)1, IARG_END);
    }

    // Part D.4: Instructions with exactly 1 register write operand — not predicated
    if (INS_MaxNumWRegs(ins) == 1) {
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                           IARG_PTR, &wregs_1, IARG_UINT32, (UINT32)1, IARG_END);
    }

    // Part D.9: Immediate values — not predicated
    for (UINT32 i = 0; i < op_count; i++) {
        if (INS_OperandIsImmediate(ins, i)) {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordImmediate,
                               IARG_ADDRINT, (ADDRINT)INS_OperandImmediate(ins, i), IARG_END);
        }
    }

    /**
     * Predicated Calls
     */
    UINT32 mem_ops = 0, mem_reads = 0, mem_writes = 0;
    UINT32 num_loads = 0, num_stores = 0;

    /**
     * Count the total unique bytes touched by memory operands in this instruction, for Part D.8.
     */
    UINT32 bytes_touched = 0;

    UINT32 memOperands = INS_MemoryOperandCount(ins);
    for (UINT32 i = 0; i < memOperands; i++) {
        UINT32 mem_size = INS_MemoryOperandSize(ins, i);
        UINT32 chunks = (mem_size + 3) / 4; // ceiling division: max 4 bytes per micro-op

        bytes_touched += mem_size;

        if (INS_MemoryOperandIsRead(ins, i)) {
            mem_ops++;
            mem_reads++;
            num_loads += chunks;
            // Data footprint: record the effective address at runtime (predicated)
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordDataAccess,
                                         IARG_MEMORYOP_EA, i,
                                         IARG_UINT32, (UINT32)mem_size, IARG_END);
        }
        if (INS_MemoryOperandIsWritten(ins, i)) {
            mem_ops++;
            mem_writes++;
            num_stores += chunks;
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordDataAccess,
                                         IARG_MEMORYOP_EA, i,
                                         IARG_UINT32, (UINT32)mem_size, IARG_END);
        }

        /**
         * NOTE: INS_OperandMemoryDisplacement expects a general operand index, not a memory operand index
         * We convert using INS_MemoryOperandIndexToOperandIndex to get the correct index for the displacement operand
         */
        UINT32 opIdx = INS_MemoryOperandIndexToOperandIndex(ins, i);
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordDisplacement,
                                     IARG_ADDRINT, (ADDRINT)INS_OperandMemoryDisplacement(ins, opIdx),
                                     IARG_END);
    }

    // Aggregate Loads/Stores (predicated — only count if instruction executes)
    if (num_loads > 0) {
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                                     IARG_PTR, &count_loads,
                                     IARG_UINT32, (UINT32)num_loads, IARG_END);
    }
    if (num_stores > 0) {
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                                     IARG_PTR, &count_stores,
                                     IARG_UINT32, (UINT32)num_stores, IARG_END);
    }

    // Part D.5: Instructions with exactly 3 memory operands (predicated)
    if (mem_ops == 3) {
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                                     IARG_PTR, &mem_ops_3, IARG_UINT32, (UINT32)1, IARG_END);
    }

    // Part D.6: Instructions with exactly 1 memory read operand (predicated)
    if (mem_reads == 1) {
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                                     IARG_PTR, &mem_reads_1, IARG_UINT32, (UINT32)1, IARG_END);
    }

    // Part D.7: Instructions with exactly 2 memory write operands (predicated)
    if (mem_writes == 2) {
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                                     IARG_PTR, &mem_writes_2, IARG_UINT32, (UINT32)1, IARG_END);
    }

    // Part D.8: Memory bytes touched — only for instructions that have memory operands (predicated)
    if (memOperands > 0) {
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemBytes,
                                     IARG_UINT32, (UINT32)bytes_touched, IARG_END);
    }

    /**
     * Runs at the Instrumentation time and not called in the analysis routine
     */
    UINT64* target_counter = &count_others; // default: "the rest"
    INT32 category = INS_Category(ins);

    if (category == XED_CATEGORY_NOP)
        target_counter = &count_nops;
    else if (category == XED_CATEGORY_CALL)
        target_counter = INS_IsDirectCall(ins) ? &count_direct_calls : &count_indirect_calls;
    else if (category == XED_CATEGORY_RET)
        target_counter = &count_returns;
    else if (category == XED_CATEGORY_UNCOND_BR)
        target_counter = &count_uncond_br;
    else if (category == XED_CATEGORY_COND_BR)
        target_counter = &count_cond_br;
    else if (category == XED_CATEGORY_LOGICAL)
        target_counter = &count_logical;
    else if (category == XED_CATEGORY_ROTATE || category == XED_CATEGORY_SHIFT)
        target_counter = &count_rotate_shift;
    else if (category == XED_CATEGORY_FLAGOP)
        target_counter = &count_flag;
    else if (category == XED_CATEGORY_AVX    || category == XED_CATEGORY_AVX2 ||
             category == XED_CATEGORY_AVX2GATHER || category == XED_CATEGORY_AVX512)
        target_counter = &count_vector;
    else if (category == XED_CATEGORY_CMOV)
        target_counter = &count_cmov;
    else if (category == XED_CATEGORY_MMX || category == XED_CATEGORY_SSE)
        target_counter = &count_mmx_sse;
    else if (category == XED_CATEGORY_SYSCALL)
        target_counter = &count_syscall;
    else if (category == XED_CATEGORY_X87_ALU)
        target_counter = &count_fp;

    // Predicated: only count if the instruction actually executes
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)IncCounter,
                                 IARG_PTR, target_counter, IARG_UINT32, (UINT32)1, IARG_END);
}

VOID Fini(INT32 code, VOID *v) { PrintStatsAndExit(); }

int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) return 1;
    OutFile.open(KnobOutputFile.Value().c_str());
    ff_target  = KnobFastForward.Value() * 1000000000ULL;
    end_target = ff_target + 1000000000ULL;
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    return 0;
}
