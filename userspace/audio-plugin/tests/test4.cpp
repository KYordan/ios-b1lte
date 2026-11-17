/* Host test of xnu-overlay B2LAudio.h (lots 1-3): simulated PERIC1 pins + bit-banged I2C to two CS35L40,
 * simulated ABOX (SYSPOWER, RDMA0 status), DMA ring, service thread steps.  Build: see run.sh. */
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include <map>
#define B2L_AUDIO_HOST_TEST
typedef uintptr_t vm_offset_t; typedef size_t vm_size_t;
typedef int IOSimpleLock; static IOSimpleLock the_lock; static int locked;
static IOSimpleLock *IOSimpleLockAlloc(){ return &the_lock; }
static void IOSimpleLockLock(IOSimpleLock*){ assert(!locked); locked=1; }
static void IOSimpleLockUnlock(IOSimpleLock*){ assert(locked); locked=0; }
static uint32_t bootarg, maxarg; static bool bootarg_set, maxarg_set;
static bool PE_parse_boot_argn(const char*n,void*v,int){
  if(!strcmp(n,"b2l_audio")){ if(bootarg_set) memcpy(v,&bootarg,4); return bootarg_set; }
  if(!strcmp(n,"b2l_audio_max")){ if(maxarg_set) memcpy(v,&maxarg,4); return maxarg_set; }
  return false; }
static void IODelay(unsigned){}
static char logbuf[1<<17];
extern "C" void b2l_diag_record(const char*f,...){va_list a;va_start(a,f);size_t n=strlen(logbuf);int k=vsnprintf(logbuf+n,sizeof logbuf-n,f,a);va_end(a);assert(k<640);}
static uint8_t mem[9][0x4000]; static uint64_t pa[9]={0x15860000,0x1a240000,0x10430000,0x15850000,0x10800000,0x10830000,0x18c00000,0x18c50000,0x18c60000}; static unsigned mapped[9];
static uint8_t i2cmem[0x1000];
extern "C" vm_offset_t ml_io_map(vm_offset_t p, vm_size_t){ for(int i=0;i<9;i++) if(pa[i]==p){mapped[i]++; return (vm_offset_t)mem[i];} if(p==0x109a0000) return (vm_offset_t)i2cmem; assert(!"unexpected map"); return 0;}
#define P1 mem[5]
static uint32_t raw(int i,uint32_t off){uint32_t v;memcpy(&v,&mem[i][off],4);return v;}
static void setraw(int i,uint32_t off,uint32_t v){memcpy(&mem[i][off],&v,4);}
/* ---- simulated amplifiers ---- */
static std::map<uint32_t,uint32_t> R[2]; static bool pup[2], pdn[2], noclk, inject_err; static uint32_t revs[2], otpid=3, otpmem[32];
static unsigned writes[2], pupcount, amp_en, loud_vol[2];
static uint32_t vol_allowed;     /* besides mute: the one PCM volume code allowed now (0 = none) */
static bool clocked(){ return !noclk && (raw(7,0x510)&5)==5 && (raw(7,0x38)&8) && (raw(6,0x1830)&0x1ff)==95 && (raw(8,0x20)&0xffff)==0x2222 && raw(7,0x514)==0x1b4df7df; }
static void amp_write(int a,uint32_t r,uint32_t v){
  writes[a]++;
  if(r==0x10010){ if(v&(1u<<24)) pup[a]=false; if(v&(1u<<23)) pdn[a]=false; if(v&0x800281c0u) inject_err=false; return; }   /* W1C */
  if(r==0x2014){ bool was=R[a][r]&1; if((v&1)&&!was&&clocked()){pup[a]=true;pupcount++;} if(!(v&1)&&was) pdn[a]=true; }
  if(r==0x2018){ assert((v&0x30)==0); if(v&1){ assert(a==0); amp_en++; } }   /* boost never; output stage only on bot */
  if(r==0x6000){ uint32_t vol=(v>>3)&0x7ff; if(vol!=0x4cf){ assert(a==0 && vol==vol_allowed); assert(R[0][0x2018]&1); loud_vol[a]++; } }
  R[a][r]=v; }
