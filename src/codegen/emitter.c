#include "emitter.h"
#include <stdint.h>

static const char *reg_names_64[] = {
    "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
    "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
    "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
    "x24", "x25", "x26", "x27", "x28", "x29", "x30", "sp", "xzr"
};

static const char *reg_names_32[] = {
    "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7",
    "w8", "w9", "w10", "w11", "w12", "w13", "w14", "w15",
    "w16", "w17", "w18", "w19", "w20", "w21", "w22", "w23",
    "w24", "w25", "w26", "w27", "w28", "w29", "w30", "wsp", "wzr"
};

static const char *float_reg_names[] = {
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15",
    "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23",
    "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31"
};

static const char *cond_names[] = {
    "eq", "ne", "lt", "le", "gt", "ge"
};

static uint32_t reg_enc(Reg r) {
    if (r == REG_XZR) return 31;
    return (uint32_t)r;
}

static uint32_t float_reg_enc(FloatReg r) {
    return (uint32_t)r;
}

static uint32_t cond_code(Cond cond) {
    switch (cond) {
        case COND_EQ: return 0;
        case COND_NE: return 1;
        case COND_LT: return 11;
        case COND_LE: return 13;
        case COND_GT: return 12;
        case COND_GE: return 10;
    }
    return 0;
}

static uint32_t invert_cond(Cond cond) {
    switch (cond) {
        case COND_EQ: return 1;  // NE
        case COND_NE: return 0;  // EQ
        case COND_LT: return 10; // GE
        case COND_LE: return 12; // GT
        case COND_GT: return 13; // LE
        case COND_GE: return 11; // LT
    }
    return 0;
}

static void emit_u32(Emitter *e, uint32_t val) {
    if (e->size + 4 > e->capacity) {
        e->capacity = e->capacity == 0 ? 4096 : e->capacity * 2;
        e->buffer = realloc(e->buffer, e->capacity);
    }
    e->buffer[e->size++] = val & 0xff;
    e->buffer[e->size++] = (val >> 8) & 0xff;
    e->buffer[e->size++] = (val >> 16) & 0xff;
    e->buffer[e->size++] = (val >> 24) & 0xff;
}

void emitter_init(Emitter *e, FILE *out_file, bool binary_mode) {
    e->binary_mode = binary_mode;
    e->out_file = out_file;
    e->buffer = NULL;
    e->capacity = 0;
    e->size = 0;
    e->label_count = 0;
    e->reloc_count = 0;
    e->symbol_count = 0;
    e->sym_reloc_count = 0;
}

void emitter_free(Emitter *e) {
    if (e->buffer) {
        free(e->buffer);
    }
    for (int i = 0; i < e->symbol_count; i++) {
        free(e->symbols[i].name);
    }
    for (int i = 0; i < e->sym_reloc_count; i++) {
        free(e->sym_relocs[i].name);
    }
}

void emitter_add_symbol(Emitter *e, const char *name, int offset) {
    e->symbols[e->symbol_count].name = xstrdup(name);
    e->symbols[e->symbol_count].offset = offset;
    e->symbol_count++;
}

int emitter_find_symbol_offset(Emitter *e, const char *name) {
    for (int i = 0; i < e->symbol_count; i++) {
        if (strcmp(e->symbols[i].name, name) == 0) {
            return e->symbols[i].offset;
        }
    }
    return -1;
}

int emitter_new_label(Emitter *e) {
    int id = e->label_count++;
    e->labels[id].id = id;
    e->labels[id].offset = -1;
    return id;
}

void emitter_define_label(Emitter *e, int label_id) {
    if (e->binary_mode) {
        e->labels[label_id].offset = e->size;
    } else {
        fprintf(e->out_file, ".L.label.%d:\n", label_id);
    }
}

