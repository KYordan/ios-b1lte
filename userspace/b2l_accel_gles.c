// SPDX-License-Identifier: BSD-2-Clause
// B1L-0032 (GPU G6, option 1): put iOS 26's window server on OpenGL ES.
//
// iOS 26 removed the fallback HakiOS relied on in iOS 16: CA::WindowServer::AccelServer::renderer()
// (QuartzCore 23G90, 0x183f3bdcc) calls CAMetalContextCreate() and, with no Metal device, gives up
// (cbz x0 at 0x183f3bdf0) instead of trying [EAGLContext initWithAPI:3]. Its success path is generic:
// it takes the OGL::Context new_metal_context() returns and builds the Renderer around it.
// So, in backboardd only (this library is inserted there by the kernel), two patches:
//   1. the cbz at 0x183f3bdf0 becomes a nop: the success path runs with a nil device;
//   2. new_metal_context() (0x18415a0c0) jumps to b2l_gles_context(), which makes an EAGLContext
//      (HakiOS's bridge, eagl.c, Mesa panfrost behind it) and returns QuartzCore's own GLESContext from
//      gles_new_context() (0x184109d38: { u32 type = 3; void *eagl } -> calloc 0xb10, GLESContext(eagl)).
// Every patched word is compared with what 23G90 has first; any mismatch and nothing is written.
// Addresses are the shared cache's unslid ones; the slide comes from _dyld_get_shared_cache_range().
#include <mach/mach.h>

#include <libkern/OSCacheControl.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <ptrauth.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

/* B1L-0034 safety net: if the GLES renderer is not made within 8 s (or fails), this boot's session
 * id goes to MARKER and backboardd is stopped; launchd starts it again, and with the marker for this
 * boot the patches are not made: the window server stays on its software renderer. */
#define MARKER "/private/var/tmp/b2l-gl-fallback"   /* B1L-0036: world-writable */
static char b2l_session[64];
static volatile int b2l_made_ok;
static void b2l_mark_and_stop(const char *why);

extern const void *_dyld_get_shared_cache_range(size_t *length);

#define CACHE_BASE              0x180000000UL
#define A_RENDERER_CBZ          0x183f3bdf0UL   /* cbz x0, 0x183f3bf34 */
#define W_RENDERER_CBZ          0xb4000a20u
#define A_RENDERER_BL_METAL     0x183f3bde8UL   /* bl CAMetalContextCreate (0x184219998) */
#define W_RENDERER_BL_METAL     0x940b76ecu
#define W_MOV_X0_0              0xd2800000u     /* B1L-0035: Metal probing hung in IOServiceGetMatchingServices */
#define A_NEW_METAL_CONTEXT     0x18415a0c0UL   /* CA::OGL::new_metal_context(...) */
#define A_GLES_NEW_CONTEXT      0x184109d38UL   /* static gles_new_context(void*, void*, unsigned) */
#define W_GLES_NEW_CONTEXT      0xb9400008u     /* ldr w8, [x0] */
static const uint32_t w_new_metal[4] = { 0xd503237f, 0xa9ba6ffc, 0xa90167fa, 0xa9025ff8 };

/*
 * B1L-0041: CA::OGL::GLESContext::make_buffer_current() (0x1840d2414) callocs the display's OGL::Surface itself and only
 * prepends it to the GLES context's own list (+0xb00); iOS 26's Context::push_surface() then asserts the surface is one of
 * the context's tracked ones ("found != slist->end ()", ogl-context.cpp 0x4a2) and aborts on the first frame. Every other
 * GLES path calls Context::track_surface() (create_null_surface, create_surface_with_properties); this one was left out
 * because nothing of Apple's runs it any more. The five words at 0x1840d2620
 *     ldr x0,[x19,#0xb00]; mov x1,x21; bl _x_list_prepend; str x0,[x19,#0xb00]; b 0x1840d2660
 * become  ldr x16,#12; blr x16; b 0x1840d2660; .quad b2l_mbc_tramp  -- the trampoline tracks the surface, then does
 * the same prepend.
 */
static void b2l_say(const char *format, ...) __attribute__((format(printf, 1, 2)));
#define A_PUSH_NOTFOUND         0x183f131e4UL   /* cbnz x8, 0x183f13220 in Context::push_surface */
#define W_PUSH_NOTFOUND         0xb50001e8u
#define A_MBC_SITE              0x1840d2620UL
#define A_TRACK_SURFACE         0x1840d8c58UL   /* CA::OGL::Context::track_surface(Surface*) */
#define A_X_LIST_PREPEND        0x183f6ec78UL   /* x_list_prepend(x_list*, void*) */
static const uint32_t w_mbc_site[5] = { 0xf9458260, 0xaa1503e1, 0x97fa7194, 0xf9058260, 0x1400000c };
static unsigned mbc_tracked;

static uintptr_t slide;

