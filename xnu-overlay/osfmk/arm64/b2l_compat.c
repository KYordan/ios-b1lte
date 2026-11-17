/* Independently implemented decoder for a deliberately stripped-pointer ABI.
 * Architectural encodings verified against LLVM assembly and public ISA
 * descriptions. No third-party emulator implementation is incorporated. */
#include "b2l_compat.h"
#include <stddef.h>

static uint64_t
reg_zr(const struct b2l_compat_ctx *c, unsigned r)
{
    return r == 31 ? UINT64_C(0) : c->x[r];
}

static uint64_t
reg_sp(const struct b2l_compat_ctx *c, unsigned r)
{
    return r == 31 ? c->sp : c->x[r];
}

static void
write_zr(struct b2l_compat_ctx *c, unsigned r, uint64_t value)
{
    if (r != 31) c->x[r] = value;
}

static int
is_identity_pauth(uint32_t i)
{
    /* PACIA/B/DA/DB and AUTIA/B/DA/DB, with register/SP modifier. */
    if ((i & UINT32_C(0xffffe000)) == UINT32_C(0xdac10000)) return 1;
    /* Zero-modifier variants require the reserved Rn field to be31. */
    if ((i & UINT32_C(0xffffe3e0)) == UINT32_C(0xdac123e0)) return 1;
    if ((i & UINT32_C(0xffffffe0)) == UINT32_C(0xdac143e0) ||
        (i & UINT32_C(0xffffffe0)) == UINT32_C(0xdac147e0)) return 1;
    /* PAC/AUT hint aliases, including XPACLRI. */
    switch (i) {
    case UINT32_C(0xd50320ff):
    case UINT32_C(0xd503211f): case UINT32_C(0xd503215f):
    case UINT32_C(0xd503219f): case UINT32_C(0xd50321df):
    case UINT32_C(0xd503231f): case UINT32_C(0xd503233f):
    case UINT32_C(0xd503235f): case UINT32_C(0xd503237f):
    case UINT32_C(0xd503239f): case UINT32_C(0xd50323bf):
    case UINT32_C(0xd50323df): case UINT32_C(0xd50323ff):
        return 1;
    default: return 0;
    }
}

uint32_t
b2l_compat_fjcvtzs(uint64_t bits, int *z)
{
    unsigned sign = (unsigned)(bits >> 63), exponent = (unsigned)(bits >> 52) & 0x7ffu;
    uint64_t mantissa = bits & ((UINT64_C(1) << 52) - 1u), magnitude;
    uint32_t low;
    int exact;
    *z = 0;
    if (exponent == 0x7ffu) return 0;                         /* NaN, infinities */
    if (exponent == 0) { *z = mantissa == 0 && !sign; return 0; }  /* +0 exact; -0 and subnormals not */
    mantissa |= UINT64_C(1) << 52;
    if (exponent >= 1075u) {                                  /* integral value: mantissa << shift */
        unsigned shift = exponent - 1075u;
        low = shift >= 32u ? 0u : (uint32_t)(mantissa << shift);
        if (shift > 10u) return sign ? 0u - low : low;        /* >= 2^63: out of int32 range */
        magnitude = mantissa << shift;
        exact = 1;
    } else {
        unsigned shift = 1075u - exponent;
        magnitude = shift >= 64u ? 0 : mantissa >> shift;
        exact = shift < 64u && (mantissa & ((UINT64_C(1) << shift) - 1u)) == 0;
        low = (uint32_t)magnitude;
    }
    *z = exact && (magnitude < (UINT64_C(1) << 31) || (sign && magnitude == (UINT64_C(1) << 31)));
    return sign ? 0u - low : low;
}