void emitter_resolve_relocs(Emitter *e) {
    if (!e->binary_mode) return;
    
    // 1. Resolve label relative branches & loads
    for (int i = 0; i < e->reloc_count; i++) {
        Reloc r = e->relocs[i];
        int target_offset = e->labels[r.label_id].offset;
        if (target_offset == -1) {
            error("JIT error: Undefined label %d", r.label_id);
        }
        int diff = target_offset - r.instr_offset;
        int diff_instr = diff / 4;

        uint32_t instr = e->buffer[r.instr_offset] |
                         (e->buffer[r.instr_offset + 1] << 8) |
                         (e->buffer[r.instr_offset + 2] << 16) |
                         (e->buffer[r.instr_offset + 3] << 24);

        if ((int)r.cond == -1) {
            // ADR: 21-bit signed byte offset
            printf("[DEBUG] ADR reloc: label=%d, diff=%d\n", r.label_id, diff);
            instr &= ~(3 << 29);
            instr &= ~(0x7ffff << 5);
            uint32_t immlo = diff & 3;
            uint32_t immhi = (diff >> 2) & 0x7ffff;
            instr |= (immlo << 29);
            instr |= (immhi << 5);
        } else if ((int)r.cond == -2) {
            // LDR float PC-relative: 19-bit signed offset (scaled by 4)
            printf("[DEBUG] LDR float reloc: label=%d, diff=%d, diff_instr=%d\n", r.label_id, diff, diff_instr);
            instr &= ~(0x7ffff << 5);
            instr |= ((diff_instr & 0x7ffff) << 5);
        } else if (r.is_conditional) {
            printf("[DEBUG] B.cond reloc: label=%d, diff=%d, diff_instr=%d\n", r.label_id, diff, diff_instr);
            instr &= ~(0x7ffff << 5);
            instr |= ((diff_instr & 0x7ffff) << 5);
        } else {
            printf("[DEBUG] B reloc: label=%d, diff=%d, diff_instr=%d\n", r.label_id, diff, diff_instr);
            instr &= ~0x3ffffff;
            instr |= (diff_instr & 0x3ffffff);
        }

        e->buffer[r.instr_offset] = instr & 0xff;
        e->buffer[r.instr_offset + 1] = (instr >> 8) & 0xff;
        e->buffer[r.instr_offset + 2] = (instr >> 16) & 0xff;
        e->buffer[r.instr_offset + 3] = (instr >> 24) & 0xff;
    }

    // 2. Resolve local function symbol relocations
    for (int i = 0; i < e->sym_reloc_count; i++) {
        SymbolReloc r = e->sym_relocs[i];
        int target_offset = emitter_find_symbol_offset(e, r.name);
        if (target_offset == -1) {
            error("JIT error: Undefined local function %s", r.name);
        }
        int diff = target_offset - r.instr_offset;
        int diff_instr = diff / 4;

        uint32_t instr = e->buffer[r.instr_offset] |
                         (e->buffer[r.instr_offset + 1] << 8) |
                         (e->buffer[r.instr_offset + 2] << 16) |
                         (e->buffer[r.instr_offset + 3] << 24);

        // This is always an unconditional BL instruction (26-bit signed offset)
        instr &= ~0x3ffffff;
        instr |= (diff_instr & 0x3ffffff);

        e->buffer[r.instr_offset] = instr & 0xff;
        e->buffer[r.instr_offset + 1] = (instr >> 8) & 0xff;
        e->buffer[r.instr_offset + 2] = (instr >> 16) & 0xff;
        e->buffer[r.instr_offset + 3] = (instr >> 24) & 0xff;
    }
}

void emit_ret(Emitter *e) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    ret\n");
    } else {
        emit_u32(e, 0xd65f03c0);
    }
}

void emit_mov_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    mov %s, %s\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd],
                is_64bit ? reg_names_64[rn] : reg_names_32[rn]);
    } else {
        if (rn == REG_SP || rd == REG_SP) {
            emit_u32(e, (is_64bit ? 0x91000000 : 0x11000000) | (reg_enc(rn) << 5) | reg_enc(rd));
        } else {
            uint32_t op = is_64bit ? 0xaa0003e0 : 0x2a0003e0;
            emit_u32(e, op | (reg_enc(rn) << 16) | reg_enc(rd));
        }
    }
}

