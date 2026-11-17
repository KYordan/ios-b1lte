#include "b2l_compat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include "fixtures.h"

struct memory { unsigned calls, acquire_calls, pc_calls, generic_calls, width;
    int fail, fail_pc, fail_generic; uint64_t address, value, arg_a, arg_b; };
static int pc(uint64_t a, void *v) {
    struct memory *m=v; m->pc_calls++;
    return m->fail_pc || a < 0x1000 || a >= UINT64_C(0x100000000) || (a&3);
}
static int read_mem(uint64_t a,unsigned w,uint64_t *out,void *v) {
    struct memory *m=v; m->calls++;m->address=a;m->width=w;
    if (m->fail || a<0x1000 || a>UINT64_C(0xffffffff)-w+1) return 1;
    *out=m->value;return 0;
}
static int acquire_mem(uint64_t a,unsigned w,uint64_t *out,void *v) {
    struct memory *m=v;m->acquire_calls++;return read_mem(a,w,out,v);
}
static uint64_t stored_at,stored_value;static unsigned stored_width;
static int write_rel(uint64_t a,unsigned w,uint64_t v,void *p) {
    struct memory *m=p;if(m->fail||a<0x1000||a>UINT64_C(0xffffffff)-w+1)return 1;stored_at=a;stored_width=w;stored_value=v;return 0;
}
static struct b2l_compat_ops ops(struct memory *m) {
    struct b2l_compat_ops o={B2L_COMPAT_MODE_STRIPPED,read_mem,acquire_mem,pc,m,NULL,write_rel};return o;
}
static struct b2l_compat_ctx ctx(void) {
    struct b2l_compat_ctx c;memset(&c,0,sizeof(c));
    for(unsigned i=0;i<31;i++)c.x[i]=UINT64_C(0x2000)+16*i;
    c.pc=0x1000;c.sp=0x8000;c.cpsr=UINT32_C(0xf0000000);return c;
}
static uint32_t word(const char *n) {
    for(size_t i=0;i<sizeof(fixtures)/sizeof(*fixtures);i++)if(!strcmp(n,fixtures[i].name))return fixtures[i].word;
    assert(!"missing fixture");return 0;
}
static void same(const struct b2l_compat_ctx *a,const struct b2l_compat_ctx *b) {
    assert(!memcmp(a->x,b->x,sizeof(a->x)));assert(a->sp==b->sp);assert(a->pc==b->pc);assert(a->cpsr==b->cpsr);
}
static uint64_t rng=UINT64_C(0x9876543210123456);
static uint64_t random64(void) {rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
static void fault(uint32_t i,struct b2l_compat_ctx c,struct b2l_compat_ops *o) {
    struct b2l_compat_ctx before=c;
    assert(b2l_compat_emulate(i,&c,o)==B2L_COMPAT_FAULT);same(&c,&before);
}
static void test_identity(void) {
    struct memory m={0};struct b2l_compat_ops o=ops(&m);
    for(size_t i=0;i<sizeof(fixtures)/sizeof(*fixtures);i++)if(fixtures[i].kind==IDENTITY) {
        struct b2l_compat_ctx c=ctx(),e=c;
        c.x[0]=e.x[0]=UINT64_C(0xabcd123456789abc); /* never guess a tag/PAC mask */
        e.pc+=4;assert(b2l_compat_emulate(fixtures[i].word,&c,&o)==B2L_COMPAT_OK);same(&c,&e);
    }
    assert(!m.calls && !m.generic_calls);
    fault(word("pacia_reg"),ctx(),NULL);
    o.mode=0;fault(word("pacia_reg"),ctx(),&o);
}
static void test_branches(void) {
    struct memory m={0};struct b2l_compat_ops o=ops(&m);
    for(size_t i=0;i<sizeof(fixtures)/sizeof(*fixtures);i++)if(fixtures[i].kind==BRANCH) {
        struct b2l_compat_ctx c=ctx(),e=c;
        uint32_t w=fixtures[i].word;unsigned rn=(w>>5)&31;
        if(w==word("retaa")||w==word("retab"))rn=30;
        if(rn==31){fault(w,c,&o);continue;}
        e.pc=c.x[rn];if((w&(1u<<21)) && w!=word("retaa") && w!=word("retab"))e.x[30]=c.pc+4;
        assert(b2l_compat_emulate(w,&c,&o)==B2L_COMPAT_OK);same(&c,&e);
    }
    struct b2l_compat_ctx c=ctx();c.x[0]=0x2001;fault(word("blraa_normal"),c,&o);
    c.x[0]=UINT64_C(0xffff000000002000);fault(word("blraa_normal"),c,&o);
    m.fail_pc=1;fault(word("retab"),ctx(),&o);
}
static void test_loads(void) {
    struct memory m={0};m.value=UINT64_C(0xfedcba9876543210);struct b2l_compat_ops o=ops(&m);
    for(unsigned key=0;key<2;key++) {
        const char *names[2][8]={{"ldraa_zero","ldraa_plus","ldraa_minus","ldraa_wbplus","ldraa_wbminus","ldraa_sp","ldraa_zr","ldraa_overlap"},{"ldrab_zero","ldrab_plus","ldrab_minus","ldrab_wbplus","ldrab_wbminus","ldrab_sp","ldrab_zr","ldrab_overlap"}};
        const int offsets[]={0,4088,-4096,8,-8,8,8,0};
        for(unsigned n=0;n<8;n++) {
            struct b2l_compat_ctx c=ctx(),e=c;c.x[1]=e.x[1]=0x6000;
            uint32_t w=word(names[key][n]);unsigned rn=(w>>5)&31,rd=w&31;
            uint64_t base=rn==31?c.sp:c.x[rn];uint64_t a=base+(uint64_t)(int64_t)offsets[n];
            e.pc+=4;if(rd!=31)e.x[rd]=m.value;
            if(w&0x800){if(rn==31)e.sp=a;else e.x[rn]=a;}
            assert(b2l_compat_emulate(w,&c,&o)==B2L_COMPAT_OK);same(&c,&e);assert(m.address==a && m.width==8);
        }
    }
    const char *names[]={"ldapr_x","ldapr_w","ldaprb","ldaprh","ldapr_sp","ldapr_zr","ldaprb_zr"};
    const unsigned widths[]={8,4,1,2,8,8,1};
    for(unsigned n=0;n<7;n++) {
        struct b2l_compat_ctx c=ctx(),e=c;unsigned rd=word(names[n])&31;
        uint64_t value=m.value;if(widths[n]!=8)value&=(UINT64_C(1)<<(8*widths[n]))-1;
        if(rd!=31)e.x[rd]=value;e.pc+=4;unsigned old=m.acquire_calls;
        assert(b2l_compat_emulate(word(names[n]),&c,&o)==B2L_COMPAT_OK);same(&c,&e);
        assert(m.acquire_calls==old+1 && m.width==widths[n]);
    }
    /* Every signed10-bit LDRA offset, both keys, writeback modes. */
    for (unsigned key=0;key<2;key++) for(unsigned wb=0;wb<2;wb++)
      for(unsigned imm=0;imm<1024;imm++) {
        uint32_t w=word("ldraa_zero") | (key<<23) | ((imm&511u)<<12) |
            ((imm&512u)<<13) | (wb<<11);
        struct b2l_compat_ctx c=ctx(),e=c;c.x[1]=e.x[1]=0x6000;
        int off=(imm>=512 ? (int)imm-1024 : (int)imm)*8;
        uint64_t a=c.x[1]+(uint64_t)(int64_t)off;e.x[0]=m.value;e.pc+=4;
        if(wb)e.x[1]=a;
        assert(b2l_compat_emulate(w,&c,&o)==B2L_COMPAT_OK);same(&c,&e);assert(m.address==a);
    }
    struct b2l_compat_ctx unaligned=ctx();unaligned.x[1]|=1;
    fault(word("ldapr_x"),unaligned,&o);fault(word("ldaprh"),unaligned,&o);
    /* Ordinary LDRA data access may be unaligned; callback owns data mapping. */
    assert(b2l_compat_emulate(word("ldraa_zero"),&unaligned,&o)==B2L_COMPAT_OK);
    struct b2l_compat_ctx edge=ctx();edge.x[1]=UINT64_MAX-3;
    fault(word("ldraa_zero"),edge,&o);fault(word("ldapr_x"),edge,&o);
    m.fail=1;fault(word("ldraa_wbplus"),ctx(),&o);fault(word("ldapr_x"),ctx(),&o);
    m.fail=0;struct b2l_compat_ctx c=ctx();c.sp=0x8008;unsigned old=m.calls;
    fault(word("ldraa_sp"),c,&o);fault(word("ldapr_sp"),c,&o);assert(m.calls==old);
    /* WB with base==destination is rejected before any callbacks. */
    fault(word("ldraa_wbplus")|1u,ctx(),&o);assert(m.calls==old);
    o.read=NULL;fault(word("ldraa_zero"),ctx(),&o);o=ops(&m);
    o.read_acquire=NULL;fault(word("ldapr_zr"),ctx(),&o);
}
static void test_generic(void) {
    struct memory m={0};struct b2l_compat_ops o=ops(&m);
    const char *names[]={"pacga","pacga_alias_n","pacga_alias_m","pacga_zr_n","pacga_zr_d"};
    for(unsigned n=0;n<5;n++) {
        struct b2l_compat_ctx c=ctx(),e=c;uint32_t w=word(names[n]);
        unsigned rn=(w>>5)&31,rm=(w>>16)&31,rd=w&31;
        uint64_t a=rn==31?0:c.x[rn],b=rm==31?c.sp:c.x[rm];
        /* B2L PACGA ABI v2: EOR Xd, Xn, Xm|SP, ROR #32; no callback. */
        if(rd!=31)e.x[rd]=a^((b>>32)|(b<<32));e.pc+=4;
        assert(b2l_compat_emulate(w,&c,&o)==B2L_COMPAT_OK);same(&c,&e);
    }
    assert(m.generic_calls==0);
}
static void test_misc_and_random(void) {
    struct memory m={0};struct b2l_compat_ops o=ops(&m);struct b2l_compat_ctx c=ctx(),before=c;
    assert(b2l_compat_emulate(0xd503201f,&c,&o)==B2L_COMPAT_UNHANDLED);same(&c,&before);
    /* Reserved zero-modifier Rn and adjacent opcodes must not be swallowed. */
    assert(b2l_compat_emulate(0xdac12000,&c,&o)==B2L_COMPAT_UNHANDLED);same(&c,&before);
    assert(b2l_compat_emulate(0xdac14800,&c,&o)==B2L_COMPAT_UNHANDLED);same(&c,&before);
    c.pc=UINT64_MAX-3;fault(word("pacibsp"),c,&o);c=ctx();c.pc|=2;fault(word("pacibsp"),c,&o);
    for(unsigned n=0;n<200000;n++) {
        struct b2l_compat_ctx a=ctx(),b;struct memory ma={0},mb={0};
        for(unsigned r=0;r<31;r++)a.x[r]=(random64()&0xfffff)&~UINT64_C(3);
        a.sp=(random64()&0xffff0);a.cpsr=(uint32_t)random64();b=a;
        ma.value=mb.value=random64();ma.fail=mb.fail=(n%17==0);ma.fail_pc=mb.fail_pc=(n%31==0);
        struct b2l_compat_ops oa=ops(&ma),ob=ops(&mb);uint32_t w;
        if(n&1)w=(uint32_t)random64();
        else {w=fixtures[n%(sizeof(fixtures)/sizeof(*fixtures))].word;
            /* Mix real operand fields, including register31 and overlap. */
            if((w&0xff000000)==0xda000000 || (w&0xff000000)==0xf8000000 || (w&0xff000000)==0xb8000000)w=(w&~0x3ffu)|((uint32_t)random64()&0x3ffu);
            else if((w&0xff000000)==0xd7000000)w=(w&~0x3ffu)|((uint32_t)random64()&0x3ffu);
        }
        enum b2l_compat_result ra=b2l_compat_emulate(w,&a,&oa),rb=b2l_compat_emulate_reference(w,&b,&ob);
        assert(ra==rb);same(&a,&b);assert(!memcmp(&ma,&mb,sizeof(ma)));
    }
}
/* FJCVTZS: integer-only decoder checked against an independent floating-point ToInt32 on the host. */
static uint64_t fp_reg[32];
static int read_d(unsigned n,uint64_t *out,void *v){(void)v;if(n>31)return 1;*out=fp_reg[n];return 0;}
static uint32_t oracle(double x,int *z){
    if(!isfinite(x)){*z=0;return 0;}
    double t=trunc(x),m=fmod(t,4294967296.0);if(m<0)m+=4294967296.0;
    *z=x==t&&t>=-2147483648.0&&t<=2147483647.0&&!(x==0&&signbit(x));
    return (uint32_t)m;
}
static void check_bits(uint64_t bits){
    double x;memcpy(&x,&bits,8);int z1,z2;uint32_t a=b2l_compat_fjcvtzs(bits,&z1),b=oracle(x,&z2);
    if(a!=b||z1!=z2){printf("fjcvtzs 0x%016llx: got %08x z%d want %08x z%d\n",(unsigned long long)bits,a,z1,b,z2);assert(0);}
}
static void test_fjcvtzs(void){
    static const double edge[]={0.0,-0.0,0.5,-0.5,1.0,-1.0,1.5,-1.5,2147483647.0,2147483648.0,-2147483648.0,-2147483649.0,
        4294967296.0,4294967297.5,-4294967297.0,9007199254740993.0,1e19,-1e19,1e300,-1e300,5e-324,-5e-324,2.2250738585072014e-308,
        0x1p63,0x1p64,0x1p84,-0x1p84,0x1p85,INFINITY,-INFINITY,NAN,2147483647.5,-2147483648.5};
    for(size_t i=0;i<sizeof(edge)/sizeof(*edge);i++){uint64_t b;memcpy(&b,&edge[i],8);check_bits(b);}
    for(unsigned n=0;n<2000000;n++){
        uint64_t r=random64();
        /* half of the cases near the int32 range, the rest anywhere (NaN payloads, subnormals included) */
        if(n&1)r=(r&UINT64_C(0x800fffffffffffff))|((uint64_t)(1023+(r>>53)%70)<<52);
        check_bits(r);
    }
    struct memory m={0};struct b2l_compat_ops o=ops(&m);o.read_d=read_d;
    double v=-3.75;memcpy(&fp_reg[1],&v,8);v=7.0;memcpy(&fp_reg[0],&v,8);
    struct b2l_compat_ctx c=ctx(),e=c;c.x[0]=e.x[0]=UINT64_MAX;
    assert(b2l_compat_emulate(word("fjcvtzs"),&c,&o)==B2L_COMPAT_OK);
    e.x[0]=UINT32_C(0xfffffffd);e.pc+=4;e.cpsr=0;same(&c,&e);                 /* inexact: Z=0, NZCV cleared, W zero-extends */
    c=ctx();e=c;c.cpsr=e.cpsr=UINT32_C(0x9000000f);
    assert(b2l_compat_emulate(word("fjcvtzs_hi"),&c,&o)==B2L_COMPAT_OK);
    e.x[30]=7;e.pc+=4;e.cpsr=UINT32_C(0x4000000f);same(&c,&e);                 /* exact: Z=1, other cpsr bits kept */
    c=ctx();e=c;assert(b2l_compat_emulate(word("fjcvtzs_zr"),&c,&o)==B2L_COMPAT_OK);e.pc+=4;e.cpsr=(e.cpsr&0x0fffffff)|(fp_reg[31]==0?0x40000000:0);same(&c,&e);
    o.read_d=NULL;fault(word("fjcvtzs"),ctx(),&o);                              /* no FP accessor: refused, ctx unchanged */
    assert(b2l_compat_emulate(0x1e7e0400,&c,&o)==B2L_COMPAT_UNHANDLED);       /* neighbouring opcode not swallowed */
    puts("FJCVTZS: edges + 2000000 random doubles vs host ToInt32, Z flag, WZR, missing accessor PASS");
}
/* LDAPUR* and STLUR* (encodings checked with llvm-mc +rcpc-immo), SB, DIT and SSBS controls */
static void test_v84(void) {
    struct memory m={0};struct b2l_compat_ops o=ops(&m);struct b2l_compat_ctx c,e;
    /* ldapur w0,[x1] */
    m.value=UINT64_C(0xfedcba9887654321);c=ctx();e=c;
    assert(b2l_compat_emulate(0x99400020,&c,&o)==B2L_COMPAT_OK);assert(m.address==c.x[1]&&m.width==4&&m.acquire_calls==1);
    e.x[0]=0x87654321;e.pc+=4;same(&c,&e);
    /* ldapursh w0,[x1]: sign to 32 bits, upper half zero */
    m.value=0x8001;c=ctx();e=c;assert(b2l_compat_emulate(0x59c00020,&c,&o)==B2L_COMPAT_OK);e.x[0]=0xffff8001;e.pc+=4;same(&c,&e);
    /* ldapursh x0,[x1] (opc 10): sign to 64 bits */
    c=ctx();e=c;assert(b2l_compat_emulate(0x59800020,&c,&o)==B2L_COMPAT_OK);e.x[0]=UINT64_C(0xffffffffffff8001);e.pc+=4;same(&c,&e);
    /* ldapursw x2,[x3,#-4] (size 10 opc 10, imm9 = -4) */
    m.value=0x80000000;c=ctx();e=c;uint32_t w=0x99800000u|(0x1fcu<<12)|(3u<<5)|2u;
    assert(b2l_compat_emulate(w,&c,&o)==B2L_COMPAT_OK);assert(m.address==c.x[3]-4);e.x[2]=UINT64_C(0xffffffff80000000);e.pc+=4;same(&c,&e);
    /* ldapur x4,[sp,#16] */
    m.value=UINT64_C(0x1122334455667788);c=ctx();e=c;w=0xd9400000u|(16u<<12)|(31u<<5)|4u;
    assert(b2l_compat_emulate(w,&c,&o)==B2L_COMPAT_OK);assert(m.address==c.sp+16);e.x[4]=m.value;e.pc+=4;same(&c,&e);
    /* stlur w5,[x6,#8] and stlurh wzr,[x6] */
    c=ctx();e=c;w=0x99000000u|(8u<<12)|(6u<<5)|5u;assert(b2l_compat_emulate(w,&c,&o)==B2L_COMPAT_OK);
    assert(stored_at==c.x[6]+8&&stored_width==4&&stored_value==c.x[5]);e.pc+=4;same(&c,&e);
    c=ctx();w=0x59000000u|(6u<<5)|31u;assert(b2l_compat_emulate(w,&c,&o)==B2L_COMPAT_OK);assert(stored_width==2&&stored_value==0);
    /* misaligned, SP misaligned, failing store, missing store op, unallocated encodings */
    c=ctx();c.x[1]+=2;fault(0x99400020,c,&o);
    c=ctx();c.sp+=8;fault(0xd9400000u|(31u<<5),c,&o);
    m.fail=1;fault(0x99000020,ctx(),&o);m.fail=0;
    o.write_release=NULL;fault(0x99000020,ctx(),&o);o=ops(&m);
    c=ctx();assert(b2l_compat_emulate(0xd9800020,&c,&o)==B2L_COMPAT_UNHANDLED);   /* size 11 opc 10 */
    assert(b2l_compat_emulate(0x99c00020,&c,&o)==B2L_COMPAT_UNHANDLED);           /* size 10 opc 11 */
    assert(b2l_compat_emulate(0x99400420,&c,&o)==B2L_COMPAT_UNHANDLED);           /* bits 11:10 != 00 */
    /* sb, msr dit/ssbs #imm and Xt: no effect but the PC */
    const uint32_t nops[]={0xd50330ff,0xd503415f,0xd503405f,0xd503403f,0xd503413f,0xd51b42a0,0xd51b42c3};
    for(unsigned k=0;k<sizeof(nops)/sizeof(*nops);k++){c=ctx();e=c;assert(b2l_compat_emulate(nops[k],&c,&o)==B2L_COMPAT_OK);e.pc+=4;same(&c,&e);}
    /* mrs x5,dit / mrs x7,ssbs read 0; mrs xzr leaves registers */
    c=ctx();e=c;assert(b2l_compat_emulate(0xd53b42a5,&c,&o)==B2L_COMPAT_OK);e.x[5]=0;e.pc+=4;same(&c,&e);
    c=ctx();e=c;assert(b2l_compat_emulate(0xd53b42c7,&c,&o)==B2L_COMPAT_OK);e.x[7]=0;e.pc+=4;same(&c,&e);
    c=ctx();e=c;assert(b2l_compat_emulate(0xd53b42df,&c,&o)==B2L_COMPAT_OK);e.pc+=4;same(&c,&e);
    /* neighbours are not swallowed: S3_3_C4_C2_4 and _7, SB's neighbour (dsb #0 form) */
    assert(b2l_compat_emulate(0xd51b4280,&c,&o)==B2L_COMPAT_UNHANDLED&&b2l_compat_emulate(0xd53b42e0,&c,&o)==B2L_COMPAT_UNHANDLED);
    assert(b2l_compat_emulate(0xd50330df,&c,&o)==B2L_COMPAT_UNHANDLED);
    puts("v8.4/8.5: LDAPUR/LDAPURS/STLUR (sign, offset, SP, faults, unallocated), SB, DIT, SSBS PASS");
}
int main(void) {test_v84();test_fjcvtzs();test_identity();test_branches();test_loads();test_generic();test_misc_and_random();puts("compat core fixtures + 200000 C/assembly differential cases passed");return 0;}
