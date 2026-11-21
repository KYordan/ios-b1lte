/* SPDX-License-Identifier: BSD-2-Clause */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../uefi/XnuLoader/Progress.h"
int main(void) {
    const size_t width=1440,height=3040,stride=width*4,size=height*stride;
    unsigned char *p=malloc(size+8192); assert(p); memset(p,0x5a,size+8192);
    struct b2l_handoff_info info={0};
    B2lProgressPrepare((uintptr_t)p+4096,stride,width,height,&info);
    assert(info.panel_base==(uintptr_t)p+4096+(height/3)*stride+((width-1184)/2)*4);
    for(size_t i=0;i<6;++i) {
        B2lProgressSet(&info,i,0x00ff00);
        for(size_t y=0;y<64;++y)for(size_t x=0;x<128;++x) {
            uint32_t pixel; memcpy(&pixel,(void *)(uintptr_t)(info.panel_base+(i*96+y)*stride+x*4),4);
            assert(pixel==0x00ff00);
        }
    }
    for(size_t i=0;i<4096;++i) assert(p[i]==0x5a && p[4096+size+i]==0x5a);
    B2lProgressPrepare((uintptr_t)p+4096,stride,500,100,&info);
    assert(!info.panel_base); B2lProgressSet(&info,0,0x00ff00);
    free(p);
    /* S10e panel (1080 x 2280): narrow 768 px panel, every write inside the frame. */
    const size_t w2=1080,h2=2280,s2=w2*4,z2=h2*s2;
    p=malloc(z2+8192); assert(p); memset(p,0x5a,z2+8192);
    B2lProgressPrepare((uintptr_t)p+4096,s2,w2,h2,&info);
    assert(info.panel_base==(uintptr_t)p+4096+(h2/3)*s2+((w2-B2L_PROGRESS_MIN_W)/2)*4);
    for(size_t y=0;y<h2;++y){ /* nothing written left or right of the 768 px panel */
        const unsigned char *row=p+4096+y*s2;
        for(size_t x=0;x<(w2-B2L_PROGRESS_MIN_W)/2;++x) assert(row[x*4]==0x5a && row[(w2-1-x)*4]==0x5a);
    }
    for(size_t i=0;i<4096;++i) assert(p[i]==0x5a && p[4096+z2+i]==0x5a);
    free(p); puts("Direct framebuffer progress bounds and stage tests passed");
}