enum b2l_compat_result
b2l_compat_emulate_reference(uint32_t i, struct b2l_compat_ctx *c,
                   const struct b2l_compat_ops *o)
{
    enum { IDENTITY, BRANCH, GENERIC, LOAD_AUTH, LOAD_ACQUIRE, JSCVT, NOP, READ_ZERO, RCPC2 } kind;
    unsigned rn = (i >> 5) & 31u, rd = i & 31u;
    unsigned rm = (i >> 16) & 31u, width = 8, opc = 0;
    int link = 0, wb = 0;
    uint64_t next, value = 0, address = 0, a = 0, b = 0;
    struct b2l_compat_ctx candidate;

    if (c == NULL) return B2L_COMPAT_FAULT;
    if (is_identity_pauth(i)) {
        kind = IDENTITY;
    } else if ((i & UINT32_C(0xffe0fc00)) == UINT32_C(0x9ac03000)) {
        kind = GENERIC;
    } else if ((i & UINT32_C(0xfffff800)) == UINT32_C(0xd71f0800) ||
               (i & UINT32_C(0xfffff800)) == UINT32_C(0xd73f0800)) {
        kind = BRANCH;
        link = (i & UINT32_C(0x00200000)) != 0;
    } else if ((i & UINT32_C(0xfffff81f)) == UINT32_C(0xd61f081f) ||
               (i & UINT32_C(0xfffff81f)) == UINT32_C(0xd63f081f)) {
        kind = BRANCH;
        link = (i & UINT32_C(0x00200000)) != 0;
    } else if (i == UINT32_C(0xd65f0bff) || i == UINT32_C(0xd65f0fff)) {
        kind = BRANCH;
        rn = 30;
    } else if ((i & UINT32_C(0xff200400)) == UINT32_C(0xf8200400)) {
        kind = LOAD_AUTH;
        wb = (i & UINT32_C(0x800)) != 0;
        /* Architecture permits constrained-unpredictable behavior here.
         * Never choose an outcome that silently destroys the base. */
        if (wb && rn == rd && rn != 31) return B2L_COMPAT_FAULT;
    } else if ((i & UINT32_C(0x3ffffc00)) == UINT32_C(0x38bfc000)) {
        kind = LOAD_ACQUIRE;
        width = 1u << (i >> 30);
    } else if ((i & UINT32_C(0xfffffc00)) == UINT32_C(0x1e7e0000)) {
        kind = JSCVT;                /* FJCVTZS Wd, Dn: Rn names a SIMD/FP register */
    } else if (i == UINT32_C(0xd50330ff) ||                          /* SB (v8.5): a speculation barrier only */
               (i & UINT32_C(0xfffff0ff)) == UINT32_C(0xd503405f) || /* MSR DIT, #imm (v8.4) */
               (i & UINT32_C(0xfffff0ff)) == UINT32_C(0xd503403f) || /* MSR SSBS, #imm (v8.5) */
               (i & UINT32_C(0xffffffe0)) == UINT32_C(0xd51b42a0) || /* MSR DIT|SSBS, Xt (S3_3_C4_C2_5/6) */
               (i & UINT32_C(0xffffffe0)) == UINT32_C(0xd51b42c0)) {
        /* Timing and speculation controls with no architectural effect on data: none of them exists here. */
        kind = NOP;
    } else if ((i & UINT32_C(0xffffffe0)) == UINT32_C(0xd53b42a0) || /* MRS Xt, DIT|SSBS: reads as clear */
               (i & UINT32_C(0xffffffe0)) == UINT32_C(0xd53b42c0)) {
        kind = READ_ZERO;
    } else if ((i & UINT32_C(0x3f200c00)) == UINT32_C(0x19000000)) {
        /* LDAPUR* and STLUR* (v8.4 LRCPC2): size 011001 opc 0 imm9 00 Rn Rt */
        width = 1u << (i >> 30);
        opc = (i >> 22) & 3u;
        if ((width == 8 && opc >= 2u) || (width == 4 && opc == 3u)) return B2L_COMPAT_UNHANDLED;   /* unallocated */
        kind = RCPC2;
    } else {
        return B2L_COMPAT_UNHANDLED;
    }

    if (o == NULL || o->mode != B2L_COMPAT_MODE_STRIPPED ||
        o->validate_pc == NULL || (c->pc & 3u) != 0 ||
        c->pc > UINT64_MAX - 4u) return B2L_COMPAT_FAULT;
    next = c->pc + 4u;
    if (kind == BRANCH) next = reg_zr(c, rn);
    if ((next & 3u) != 0 || o->validate_pc(next, o->opaque) != 0)
        return B2L_COMPAT_FAULT;

    /* Snapshot operands before any destination register changes. */
    if (kind == GENERIC) {
        a = reg_zr(c, rn);           /* Rn31 is XZR, unlike Rm31. */
        b = reg_sp(c, rm);
        /* B2L PACGA ABI v2: identical to the static/native rewrite
         * EOR Xd, Xn, Xm, ROR #32, so trapped and rewritten sites agree. */
        value = a ^ ((b >> 32) | (b << 32));
    } else if (kind == LOAD_AUTH || kind == LOAD_ACQUIRE) {
        address = reg_sp(c, rn);
        if (rn == 31 && (address & 15u) != 0) return B2L_COMPAT_FAULT;
        if (kind == LOAD_AUTH) {
            /* LDRA authenticates BASE, then adds signed scaled10-bit offset.
             * Identity authentication leaves that base untouched here. */
            unsigned imm10 = ((i >> 12) & 511u) | ((i >> 13) & 512u);
            int offset = (imm10 >= 512u ? (int)imm10 - 1024 : (int)imm10) * 8;
            address += (uint64_t)(int64_t)offset;
            if (address > UINT64_MAX - 7u || o->read == NULL ||
                o->read(address, 8, &value, o->opaque) != 0)
                return B2L_COMPAT_FAULT;
        } else {
            if ((address & (width - 1u)) != 0 ||
                address > UINT64_MAX - (width - 1u) ||
                o->read_acquire == NULL ||
                o->read_acquire(address, width, &value, o->opaque) != 0)
                return B2L_COMPAT_FAULT;
            if (width != 8) value &= (UINT64_C(1) << (width * 8u)) - 1u;
        }
    }
    if (kind == RCPC2) {
        unsigned imm9 = (i >> 12) & 511u;
        address = reg_sp(c, rn) + (uint64_t)(int64_t)(imm9 >= 256u ? (int)imm9 - 512 : (int)imm9);
        if (rn == 31 && (c->sp & 15u) != 0) return B2L_COMPAT_FAULT;
        /* No FEAT_LSE2 here: an unaligned acquire/release access is an alignment fault, as on this processor. */
        if ((address & (width - 1u)) != 0 || address > UINT64_MAX - (width - 1u)) return B2L_COMPAT_FAULT;
        if (opc == 0) {
            if (o->write_release == NULL || o->write_release(address, width, reg_zr(c, rd), o->opaque) != 0)
                return B2L_COMPAT_FAULT;
        } else {
            if (o->read_acquire == NULL || o->read_acquire(address, width, &value, o->opaque) != 0)
                return B2L_COMPAT_FAULT;
            unsigned bits = width * 8u;
            if (width != 8) value &= (UINT64_C(1) << bits) - 1u;
            if (opc >= 2u && width != 8 && (value >> (bits - 1u)) & 1u) value |= ~UINT64_C(0) << bits;   /* sign */
            if (opc == 3u) value &= UINT32_MAX;                                  /* Wt destination */
        }
    }
    int z = 0;
    if (kind == JSCVT) {
        uint64_t bits;
        if (o->read_d == NULL || o->read_d(rn, &bits, o->opaque) != 0) return B2L_COMPAT_FAULT;
        value = b2l_compat_fjcvtzs(bits, &z);          /* Wd write zero-extends */
    }
    candidate = *c;
    candidate.pc = next;
    if (kind == JSCVT) {
        write_zr(&candidate, rd, value);
        candidate.cpsr = (c->cpsr & UINT32_C(0x0fffffff)) | (z ? UINT32_C(0x40000000) : 0);
    }
    if (kind == BRANCH && link) candidate.x[30] = c->pc + 4u;
    if (kind == GENERIC || kind == LOAD_AUTH || kind == LOAD_ACQUIRE || kind == READ_ZERO ||
        (kind == RCPC2 && opc != 0))
        write_zr(&candidate, rd, value);
    if (kind == LOAD_AUTH && wb) {
        if (rn == 31) candidate.sp = address;
        else candidate.x[rn] = address;
    }
    *c = candidate;
    return B2L_COMPAT_OK;
}

