#ifndef EMITTER_H
#define EMITTER_H

#include "common.h"

typedef enum {
    REG_X0 = 0, REG_X1, REG_X2, REG_X3, REG_X4, REG_X5, REG_X6, REG_X7,
    REG_X8, REG_X9, REG_X10, REG_X11, REG_X12, REG_X13, REG_X14, REG_X15,
    REG_X16, REG_X17, REG_X18, REG_X19, REG_X20, REG_X21, REG_X22, REG_X23,
    REG_X24, REG_X25, REG_X26, REG_X27, REG_X28, REG_X29, REG_X30,
    REG_SP = 31, REG_XZR = 32
} Reg;

typedef enum {
    REG_S0 = 0, REG_S1, REG_S2, REG_S3, REG_S4, REG_S5, REG_S6, REG_S7,
    REG_S8, REG_S9, REG_S10, REG_S11, REG_S12, REG_S13, REG_S14, REG_S15,
    REG_S16, REG_S17, REG_S18, REG_S19, REG_S20, REG_S21, REG_S22, REG_S23,
    REG_S24, REG_S25, REG_S26, REG_S27, REG_S28, REG_S29, REG_S30, REG_S31
} FloatReg;

typedef enum {
    COND_EQ = 0,
    COND_NE,
    COND_LT,
    COND_LE,
    COND_GT,
    COND_GE
} Cond;

typedef struct {
    int id;
    int offset; // Offset in bytes, -1 if undefined
} Label;

typedef struct {
    int label_id;
    int instr_offset; // Offset of instruction in buffer
    bool is_conditional;
    Cond cond;
} Reloc;

typedef struct {
    char *name;
    int offset;
} JITSymbol;

typedef struct {
    char *name;
    int instr_offset;
} SymbolReloc;

typedef struct Emitter {
    bool binary_mode;
    FILE *out_file;
    unsigned char *buffer;
    int capacity;
    int size;

    Label labels[500];
    int label_count;
    Reloc relocs[1000];
    int reloc_count;

    JITSymbol symbols[100];
    int symbol_count;

    SymbolReloc sym_relocs[100];
    int sym_reloc_count;
} Emitter;

void emitter_add_symbol(Emitter *e, const char *name, int offset);
int emitter_find_symbol_offset(Emitter *e, const char *name);

// Emitter lifecycle
void emitter_init(Emitter *e, FILE *out_file, bool binary_mode);
void emitter_free(Emitter *e);

// Label management
int emitter_new_label(Emitter *e);
void emitter_define_label(Emitter *e, int label_id);
void emitter_resolve_relocs(Emitter *e);

// Core instructions
void emit_ret(Emitter *e);
void emit_mov_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn);
void emit_mov_imm(Emitter *e, bool is_64bit, Reg rd, int imm);
void emit_add_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn, Reg rm);
void emit_sub_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn, Reg rm);
void emit_add_imm(Emitter *e, bool is_64bit, Reg rd, Reg rn, int imm);
void emit_sub_imm(Emitter *e, bool is_64bit, Reg rd, Reg rn, int imm);

void emit_mul_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn, Reg rm);
void emit_sdiv_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn, Reg rm);
void emit_neg_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn);
void emit_mvn_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn);

void emit_cmp_reg(Emitter *e, bool is_64bit, Reg rn, Reg rm);
void emit_cmp_imm(Emitter *e, bool is_64bit, Reg rn, int imm);
void emit_cset(Emitter *e, Reg rd, Cond cond);

void emit_str_imm(Emitter *e, int size, Reg rt, Reg rn, int offset);
void emit_ldr_imm(Emitter *e, int size, Reg rt, Reg rn, int offset);
void emit_str_preindex(Emitter *e, int size, Reg rt, Reg rn, int offset);
void emit_ldr_postindex(Emitter *e, int size, Reg rt, Reg rn, int offset);

void emit_fstr_imm(Emitter *e, FloatReg rt, Reg rn, int offset);
void emit_fldr_imm(Emitter *e, FloatReg rt, Reg rn, int offset);
void emit_fstr_preindex(Emitter *e, FloatReg rt, Reg rn, int offset);
void emit_fldr_postindex(Emitter *e, FloatReg rt, Reg rn, int offset);

void emit_stp_preindex(Emitter *e, Reg rt1, Reg rt2, Reg rn, int offset);
void emit_ldp_postindex(Emitter *e, Reg rt1, Reg rt2, Reg rn, int offset);

// Floating point instructions
void emit_fadd(Emitter *e, FloatReg rd, FloatReg rn, FloatReg rm);
void emit_fsub(Emitter *e, FloatReg rd, FloatReg rn, FloatReg rm);
void emit_fmul(Emitter *e, FloatReg rd, FloatReg rn, FloatReg rm);
void emit_fdiv(Emitter *e, FloatReg rd, FloatReg rn, FloatReg rm);
void emit_fneg(Emitter *e, FloatReg rd, FloatReg rn);
void emit_fcmp(Emitter *e, FloatReg rn, FloatReg rm);
void emit_fcvt_d_s(Emitter *e, int d_idx, FloatReg sn);
void emit_fmov(Emitter *e, FloatReg rd, FloatReg rn);

// Control flow & labels
void emit_b_label(Emitter *e, int label_id);
void emit_b_cond_label(Emitter *e, Cond cond, int label_id);
void emit_bl(Emitter *e, const char *func_name);

// Helper helper
void emit_sxtw(Emitter *e, Reg rd, Reg rn);

// Literal/Data emission & PC-relative address loads
void emit_float_data(Emitter *e, float val);
void emit_string_data(Emitter *e, const char *str);
void emit_ldr_pc_relative_float(Emitter *e, FloatReg rt, int label_id);
void emit_adr(Emitter *e, Reg rd, int label_id);

#endif // EMITTER_H