void emit_mov_imm(Emitter *e, bool is_64bit, Reg rd, int imm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    mov %s, #%d\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd], imm);
    } else {
        if (imm >= 0) {
            uint32_t op = is_64bit ? 0xd2800000 : 0x52800000;
            emit_u32(e, op | ((imm & 0xffff) << 5) | reg_enc(rd));
        } else {
            uint32_t op = is_64bit ? 0x92800000 : 0x12800000;
            emit_u32(e, op | (((~imm) & 0xffff) << 5) | reg_enc(rd));
        }
    }
}

void emit_add_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn, Reg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    add %s, %s, %s\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd],
                is_64bit ? reg_names_64[rn] : reg_names_32[rn],
                is_64bit ? reg_names_64[rm] : reg_names_32[rm]);
    } else {
        uint32_t op = is_64bit ? 0x8b000000 : 0x0b000000;
        emit_u32(e, op | (reg_enc(rm) << 16) | (reg_enc(rn) << 5) | reg_enc(rd));
    }
}

void emit_sub_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn, Reg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    sub %s, %s, %s\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd],
                is_64bit ? reg_names_64[rn] : reg_names_32[rn],
                is_64bit ? reg_names_64[rm] : reg_names_32[rm]);
    } else {
        uint32_t op = is_64bit ? 0xcb000000 : 0x4b000000;
        emit_u32(e, op | (reg_enc(rm) << 16) | (reg_enc(rn) << 5) | reg_enc(rd));
    }
}

void emit_add_imm(Emitter *e, bool is_64bit, Reg rd, Reg rn, int imm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    add %s, %s, #%d\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd],
                is_64bit ? reg_names_64[rn] : reg_names_32[rn], imm);
    } else {
        if (imm >= 0) {
            uint32_t op = is_64bit ? 0x91000000 : 0x11000000;
            emit_u32(e, op | ((imm & 0xfff) << 10) | (reg_enc(rn) << 5) | reg_enc(rd));
        } else {
            uint32_t op = is_64bit ? 0xd1000000 : 0x51000000;
            emit_u32(e, op | (((-imm) & 0xfff) << 10) | (reg_enc(rn) << 5) | reg_enc(rd));
        }
    }
}

void emit_sub_imm(Emitter *e, bool is_64bit, Reg rd, Reg rn, int imm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    sub %s, %s, #%d\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd],
                is_64bit ? reg_names_64[rn] : reg_names_32[rn], imm);
    } else {
        if (imm >= 0) {
            uint32_t op = is_64bit ? 0xd1000000 : 0x51000000;
            emit_u32(e, op | ((imm & 0xfff) << 10) | (reg_enc(rn) << 5) | reg_enc(rd));
        } else {
            uint32_t op = is_64bit ? 0x91000000 : 0x11000000;
            emit_u32(e, op | (((-imm) & 0xfff) << 10) | (reg_enc(rn) << 5) | reg_enc(rd));
        }
    }
}

void emit_mul_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn, Reg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    mul %s, %s, %s\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd],
                is_64bit ? reg_names_64[rn] : reg_names_32[rn],
                is_64bit ? reg_names_64[rm] : reg_names_32[rm]);
    } else {
        uint32_t op = is_64bit ? 0x9b007c00 : 0x1b007c00;
        emit_u32(e, op | (reg_enc(rm) << 16) | (reg_enc(rn) << 5) | reg_enc(rd));
    }
}

void emit_sdiv_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn, Reg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    sdiv %s, %s, %s\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd],
                is_64bit ? reg_names_64[rn] : reg_names_32[rn],
                is_64bit ? reg_names_64[rm] : reg_names_32[rm]);
    } else {
        uint32_t op = is_64bit ? 0x9ac00c00 : 0x1ac00c00;
        emit_u32(e, op | (reg_enc(rm) << 16) | (reg_enc(rn) << 5) | reg_enc(rd));
    }
}

