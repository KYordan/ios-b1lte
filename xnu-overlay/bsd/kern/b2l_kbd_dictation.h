/* SPDX-License-Identifier: BSD-2-Clause
 * B1L-0164: keyboard freezes. Dictation availability flips endlessly on this port; every flip makes the keyboard dock
 * (UISystemKeyboardDockController) reload its glyph images on the app's main thread, so key presses wait behind it.
 * Two UIKit methods in dyld_shared_cache_arm64e.03 (23G90) are turned off in the file, before launchd:
 *   -[UIDictationController dictationConnnectionDidChangeAvailability:]  -> ret
 *   -[UIDictationController dictationEnabled]                            -> mov w0,#0 ; ret
 * Each word is written only if the subcache UUID matches and the bytes are the original (PAC hint or translated NOP)
 * or already patched. Kill switch: b2l_kbd_dictation=0.
 */
static int b2l_kbd_rw(vnode_t vp,enum uio_rw rw,uint64_t off,void *buf,int n,vfs_context_t ctx)
{
    int resid=0;int e=vn_rdwr(rw,vp,(caddr_t)buf,n,(off_t)off,UIO_SYSSPACE,rw==UIO_WRITE?IO_SYNC:0,vfs_context_ucred(ctx),&resid,current_proc());
    return e?e:(resid?EIO:0);
}
static void b2l_kbd_dictation_patch(vfs_context_t ctx)
{
    int on=1;PE_parse_boot_argn("b2l_kbd_dictation",&on,sizeof(on));
    if(!on){printf("B2L KBD: dictation patch off (boot-arg)\n");return;}
    static const uint8_t uuid[16]={0xc8,0x87,0x13,0xfe,0xf7,0xcf,0x3c,0xf2,0xa0,0xdf,0xb5,0x29,0x69,0xeb,0xec,0x72};
    static const struct{uint64_t off;uint32_t old1,want0,want1;int two;}p[2]={
        {0x2a5fb2c,0xd10103ff,0xd65f03c0,0,0},   /* dictationConnnectionDidChangeAvailability: */
        {0x2b9be48,0xa9bb67fa,0x52800000,0xd65f03c0,1}, /* dictationEnabled */
    };
    vnode_t vp=NULLVP;int e=vnode_lookup("/System/Library/Caches/com.apple.dyld/dyld_shared_cache_arm64e.03",0,&vp,ctx);
    if(e||!vp){printf("B2L KBD: subcache lookup error=%d\n",e);return;}
    uint8_t hdr[104];int patched=0,already=0;
    if(vnode_vtype(vp)!=VREG||vnode_mount(vp)!=vnode_mount(rootvnode)){e=EXDEV;goto out;}
    if((e=b2l_kbd_rw(vp,UIO_READ,0,hdr,sizeof(hdr),ctx)))goto out;
    if(memcmp(hdr,"dyld_v1",7)||memcmp(hdr+88,uuid,16)){e=ESTALE;goto out;}
    for(int i=0;i<2;i++){
        uint32_t w[2];
        if((e=b2l_kbd_rw(vp,UIO_READ,p[i].off,w,8,ctx)))goto out;
        int done=w[0]==p[i].want0&&(!p[i].two||w[1]==p[i].want1);
        if(done){already++;continue;}
        if((w[0]!=0xd503237f&&w[0]!=0xd503201f)||w[1]!=p[i].old1){e=EILSEQ;printf("B2L KBD: unexpected words %08x %08x at 0x%llx\n",w[0],w[1],p[i].off);goto out;}
        uint32_t n[2]={p[i].want0,p[i].two?p[i].want1:w[1]};
        if(p[i].two){if((e=b2l_kbd_rw(vp,UIO_WRITE,p[i].off,n,8,ctx)))goto out;}
        else if((e=b2l_kbd_rw(vp,UIO_WRITE,p[i].off,n,4,ctx)))goto out;
        uint32_t r[2];if((e=b2l_kbd_rw(vp,UIO_READ,p[i].off,r,8,ctx)))goto out;
        if(r[0]!=n[0]||r[1]!=n[1]){e=EIO;goto out;}
        patched++;
    }
    e=VNOP_FSYNC(vp,MNT_WAIT,ctx);
out:
    vnode_put(vp);
    printf("B2L KBD: dictation patch patched=%d already=%d error=%d\n",patched,already,e);
}