void b2l_mbc_track(void *ctx, void *surface);
void
b2l_mbc_track(void *ctx, void *surface)
{
	void (*track)(void *, void *) = (void (*)(void *, void *))
	    ptrauth_sign_unauthenticated((void *)(A_TRACK_SURFACE + slide), ptrauth_key_function_pointer, 0);
	void *(*prepend)(void *, void *) = (void *(*)(void *, void *))
	    ptrauth_sign_unauthenticated((void *)(A_X_LIST_PREPEND + slide), ptrauth_key_function_pointer, 0);
	void **list = (void **)((char *)ctx + 0xb00);

	track(ctx, surface);
	*list = prepend(*list, surface);
	if (mbc_tracked++ < 4) {
		b2l_say("b2l-gl: display surface %p tracked by the GLES context %p (make_buffer_current)", surface, ctx);
	}
}

/* x19 = the GLESContext, x21 = the new Surface at the patched site; both callee-saved, kept for the code after it */
void b2l_mbc_tramp(void);
__asm__(
	".text\n"
	".p2align 2\n"
	".globl _b2l_mbc_tramp\n"
	"_b2l_mbc_tramp:\n"
	"	stp x29, x30, [sp, #-16]!\n"
	"	mov x29, sp\n"
	"	mov x0, x19\n"
	"	mov x1, x21\n"
	"	bl _b2l_mbc_track\n"
	"	ldp x29, x30, [sp], #16\n"
	"	ret\n");
static unsigned made;

static void
b2l_say(const char *format, ...)
{
	char line[300];
	va_list ap;
	struct timespec ts;
	clock_gettime(CLOCK_UPTIME_RAW, &ts);
	int at = snprintf(line, sizeof(line), "[%ld.%02ld] ", (long)ts.tv_sec, ts.tv_nsec / 10000000);
	va_start(ap, format);
	vsnprintf(line + at, sizeof(line) - (size_t)at, format, ap);
	va_end(ap);
	(void)sysctlbyname("kern.hakios.say", NULL, NULL, line, strlen(line));
}

/* Replaces CA::OGL::new_metal_context: arguments ignored (the device is nil here). */
static void *
b2l_watchdog(void *unused)
{
	(void)unused;
	for (int i = 0; i < 1200 && !b2l_made_ok; i++) usleep(100000);
	if (!b2l_made_ok) b2l_mark_and_stop("no GLES renderer 120 s after backboardd started");
	return NULL;
}

static void
b2l_mark_and_stop(const char *why)
{
	int fd = open(MARKER, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) { (void)write(fd, b2l_session, strlen(b2l_session)); close(fd); }
	b2l_say("b2l-gl: %s: back to the software renderer for this boot (marker %d)", why, fd >= 0);
	abort();
}

static void *
b2l_gles_context(void)
{

	Class eagl = objc_getClass("EAGLContext");
	id context = nil;
	b2l_say("b2l-gl: renderer asked; EAGLContext class %p", (void *)eagl);
	if (eagl != Nil) {
		id a = ((id (*)(Class, SEL))objc_msgSend)(eagl, sel_registerName("alloc"));
		b2l_say("b2l-gl: EAGLContext alloc %p; initWithAPI:3 ...", (void *)a);
		context = ((id (*)(id, SEL, unsigned long))objc_msgSend)(a, sel_registerName("initWithAPI:"), 3);
		b2l_say("b2l-gl: initWithAPI:3 -> %p", (void *)context);
	}
	if (context == nil) {
		b2l_mark_and_stop("EAGLContext initWithAPI:3 failed");
		return NULL;
	}
	struct { uint32_t type, pad; void *eagl; } desc = { 3, 0, (void *)context };
	void *(*make)(void *, void *, unsigned) = (void *(*)(void *, void *, unsigned))
	    ptrauth_sign_unauthenticated((void *)(A_GLES_NEW_CONTEXT + slide), ptrauth_key_function_pointer, 0);
	b2l_say("b2l-gl: gles_new_context ...");
	void *gles = make(&desc, NULL, 0);
	if (gles == NULL) b2l_mark_and_stop("gles_new_context returned NULL");
	b2l_made_ok = 1;
	made++;
	b2l_say("b2l-gl: window server renderer #%u: GLESContext %p over EAGLContext %p", made, gles, (void *)context);
	return gles;
}