void emit_neg_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    neg %s, %s\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd],
                is_64bit ? reg_names_64[rn] : reg_names_32[rn]);
    } else {
        uint32_t op = is_64bit ? 0xcb0003e0 : 0x4b0003e0;
        emit_u32(e, op | (reg_enc(rn) << 16) | reg_enc(rd));
    }
}

void emit_mvn_reg(Emitter *e, bool is_64bit, Reg rd, Reg rn) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    mvn %s, %s\n",
                is_64bit ? reg_names_64[rd] : reg_names_32[rd],
                is_64bit ? reg_names_64[rn] : reg_names_32[rn]);
    } else {
        uint32_t op = is_64bit ? 0xaa2003e0 : 0x2a2003e0;
        emit_u32(e, op | (reg_enc(rn) << 16) | reg_enc(rd));
    }
}

void emit_cmp_reg(Emitter *e, bool is_64bit, Reg rn, Reg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    cmp %s, %s\n",
                is_64bit ? reg_names_64[rn] : reg_names_32[rn],
                is_64bit ? reg_names_64[rm] : reg_names_32[rm]);
    } else {
        uint32_t op = is_64bit ? 0xeb00001f : 0x6b00001f;
        emit_u32(e, op | (reg_enc(rm) << 16) | (reg_enc(rn) << 5));
    }
}

void emit_cmp_imm(Emitter *e, bool is_64bit, Reg rn, int imm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    cmp %s, #%d\n",
                is_64bit ? reg_names_64[rn] : reg_names_32[rn], imm);
    } else {
        if (imm >= 0) {
            uint32_t op = is_64bit ? 0xf100001f : 0x7100001f;
            emit_u32(e, op | ((imm & 0xfff) << 10) | (reg_enc(rn) << 5));
        } else {
            uint32_t op = is_64bit ? 0xb100001f : 0x3100001f;
            emit_u32(e, op | (((-imm) & 0xfff) << 10) | (reg_enc(rn) << 5));
        }
    }
}

void emit_cset(Emitter *e, Reg rd, Cond cond) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    cset %s, %s\n", reg_names_32[rd], cond_names[cond]);
    } else {
        emit_u32(e, 0x1a9f07e0 | (invert_cond(cond) << 12) | reg_enc(rd));
    }
}

void emit_str_imm(Emitter *e, int size, Reg rt, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    str %s, [%s, #%d]\n",
                size == 8 ? reg_names_64[rt] : reg_names_32[rt],
                reg_names_64[rn], offset);
    } else {
        if (offset >= 0) {
            if (size == 4) {
                emit_u32(e, 0xb9000000 | ((offset / 4) << 10) | (reg_enc(rn) << 5) | reg_enc(rt));
            } else {
                emit_u32(e, 0xf9000000 | ((offset / 8) << 10) | (reg_enc(rn) << 5) | reg_enc(rt));
            }
        } else {
            // Unscaled signed immediate store (9-bit signed offset)
            emit_u32(e, (size == 8 ? 0xf8000000 : 0xb8000000) | ((offset & 0x1ff) << 12) | (reg_enc(rn) << 5) | reg_enc(rt));
        }
    }
}

void emit_ldr_imm(Emitter *e, int size, Reg rt, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    ldr %s, [%s, #%d]\n",
                size == 8 ? reg_names_64[rt] : reg_names_32[rt],
                reg_names_64[rn], offset);
    } else {
        if (offset >= 0) {
            if (size == 4) {
                emit_u32(e, 0xb9400000 | ((offset / 4) << 10) | (reg_enc(rn) << 5) | reg_enc(rt));
            } else {
                emit_u32(e, 0xf9400000 | ((offset / 8) << 10) | (reg_enc(rn) << 5) | reg_enc(rt));
            }
        } else {
            // Unscaled signed immediate load (9-bit signed offset)
            emit_u32(e, (size == 8 ? 0xf8400000 : 0xb8400000) | ((offset & 0x1ff) << 12) | (reg_enc(rn) << 5) | reg_enc(rt));
        }
    }
}