static uint32_t regval(int a,uint32_t r){ if(r==0) return 0x35a40; if(r==4) return revs[a]; if(r==0x1001c) return 2;
  if(r==0x10) return otpid; if(r>=0x400&&r<0x480) return otpmem[(r-0x400)/4];
  if(r==0x10010) return (pup[a]?1u<<24:0)|(pdn[a]?1u<<23:0)|(inject_err&&a==0?0x80000000u:0);
  auto it=R[a].find(r); return it==R[a].end()?0:it->second; }
static bool force_sda_low, slave_sda_low; static int last_scl=1,last_sda=1;
static enum {IDLE,RX,TX,IGN} st=IDLE; static int bit, nbytes; static uint8_t sh; static bool rw; static int sel; static uint32_t reg, dat; static uint32_t txv; static int txb;
static unsigned violations; static bool unpowered;
static bool amp_alive(int a){if(unpowered)return false;uint32_t con=raw(5,0x100),d=raw(5,0x104);unsigned pin=a?2:1;return ((con>>(pin*4))&0xf)==1 && ((d>>pin)&1);}
static int m_low(unsigned pin){uint32_t con=raw(5,0xe0),d=raw(5,0xe4);unsigned fn=(con>>(pin*4))&0xf; if(fn==1&&((d>>pin)&1)) violations++; return fn==1&&!((d>>pin)&1);}
static int scl(){return !m_low(6);} static int sda(){return !(m_low(7)||slave_sda_low||force_sda_low);}
static void edges(){
  for(int a=0;a<2;a++) if(!amp_alive(a)){R[a].clear();pup[a]=pdn[a]=false;}
  int c=scl(),d=sda();
  if(c&&last_scl&&d!=last_sda){ if(!d){st=RX;bit=0;nbytes=0;sh=0;slave_sda_low=false;} else { if(st==RX&&!rw&&nbytes==9) amp_write(sel,reg,dat); st=IDLE;slave_sda_low=false;} }
  else if(c&&!last_scl){
    if(st==RX&&bit<8){sh=(uint8_t)((sh<<1)|d);bit++;}
    else if(st==TX&&bit==8){ if(d){st=IGN;} }
  } else if(!c&&last_scl){
    if(st==RX){
      if(bit==8){
        if(nbytes==0){ sel=(sh>>1)-0x40; rw=sh&1; if(sel<0||sel>1||!amp_alive(sel)){st=IGN;slave_sda_low=false;} else slave_sda_low=true; }
        else { if(nbytes<=4) reg=(reg<<8)|sh; else dat=(dat<<8)|sh; slave_sda_low=true; }
        if(st!=IGN){nbytes++;bit=9;}
      } else if(bit==9){ slave_sda_low=false; if(rw){st=TX;txv=regval(sel,reg);txb=0;bit=0; slave_sda_low=!((txv>>31)&1);} else {bit=0;sh=0;} if(!rw&&nbytes==1){reg=0;dat=0;} }
    } else if(st==TX){
      if(bit<8){ bit++; txb++; if(bit<8) slave_sda_low=!((txv>>(31-txb))&1); else slave_sda_low=false; }
      else if(bit==8){ bit=0; slave_sda_low=!((txv>>(31-txb))&1); }
    }
  }
  last_scl=c; last_sda=sda();
}
/* ---- simulated ABOX: SYSPOWER status follows CTRL; RDMA0 status from a frame counter ---- */
static bool nosys, rdma_stall, auto_adv=true; static uint64_t rframes; static unsigned dma_allocs, dma_frees; static int32_t ring[65280/4];
static void *b2l_aud_dma_alloc(size_t len,int32_t **va,uint64_t *p){ assert(len==sizeof ring); dma_allocs++; memset(ring,0x5a,sizeof ring); *va=ring; *p=0x84b8c000; return ring; }
static void b2l_aud_dma_free(void *m){ assert(m==ring); dma_frees++; }
static unsigned cleans; static void b2l_aud_dma_clean(int32_t*,size_t){ cleans++; }
static uint32_t b2l_aud_rd(volatile uint8_t *b, uint32_t off){
  if(b==P1 && off==0xe4){ uint32_t v=raw(5,0xe4)&~0xc0u; return v|(scl()<<6)|(sda()<<7); }
  if(b==mem[7] && off==0x14) return nosys?0:(raw(7,0x10)&1);
  if(b==mem[7] && off==0x1030){ bool en=raw(7,0x1000)&1, run=(raw(7,0x510)&5)==5;
    if(!en) return 0;
    if(run&&!rdma_stall&&auto_adv) rframes+=7;
    uint32_t by=(uint32_t)(rframes%4080)*16, off2=by-by%4080;
    return (1u<<31)|((off2/16)<<12)|(by%4080); }
  uint32_t v; memcpy(&v,(const void*)(b+off),4); return v; }