#ifdef B2L_COMPAT_USE_ASM
_Static_assert(offsetof(struct b2l_compat_ctx, sp) == 248, "asm sp layout");
_Static_assert(offsetof(struct b2l_compat_ctx, pc) == 256, "asm pc layout");
_Static_assert(offsetof(struct b2l_compat_ctx, cpsr) == 264, "asm cpsr layout");
_Static_assert(sizeof(struct b2l_compat_ctx) == 272, "asm context size");
extern int b2l_compat_fast_candidate(uint32_t,
    const struct b2l_compat_ctx *, struct b2l_compat_ctx *);
#endif

enum b2l_compat_result
b2l_compat_emulate(uint32_t i, struct b2l_compat_ctx *c,
                   const struct b2l_compat_ops *o)
{
#ifdef B2L_COMPAT_USE_ASM
    struct b2l_compat_ctx candidate;
    if (c == NULL) return B2L_COMPAT_FAULT;
    if (b2l_compat_fast_candidate(i, c, &candidate)) {
        if (o == NULL || o->mode != B2L_COMPAT_MODE_STRIPPED ||
            o->validate_pc == NULL || (c->pc & 3u) != 0 ||
            c->pc > UINT64_MAX - 4u || (candidate.pc & 3u) != 0 ||
            o->validate_pc(candidate.pc, o->opaque) != 0)
            return B2L_COMPAT_FAULT;
        *c = candidate;
        return B2L_COMPAT_OK;
    }
#endif
    return b2l_compat_emulate_reference(i, c, o);
}