void emit_str_preindex(Emitter *e, int size, Reg rt, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    str %s, [%s, #%d]!\n",
                size == 8 ? reg_names_64[rt] : reg_names_32[rt],
                reg_names_64[rn], offset);
    } else {
        uint32_t op = size == 8 ? 0xf8000c00 : 0xb8000c00;
        emit_u32(e, op | ((offset & 0x1ff) << 12) | (reg_enc(rn) << 5) | reg_enc(rt));
    }
}

void emit_ldr_postindex(Emitter *e, int size, Reg rt, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    ldr %s, [%s], #%d\n",
                size == 8 ? reg_names_64[rt] : reg_names_32[rt],
                reg_names_64[rn], offset);
    } else {
        uint32_t op = size == 8 ? 0xf8400400 : 0xb8400400;
        emit_u32(e, op | ((offset & 0x1ff) << 12) | (reg_enc(rn) << 5) | reg_enc(rt));
    }
}

void emit_fstr_imm(Emitter *e, FloatReg rt, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    str %s, [%s, #%d]\n",
                float_reg_names[rt], reg_names_64[rn], offset);
    } else {
        if (offset >= 0) {
            emit_u32(e, 0xbd000000 | ((offset / 4) << 10) | (reg_enc(rn) << 5) | float_reg_enc(rt));
        } else {
            // Unscaled signed immediate float store (9-bit signed offset)
            emit_u32(e, 0xbc000000 | ((offset & 0x1ff) << 12) | (reg_enc(rn) << 5) | float_reg_enc(rt));
        }
    }
}

void emit_fldr_imm(Emitter *e, FloatReg rt, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    ldr %s, [%s, #%d]\n",
                float_reg_names[rt], reg_names_64[rn], offset);
    } else {
        if (offset >= 0) {
            emit_u32(e, 0xbd400000 | ((offset / 4) << 10) | (reg_enc(rn) << 5) | float_reg_enc(rt));
        } else {
            // Unscaled signed immediate float load (9-bit signed offset)
            emit_u32(e, 0xbc400000 | ((offset & 0x1ff) << 12) | (reg_enc(rn) << 5) | float_reg_enc(rt));
        }
    }
}

void emit_fstr_preindex(Emitter *e, FloatReg rt, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    str %s, [%s, #%d]!\n",
                float_reg_names[rt], reg_names_64[rn], offset);
    } else {
        emit_u32(e, 0xbc000c00 | ((offset & 0x1ff) << 12) | (reg_enc(rn) << 5) | float_reg_enc(rt));
    }
}

void emit_fldr_postindex(Emitter *e, FloatReg rt, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    ldr %s, [%s], #%d\n",
                float_reg_names[rt], reg_names_64[rn], offset);
    } else {
        emit_u32(e, 0xbc400400 | ((offset & 0x1ff) << 12) | (reg_enc(rn) << 5) | float_reg_enc(rt));
    }
}

void emit_stp_preindex(Emitter *e, Reg rt1, Reg rt2, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    stp %s, %s, [%s, #%d]!\n",
                reg_names_64[rt1], reg_names_64[rt2], reg_names_64[rn], offset);
    } else {
        emit_u32(e, 0xa9800000 | (((offset / 8) & 0x7f) << 15) | (reg_enc(rt2) << 10) | (reg_enc(rn) << 5) | reg_enc(rt1));
    }
}

void emit_ldp_postindex(Emitter *e, Reg rt1, Reg rt2, Reg rn, int offset) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    ldp %s, %s, [%s], #%d\n",
                reg_names_64[rt1], reg_names_64[rt2], reg_names_64[rn], offset);
    } else {
        emit_u32(e, 0xa8c00000 | (((offset / 8) & 0x7f) << 15) | (reg_enc(rt2) << 10) | (reg_enc(rn) << 5) | reg_enc(rt1));
    }
}

void emit_fadd(Emitter *e, FloatReg rd, FloatReg rn, FloatReg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    fadd %s, %s, %s\n",
                float_reg_names[rd], float_reg_names[rn], float_reg_names[rm]);
    } else {
        emit_u32(e, 0x1e202800 | (float_reg_enc(rm) << 16) | (float_reg_enc(rn) << 5) | float_reg_enc(rd));
    }
}