static void b2l_aud_wr(volatile uint8_t *b, uint32_t off, uint32_t v){ memcpy((void*)(b+off),&v,4); if(b==P1) edges(); }
#include "B2LAudio.h"
static uint8_t snap[3][0x4000];
static void setup(){
  memset(mem,0,sizeof mem); memset(logbuf,0,sizeof logbuf);
  setraw(1,0x20c4,0x10000000); setraw(4,0x2030,0x00200000); setraw(4,0x3010,0x003f0002);
  setraw(4,0x2110,0x10000000); setraw(4,0x2114,0x10000000); setraw(4,0x3074,1);
  setraw(5,0xe8,0x1234); setraw(5,0xe0,0x00500000); setraw(5,0x100,0x00007000); setraw(0,0x1904,0x1); memset(mapped,0,sizeof mapped);
  setraw(1,0x201c,0x00200000); setraw(6,0x2010,0x00200000); setraw(6,0x3020,0x003f0002); setraw(6,0x205c,0x00200000); setraw(6,0x304c,0x003f0002);
  /* 0212's values */
  setraw(6,0x120,0xa02d0110); setraw(6,0x12c,0x5eff); setraw(6,0x1800,1); setraw(6,0x1830,0x5); setraw(6,0x1010,0x0);
  setraw(7,0x4,0x02000100); setraw(7,0x510,0x01000010); setraw(7,0x1000,0x00400000); setraw(8,0x20,0); setraw(6,0x2024,0x00200000); setraw(0,0x1920,0x860);
  st=IDLE; slave_sda_low=false; last_scl=last_sda=1; force_sda_low=false; violations=0; noclk=false; unpowered=false; inject_err=false;
  revs[0]=revs[1]=0xa0; R[0].clear(); R[1].clear(); pup[0]=pup[1]=pdn[0]=pdn[1]=false; writes[0]=writes[1]=0; pupcount=0; amp_en=0; loud_vol[0]=loud_vol[1]=0;
  nosys=rdma_stall=false; auto_adv=true; rframes=0; dma_allocs=dma_frees=0; cleans=0; vol_allowed=0; bootarg_set=maxarg_set=false;
  for(int i=0;i<32;i++) otpmem[i]=0x9e3779b9u*(i+1); otpmem[1]=0x1501; otpmem[2]=(otpmem[2]&0xffff0000u)|0x300;
  memset(&b2l_aud_svc,0,sizeof b2l_aud_svc);
  memcpy(snap[0],mem[6],0x4000); memcpy(snap[1],mem[7],0x4000); memcpy(snap[2],mem[8],0x4000);
}
static bool aud_untouched(){ return !memcmp(snap[0],mem[6],0x4000) && !memcmp(snap[1],mem[7],0x4000) && !memcmp(snap[2],mem[8],0x4000); }
static bool pins_back(){ return raw(5,0xe0)==0x00500000 && raw(5,0x100)==0x00007000 && violations==0; }
static void done_undone(){ assert(aud_untouched() && pins_back() && R[0].empty() && R[1].empty()); }
static int16_t pcm[2*1024];
static void fill(int16_t l,int16_t r){ for(int i=0;i<1024;i++){pcm[2*i]=l;pcm[2*i+1]=r;} }
static void steps(unsigned n){ for(unsigned i=0;i<n;i++){ rframes+=480; b2l_aud_svc_step(&b2l_aud_svc); assert(!locked); } }
int main(){
  // ---- level 7 (default): silent path kept for iOS, no tone at all ----
  setup(); b2l_audio_probe();
  assert(strstr(logbuf,"amp-otp bot@40 otpid=3 result=ok"));
  assert(strstr(logbuf,"1010:00000000>00000ff0 1000:00400000>00401b21") && strstr(logbuf,"| rdma-moving"));
  assert(strstr(logbuf,"beep-amp bot@40") && strstr(logbuf,"| ready"));
  assert(strstr(logbuf,"beep tone=0 played=0 ms=0 vol_up=0 vol_down=9") && strstr(logbuf,"| kept-for-ios"));
  assert(strstr(logbuf,"ios-ready /dev/hakios-audio -> rdma0 ring_pa=84b8c000 (4080 frames)") && strstr(logbuf,"max=-48/8 dB"));
  assert(strstr(logbuf,"uaif1-verdict amps-clocked-by-uaif1 | abox-cmu-pins-undone=0 kept-for-ios=1") && strstr(logbuf,"amps-kept"));
  assert(!strstr(logbuf,"amps-off") && loud_vol[0]==0 && loud_vol[1]==0 && dma_frees==0 && b2l_aud_svc.running);
  for(int f=0;f<4080;f++) assert(!ring[4*f]&&!ring[4*f+1]&&!ring[4*f+2]&&!ring[4*f+3]);   /* silent ring */
  assert(raw(7,0x510)==0x01000015 && (raw(7,0x1000)&1) && amp_alive(0) && amp_alive(1) && !(R[0][0x2018]&1) && !(R[0][0x2014]&1));
  puts(strstr(logbuf,"B2L AUDIO: beep-path"));
  // ---- the service ----
  auto_adv=false; logbuf[0]=0;
  steps(5); assert(!b2l_aud_svc.amp_on && !strstr(logbuf,"ios-play"));
  fill(1000,3000); assert(b2l_audio_sink(pcm,1024)==1024);                    /* first write: ahead of the reader by LEAD */
  { uint32_t r=b2l_aud_rpos(mem[7]); uint32_t w0=(r+1024)%4080; assert(ring[4*w0+1]==(1000+3000)*32768 && ring[4*w0]==0 && ring[4*w0+2]==0); }
  vol_allowed=0x800-48; steps(1);
  assert(b2l_aud_svc.amp_on && strstr(logbuf,"ios-play amp=on vol=7d0 below=0 muted=0 in=1024 lost=0 resyncs=1") && loud_vol[0]==1);
  assert((R[0][0x2014]&1) && (R[0][0x2018]&1) && R[0][0x4c00]==8 && !(R[1][0x2018]&1));
  // writer keeps up: no resync, nothing lost
  for(int k=0;k<20;k++){ fill(-200,100); assert(b2l_audio_sink(pcm,480)==480); steps(1); }
  assert(b2l_aud_svc.resyncs==1 && b2l_aud_svc.lost==0);
  // a too-big write: capped, the rest lost
  fill(5,5); uint32_t t=b2l_audio_sink(pcm,1024); t+=b2l_audio_sink(pcm,1024); t+=b2l_audio_sink(pcm,1024); t+=b2l_audio_sink(pcm,1024);
  assert(t<4096 && b2l_aud_svc.lost==4096-t);
  // what was played is zeroed
  steps(20); { uint32_t r=b2l_aud_rpos(mem[7]); for(uint32_t i=300;i<2000;i++){ uint32_t f=(r+4080-i)%4080; assert(ring[4*f+1]==0); } }
  // volume from iOS while playing
  b2l_audio_volume_set(80,0); vol_allowed=0x800-48-80; steps(1); assert(strstr(logbuf,"ios-volume below=80 muted=0 vol=780") && (R[0][0x6000]>>3)==0x780);
  b2l_audio_volume_set(0,1); steps(1); assert((R[0][0x6000]>>3&0x7ff)==0x4cf);
  b2l_audio_volume_set(0,0); vol_allowed=0x800-48; steps(1);
  // 3 s of silence: off
  steps(300); assert(!b2l_aud_svc.amp_on && strstr(logbuf,"ios-stop amp=off") && !(R[0][0x2014]&1) && !(R[0][0x2018]&1) && (R[0][0x6000]>>3&0x7ff)==0x4cf);
  // silence written does not wake the amplifier
  fill(0,0); b2l_audio_sink(pcm,480); steps(2); assert(!b2l_aud_svc.amp_on);
  // the writer fell behind (a gap): resync
  uint64_t rs=b2l_aud_svc.resyncs; steps(50); fill(7,7); b2l_audio_sink(pcm,256); assert(b2l_aud_svc.resyncs==rs+1);
  steps(1); assert(b2l_aud_svc.amp_on);
  // amplifier error: off 30 s, then again on sound; 3 errors: off until reboot
  for(int k=1;k<=3;k++){
    inject_err=true; steps(10); assert(!b2l_aud_svc.amp_on && b2l_aud_svc.faults==(unsigned)k);
    char e[64]; snprintf(e,sizeof e,"ios-fault amp-error value=80000000 faults=%d",k); assert(strstr(logbuf,e));
    fill(9,9); b2l_audio_sink(pcm,256); steps(1); assert(!b2l_aud_svc.amp_on);    /* cooling down */
    for(int j=0;j<300&&!b2l_aud_svc.amp_on;j++){ fill(9,9); b2l_audio_sink(pcm,128); steps(10); }
    if(k<3) assert(b2l_aud_svc.amp_on);
  }
  assert(b2l_aud_svc.dead && !b2l_aud_svc.amp_on && strstr(logbuf,"speaker-off-until-reboot") && !(R[0][0x2014]&1));
  // ceiling: b2l_audio_max below 24 is raised to 24
  { uint32_t m=b2l_aud_svc.max_below; b2l_aud_svc.max_below=24; b2l_aud_svc.below=0; b2l_aud_svc.muted=0; assert(b2l_aud_vol_code(&b2l_aud_svc)==0x7e8); b2l_aud_svc.max_below=m; }
  setup(); maxarg_set=true; maxarg=0; b2l_audio_probe(); assert(b2l_aud_svc.max_below==24); maxarg=96; setup(); maxarg_set=true; maxarg=96; b2l_audio_probe(); assert(b2l_aud_svc.max_below==96);
  // ---- not kept: RDMA does not move -> everything undone, as in 0213 ----
  setup(); rdma_stall=true; b2l_audio_probe();
  assert(strstr(logbuf,"rdma-NOT-moving") && strstr(logbuf,"| undone rdma-idle-ring-freed=1 abox-path-restored=1") && !b2l_aud_svc.running && strstr(logbuf,"amps-off pins-restored=1"));
  setraw(0,0x1920,0x860); done_undone();
  assert(b2l_audio_sink(pcm,10)==0);
  // not kept: amplifiers without clock
  setup(); noclk=true; b2l_audio_probe(); assert(strstr(logbuf,"| undone") && !b2l_aud_svc.running && strstr(logbuf,"amps-off pins-restored=1")); done_undone();
  // ---- level 6: the audible beep, then all undone (0213) ----
  setup(); bootarg_set=true; bootarg=6; vol_allowed=0x7b0; b2l_audio_probe();
  assert(strstr(logbuf,"beep tone=1 played=1 ms=700 vol_up=1 vol_down=0 err=00000000") && strstr(logbuf,"| undone rdma-idle-ring-freed=1 abox-path-restored=1") && loud_vol[0]==1 && !b2l_aud_svc.running);
  for(int f=0;f<4080;f++) assert(ring[4*f+1]==b2l_aud_sine[f%48]);
  done_undone();
  // ---- level 5: no path, no ring ----
  setup(); bootarg_set=true; bootarg=5; b2l_audio_probe(); assert(!strstr(logbuf,"beep") && dma_allocs==0 && amp_en==0 && strstr(logbuf,"amps-clocked-by-uaif1 | abox-cmu-pins-undone=1")); done_undone();
  // level 3 / 2 / 1 / 0
  setup(); bootarg_set=true; bootarg=3; b2l_audio_probe(); assert(writes[0]==0 && pins_back());
  setup(); bootarg_set=true; bootarg=0; b2l_audio_probe(); assert(strstr(logbuf,"off (b2l_audio=0)"));
  // bus stuck / unpowered: nothing kept
  setup(); force_sda_low=true; b2l_audio_probe(); assert(strstr(logbuf,"bus-not-idle") && !b2l_aud_svc.running && pins_back());
  setup(); unpowered=true; b2l_audio_probe(); assert(strstr(logbuf,"no-answer") && !b2l_aud_svc.running && pins_back());
  puts("OK");
}