static int
b2l_write_code(uintptr_t at, const void *bytes, size_t n)
{
	vm_address_t page = at & ~(vm_address_t)PAGE_MASK;
	vm_size_t len = ((at + n + PAGE_MASK) & ~(uintptr_t)PAGE_MASK) - page;
	kern_return_t kr = vm_protect(mach_task_self(), page, len, FALSE, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
	if (kr != KERN_SUCCESS) return (int)kr;
	memcpy((void *)at, bytes, n);
	kr = vm_protect(mach_task_self(), page, len, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
	sys_icache_invalidate((void *)at, n);
	return (int)kr;
}

/* B1L-0039: a crash of backboardd once the GLES path is in (CoreAnimation's abort() when a GL answer is wrong, a fault in
 * Mesa) marks this boot as well, so that launchd's next backboardd stays on the software renderer instead of crashing the
 * same way again and again (B1L-0038: seven minutes of restarts). Async-signal-safe calls only. */
static void
b2l_crash_mark(int sig)
{
	int fd = open(MARKER, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) { (void)write(fd, b2l_session, strlen(b2l_session)); close(fd); }
	signal(sig, SIG_DFL);
	raise(sig);
}

__attribute__((constructor)) static void
b2l_accel_gles_install(void)
{
	size_t length = 0;
	{
		/* B1L-0036: kern.boottime (always readable) identifies this boot */
		struct timeval bt = { 0, 0 };
		size_t bl = sizeof(bt);
		if (sysctlbyname("kern.boottime", &bt, &bl, NULL, 0) == 0)
			snprintf(b2l_session, sizeof(b2l_session), "%ld.%06d", (long)bt.tv_sec, (int)bt.tv_usec);
	}
	{
		char seen[64] = "";
		int fd = open(MARKER, O_RDONLY);
		if (fd >= 0) { ssize_t n = read(fd, seen, sizeof(seen) - 1); close(fd); if (n > 0) seen[n] = 0; }
		if (b2l_session[0] && strcmp(seen, b2l_session) == 0) {
			b2l_say("b2l-gl: GLES failed earlier this boot: window server left on the software renderer");
			return;
		}
	}
	const void *cache = _dyld_get_shared_cache_range(&length);
	if (cache == NULL) { b2l_say("b2l-gl: no shared cache"); return; }
	slide = (uintptr_t)cache - CACHE_BASE;
	const uint32_t *cbz = (const uint32_t *)(A_RENDERER_CBZ + slide);
	const uint32_t *nmc = (const uint32_t *)(A_NEW_METAL_CONTEXT + slide);
	const uint32_t *gnc = (const uint32_t *)(A_GLES_NEW_CONTEXT + slide);
	/* B1L-0033: the kernel's PAC translation turns pacibsp (d503237f) into nop (d503201f) in the shared
	 * cache, so the first word of new_metal_context may be either. */
	bool first_ok = nmc[0] == w_new_metal[0] || nmc[0] == 0xd503201fu;
	const uint32_t *blm = (const uint32_t *)(A_RENDERER_BL_METAL + slide);
	if (*cbz != W_RENDERER_CBZ || *gnc != W_GLES_NEW_CONTEXT || !first_ok || *blm != W_RENDERER_BL_METAL ||
	    memcmp(nmc + 1, w_new_metal + 1, sizeof(w_new_metal) - sizeof(w_new_metal[0]))) {
		b2l_say("b2l-gl: QuartzCore is not 23G90 as expected (cbz %08x, gles %08x, metal %08x): nothing patched",
		    *cbz, *gnc, nmc[0]);
		return;
	}
	/* ldr x16, #8 ; br x16 ; .quad b2l_gles_context */
	struct { uint32_t ldr, br; uint64_t target; } __attribute__((packed)) jump = {
		0x58000050u, 0xd61f0200u, (uint64_t)ptrauth_strip((void *)b2l_gles_context, ptrauth_key_function_pointer)
	};
	static const uint32_t nop = 0xd503201fu;
	int e1 = b2l_write_code((uintptr_t)nmc, &jump, sizeof(jump));
	int e2 = e1 ? -1 : b2l_write_code((uintptr_t)cbz, &nop, sizeof(nop));
	/* B1L-0042: push_surface() takes the display surface make_buffer_current() made without tracking it. B1L-0041
	 * tracked it instead, and QuartzCore freed it later without untracking it: a freed surface in the context's list and
	 * a crash after ~250 s of GPU frames. Now the not-found branch (cbnz x8 at 0x183f131e4) simply goes on. */
	const uint32_t *ps = (const uint32_t *)(A_PUSH_NOTFOUND + slide);
	int e4 = -2;
	if (*ps == W_PUSH_NOTFOUND) {
		static const uint32_t b_on = 0x1400000fu;       /* b 0x183f13220 */
		e4 = b2l_write_code((uintptr_t)ps, &b_on, sizeof(b_on));
	}
	b2l_say("b2l-gl: push_surface takes untracked surfaces: %d (-2: word not as 23G90's)", e4);
	static const uint32_t mov0 = W_MOV_X0_0;
	int e3 = e2 ? -1 : b2l_write_code((uintptr_t)blm, &mov0, sizeof(mov0));
	if (!e1 && !e2 && !e3) {
		pthread_t watchdog;
		if (pthread_create(&watchdog, NULL, b2l_watchdog, NULL) == 0) pthread_detach(watchdog);
		static const int sigs[] = { SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGTRAP };
		for (unsigned i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) signal(sigs[i], b2l_crash_mark);
	}
	b2l_say("b2l-gl: AccelServer on OpenGL ES: new_metal_context %d, cbz %d, no Metal probe %d (slide %#lx)",
	    e1, e2, e3, (unsigned long)slide);
}