void emit_fsub(Emitter *e, FloatReg rd, FloatReg rn, FloatReg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    fsub %s, %s, %s\n",
                float_reg_names[rd], float_reg_names[rn], float_reg_names[rm]);
    } else {
        emit_u32(e, 0x1e203800 | (float_reg_enc(rm) << 16) | (float_reg_enc(rn) << 5) | float_reg_enc(rd));
    }
}

void emit_fmul(Emitter *e, FloatReg rd, FloatReg rn, FloatReg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    fmul %s, %s, %s\n",
                float_reg_names[rd], float_reg_names[rn], float_reg_names[rm]);
    } else {
        emit_u32(e, 0x1e200800 | (float_reg_enc(rm) << 16) | (float_reg_enc(rn) << 5) | float_reg_enc(rd));
    }
}

void emit_fdiv(Emitter *e, FloatReg rd, FloatReg rn, FloatReg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    fdiv %s, %s, %s\n",
                float_reg_names[rd], float_reg_names[rn], float_reg_names[rm]);
    } else {
        emit_u32(e, 0x1e201800 | (float_reg_enc(rm) << 16) | (float_reg_enc(rn) << 5) | float_reg_enc(rd));
    }
}

void emit_fneg(Emitter *e, FloatReg rd, FloatReg rn) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    fneg %s, %s\n", float_reg_names[rd], float_reg_names[rn]);
    } else {
        emit_u32(e, 0x1e214000 | (float_reg_enc(rn) << 5) | float_reg_enc(rd));
    }
}

void emit_fcmp(Emitter *e, FloatReg rn, FloatReg rm) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    fcmp %s, %s\n", float_reg_names[rn], float_reg_names[rm]);
    } else {
        emit_u32(e, 0x1e202000 | (float_reg_enc(rm) << 16) | (float_reg_enc(rn) << 5));
    }
}

void emit_fcvt_d_s(Emitter *e, int d_idx, FloatReg sn) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    fcvt d%d, %s\n", d_idx, float_reg_names[sn]);
    } else {
        emit_u32(e, 0x1e22c000 | (float_reg_enc(sn) << 5) | (uint32_t)d_idx);
    }
}

void emit_fmov(Emitter *e, FloatReg rd, FloatReg rn) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    fmov %s, %s\n", float_reg_names[rd], float_reg_names[rn]);
    } else {
        emit_u32(e, 0x1e204000 | (float_reg_enc(rn) << 5) | float_reg_enc(rd));
    }
}

void emit_b_label(Emitter *e, int label_id) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    b .L.label.%d\n", label_id);
    } else {
        emit_u32(e, 0x14000000);
        e->relocs[e->reloc_count++] = (Reloc){ label_id, e->size - 4, false, 0 };
    }
}

void emit_b_cond_label(Emitter *e, Cond cond, int label_id) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    b%s .L.label.%d\n", cond_names[cond], label_id);
    } else {
        emit_u32(e, 0x54000000 | cond_code(cond));
        e->relocs[e->reloc_count++] = (Reloc){ label_id, e->size - 4, true, cond };
    }
}

// We will implement dynamic resolution of extern functions in emitter later
#include <dlfcn.h>
static void emit_mov_u64(Emitter *e, Reg rd, uint64_t val) {
    uint16_t w0 = val & 0xffff;
    uint16_t w1 = (val >> 16) & 0xffff;
    uint16_t w2 = (val >> 32) & 0xffff;
    uint16_t w3 = (val >> 48) & 0xffff;

    // movz rd, #w0
    emit_u32(e, 0xd2800000 | (0 << 21) | ((uint32_t)w0 << 5) | reg_enc(rd));
    // movk rd, #w1, lsl #16
    emit_u32(e, 0xf2800000 | (1 << 21) | ((uint32_t)w1 << 5) | reg_enc(rd));
    // movk rd, #w2, lsl #32
    emit_u32(e, 0xf2800000 | (2 << 21) | ((uint32_t)w2 << 5) | reg_enc(rd));
    // movk rd, #w3, lsl #48
    emit_u32(e, 0xf2800000 | (3 << 21) | ((uint32_t)w3 << 5) | reg_enc(rd));
}

void emit_bl(Emitter *e, const char *func_name) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    bl %s\n", func_name);
    } else {
        void *addr = dlsym(RTLD_DEFAULT, func_name);
        printf("[DEBUG] emit_bl: function = %s, addr = %p\n", func_name, addr);
        fflush(stdout);
        if (addr) {
            uint64_t val = (uint64_t)addr;
            emit_mov_u64(e, REG_X16, val);
            emit_u32(e, 0xd63f0200); // blr x16
        } else {
            emit_u32(e, 0x94000000); // bl 0
            e->sym_relocs[e->sym_reloc_count++] = (SymbolReloc){ xstrdup(func_name), e->size - 4 };
        }
    }
}

void emit_sxtw(Emitter *e, Reg rd, Reg rn) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    sxtw %s, %s\n", reg_names_64[rd], reg_names_32[rn]);
    } else {
        emit_u32(e, 0x93407c00 | (reg_enc(rn) << 5) | reg_enc(rd));
    }
}

void emit_float_data(Emitter *e, float val) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    .float %f\n", val);
    } else {
        union {
            float f;
            uint32_t u;
        } u;
        u.f = val;
        emit_u32(e, u.u);
    }
}

void emit_string_data(Emitter *e, const char *str) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    .asciz \"");
        for (const char *s = str; *s; s++) {
            if (*s == '\n') {
                fprintf(e->out_file, "\\n");
            } else {
                fputc(*s, e->out_file);
            }
        }
        fprintf(e->out_file, "\"\n");
        fprintf(e->out_file, "    .p2align 2\n");
    } else {
        for (const char *s = str; *s; ) {
            char c = *s++;
            if (c == '\\' && *s) {
                char next = *s++;
                switch (next) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '\\': c = '\\'; break;
                    case '"': c = '"'; break;
                    default:
                        if (e->size >= e->capacity) {
                            e->capacity = e->capacity == 0 ? 4096 : e->capacity * 2;
                            e->buffer = realloc(e->buffer, e->capacity);
                        }
                        e->buffer[e->size++] = '\\';
                        c = next;
                        break;
                }
            }
            if (e->size >= e->capacity) {
                e->capacity = e->capacity == 0 ? 4096 : e->capacity * 2;
                e->buffer = realloc(e->buffer, e->capacity);
            }
            e->buffer[e->size++] = c;
        }
        // Null terminator
        if (e->size >= e->capacity) {
            e->capacity = e->capacity == 0 ? 4096 : e->capacity * 2;
            e->buffer = realloc(e->buffer, e->capacity);
        }
        e->buffer[e->size++] = 0;

        // Align to 4 bytes
        while (e->size % 4 != 0) {
            if (e->size >= e->capacity) {
                e->capacity = e->capacity == 0 ? 4096 : e->capacity * 2;
                e->buffer = realloc(e->buffer, e->capacity);
            }
            e->buffer[e->size++] = 0;
        }
    }
}

void emit_ldr_pc_relative_float(Emitter *e, FloatReg rt, int label_id) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    ldr %s, .L.label.%d\n", float_reg_names[rt], label_id);
    } else {
        emit_u32(e, 0x1c000000 | float_reg_enc(rt));
        e->relocs[e->reloc_count++] = (Reloc){ label_id, e->size - 4, false, (Cond)-2 }; // -2 indicates float PC-relative LDR
    }
}

void emit_adr(Emitter *e, Reg rd, int label_id) {
    if (!e->binary_mode) {
        fprintf(e->out_file, "    adr %s, .L.label.%d\n", reg_names_64[rd], label_id);
    } else {
        emit_u32(e, 0x10000000 | reg_enc(rd));
        e->relocs[e->reloc_count++] = (Reloc){ label_id, e->size - 4, false, (Cond)-1 }; // -1 indicates ADR
    }
}
