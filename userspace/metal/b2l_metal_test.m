// SPDX-License-Identifier: BSD-2-Clause
// B1L-0044 (Metal M1): a Metal test inside backboardd, the one process our kernel injects a library into and whose sandbox
// reaches the GPU's IOKit services. What it checks: Metal finds our IOAcceleratorES service, loads B2LMetalV1.bundle
// (HakiOS's plug-in, userspace/metal/B2LMetal.m) and hands out its device; then a queue, a command buffer, commit and wait.
// Every step is a line in the kernel's log ("b2l-mtest: ...").
//
// CoreAnimation in the same process must not take the device (it cannot draw yet): Metal's device list is made once per
// process, so the bl CAMetalContextCreate in CA::WindowServer::AccelServer::renderer() (QuartzCore 23G90, 0x183f3bde8) becomes
// mov x0,#0 before anything asks, and the window server stays on its software renderer. The word is compared first.
//
// Once a boot: a marker with kern.boottime makes a backboardd started again skip the test, and a crash during it writes the
// marker before the process ends (launchd's next backboardd then runs without it).
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <IOSurface/IOSurfaceRef.h>
#import <QuartzCore/CAMetalLayer.h>
#include <fcntl.h>
#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>

#define MARKER                  "/private/var/tmp/b2l-metal-test-done"
#define CACHE_BASE              0x180000000UL
#define A_RENDERER_BL_METAL     0x183f3bde8UL   /* bl CAMetalContextCreate */
#define W_RENDERER_BL_METAL     0x940b76ecu
#define W_MOV_X0_0              0xd2800000u
/* B1L-0046: renderer() skips CAMetalContextCreate when the shared Metal context ([x20,#0x138]) already exists -- something
 * else in backboardd made it with our device in B1L-0045 -- and went on to new_metal_context(), which aborted loading
 * CoreAnimation's binary archive. The no-Metal branch itself (cbz x0 at 0x183f3bdf0) becomes unconditional. */
#define A_RENDERER_CBZ          0x183f3bdf0UL   /* cbz x0, 0x183f3bf34 */
#define W_RENDERER_CBZ          0xb4000a20u
#define W_B_NO_METAL            0x14000051u     /* b 0x183f3bf34 */

extern const void *_dyld_get_shared_cache_range(size_t *length);

static char session[64];

static void
mtsay(const char *format, ...)
{
	char line[400];
	struct timespec ts;
	clock_gettime(CLOCK_UPTIME_RAW, &ts);
	int at = snprintf(line, sizeof(line), "[%ld.%02ld] b2l-mtest: ", (long)ts.tv_sec, ts.tv_nsec / 10000000);
	va_list ap;
	va_start(ap, format);
	vsnprintf(line + at, sizeof(line) - (size_t)at, format, ap);
	va_end(ap);
	(void)sysctlbyname("kern.hakios.say", NULL, NULL, line, strlen(line));
}

static volatile int test_finished;
static void
mark(void)
{
	test_finished = 1;
	int fd = open(MARKER, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) { (void)write(fd, session, strlen(session)); close(fd); }
}

static void
crashed(int sig)
{
	mark();
	signal(sig, SIG_DFL);
	raise(sig);
}

static int
write_code(uintptr_t at, uint32_t word)
{
	vm_address_t page = at & ~(vm_address_t)PAGE_MASK;
	kern_return_t kr = vm_protect(mach_task_self(), page, PAGE_SIZE, FALSE, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
	if (kr != KERN_SUCCESS) return (int)kr;
	*(volatile uint32_t *)at = word;
	kr = vm_protect(mach_task_self(), page, PAGE_SIZE, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
	sys_icache_invalidate((void *)at, sizeof(word));
	return (int)kr;
}

#ifdef B2L_METAL_DRVTEST
/*
 * B1L-0068, M5 step 1: Metal drawing into an IOSurface -- the memory CoreAnimation's windows and the screen are made of.
 * A 64x64 'BGRA' surface (rows of 256 bytes) is filled by the processor with a canary, made a Metal texture with
 * -newTextureWithDescriptor:iosurface:plane: (the one call -[CAMetalLayer nextDrawable] makes of a device), then:
 *   pass 1  clear to opaque red, whole surface: every pixel must read B=0 G=0 R=255 A=255 through IOSurfaceLock;
 *   pass 2  the processor writes a second canary into rows 0..31, then a pass whose load action is Load and whose
 *           scissor... (no draw) -- left for step 2; here a second clear to green over the whole surface checks that
 *           the GPU sees the surface again after the processor wrote it, and the processor sees the GPU's write.
 * By then metal_drv_test has added libhakiosmetal's device to this process (MTLAddDevice), so
 * MTLCreateSystemDefaultDevice() answers it.
 */
static unsigned
m5_count(IOSurfaceRef s, uint32_t want, uint32_t *first)
{
	IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL);
	const uint8_t *base = IOSurfaceGetBaseAddress(s);
	size_t row = IOSurfaceGetBytesPerRow(s);
	unsigned n = 0;
	for (unsigned y = 0; y < 64; y++)
		for (unsigned x = 0; x < 64; x++) {
			uint32_t v = *(const uint32_t *)(base + y * row + x * 4);
			if (x == 0 && y == 0) *first = v;
			n += v == want;
		}
	IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL);
	return n;
}

static void
m5_fill(IOSurfaceRef s, uint32_t v)
{
	IOSurfaceLock(s, 0, NULL);
	uint8_t *base = IOSurfaceGetBaseAddress(s);
	size_t row = IOSurfaceGetBytesPerRow(s);
	for (unsigned y = 0; y < 64; y++)
		for (unsigned x = 0; x < 64; x++) *(uint32_t *)(base + y * row + x * 4) = v;
	IOSurfaceUnlock(s, 0, NULL);
}

static int
m5_clear(id<MTLCommandQueue> q, id<MTLTexture> t, double r, double g, double b)
{
	MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
	rp.colorAttachments[0].texture = t;
	rp.colorAttachments[0].loadAction = MTLLoadActionClear;
	rp.colorAttachments[0].storeAction = MTLStoreActionStore;
	rp.colorAttachments[0].clearColor = MTLClearColorMake(r, g, b, 1.0);
	id<MTLCommandBuffer> cb = [q commandBuffer];
	id<MTLRenderCommandEncoder> e = [cb renderCommandEncoderWithDescriptor:rp];
	[e endEncoding];
	[cb commit];
	[cb waitUntilCompleted];
	if ([cb status] != MTLCommandBufferStatusCompleted)
		mtsay("M5: clear ended in status %ld: %s", (long)[cb status],
		    [cb error] ? [[[cb error] description] UTF8String] : "no error");
	return [cb status] == MTLCommandBufferStatusCompleted;
}

/*
 * B1L-0071, M5 step 2: QuartzCore's own shaders drawing into the IOSurface. std_vert0_lpf (position at attribute 0,
 * colour at attribute 3, mvp_matrix at [[buffer(2)]], the stream at buffer 5) with attachment_clear_frag_lpf, which
 * writes 0 (transparent black), two triangles in ONE pass over a green clear -- the second draw must not clear the
 * first (B1L-0070). The rows below are what the Mac's own GPU drew with the same library, vertices and pass
 * (Intel UHD 617, macOS 26): 2005 pixels of 4096.
 */
static const unsigned char m5_ref_rows[64] = {
	0,27,26,25,25,24,25,25,26,25,26,26,27,27,28,28,28,28,29,29,29,30,30,31,30,31,31,32,32,33,33,33,
	33,34,34,34,35,35,35,35,36,36,37,37,38,37,38,38,39,39,40,40,41,41,42,43,44,45,46,47,47,0,0,0,
};

/* B1L-0077: one round of the 200-draw pass; with `watch`, this thread's PAC traps are counted by the kernel and the
 * most frequent places are named (dladdr) */
static void
m5_bench_round(id<MTLCommandQueue> q, MTLRenderPassDescriptor *rp, id<MTLRenderPipelineState> ps, id<MTLBuffer> vb,
    id<MTLBuffer> mb, IOSurfaceRef s, size_t row, int report, const char *where, int watch)
{
	uint32_t first_px = 0;
	m5_fill(s, 0xa5a5a5a5u);
	if (watch) {
		int on = 1;
		(void)sysctlbyname("kern.hakios.pac_watch", NULL, NULL, &on, sizeof(on));
	}
	id<MTLCommandBuffer> cb2 = [q commandBuffer];
	struct timespec a0, a1, a2;
	clock_gettime(CLOCK_MONOTONIC, &a0);
	id<MTLRenderCommandEncoder> e2 = [cb2 renderCommandEncoderWithDescriptor:rp];
	[e2 setRenderPipelineState:ps];
	[e2 setVertexBuffer:vb offset:0 atIndex:5];
	[e2 setVertexBuffer:mb offset:0 atIndex:2];
	[e2 setVertexBuffer:mb offset:0 atIndex:3];
	for (int i = 0; i < 200; i++)
		[e2 drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:(i & 1) ? 3 : 0 vertexCount:3];
	[e2 endEncoding];
	[cb2 commit];
	clock_gettime(CLOCK_MONOTONIC, &a1);
	[cb2 waitUntilCompleted];
	clock_gettime(CLOCK_MONOTONIC, &a2);
	uint64_t w[138];
	memset(w, 0, sizeof(w));
	if (watch) {
		size_t len = sizeof(w);
		int off = 0;
		(void)sysctlbyname("kern.hakios.pac_watch", w, &len, NULL, 0);
		(void)sysctlbyname("kern.hakios.pac_watch", NULL, NULL, &off, sizeof(off));
	}
	long enc = (a1.tv_sec - a0.tv_sec) * 1000000L + (a1.tv_nsec - a0.tv_nsec) / 1000;
	long all = (a2.tv_sec - a0.tv_sec) * 1000000L + (a2.tv_nsec - a0.tv_nsec) / 1000;
	unsigned drawn = m5_count(s, 0u, &first_px), rows_bad = 0;
	IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL);
	const uint8_t *b2 = IOSurfaceGetBaseAddress(s);
	for (unsigned y = 0; y < 64; y++) {
		unsigned n = 0;
		for (unsigned x = 0; x < 64; x++) n += *(const uint32_t *)(b2 + y * row + x * 4) == 0;
		rows_bad += n != m5_ref_rows[y];
	}
	IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL);
	if (report)
		mtsay("M5 bench (%s): 200 draws in one pass: encode+commit %ld us, until completed %ld us (%ld us a draw), "
		    "status %ld; %u drawn (Mac: 2005), %u rows differ", where, enc, all, all / 200, (long)cb2.status, drawn,
		    rows_bad);
	if (!watch) return;
	uint64_t total = 0;
	for (int k = 0; k < 8; k++) total += w[k];
	mtsay("M5 PAC (%s): %llu traps in the round (%llu a draw): reg %llu, hint %llu, ret %llu, br/blr %llu, ldra %llu, "
	    "ldapr %llu, pacga %llu, other %llu; in the shared cache %llu; no PC slot %llu", where,
	    (unsigned long long)total, (unsigned long long)(total / 200), (unsigned long long)w[0],
	    (unsigned long long)w[1], (unsigned long long)w[2], (unsigned long long)w[3], (unsigned long long)w[4],
	    (unsigned long long)w[5], (unsigned long long)w[6], (unsigned long long)w[7], (unsigned long long)w[8],
	    (unsigned long long)w[137]);
	for (int shown = 0; shown < 16; shown++) {
		int best = -1;
		for (int k = 0; k < 64; k++)
			if (w[9 + k] && w[73 + k] && (best < 0 || w[73 + k] > w[73 + best])) best = k;
		if (best < 0) break;
		Dl_info info;
		memset(&info, 0, sizeof(info));
		const char *img = "?", *sym = "?";
		uint64_t pc = w[9 + best];
		long off = 0;
		if (dladdr((const void *)pc, &info)) {
			img = info.dli_fname ? (strrchr(info.dli_fname, '/') ? strrchr(info.dli_fname, '/') + 1 : info.dli_fname) : "?";
			sym = info.dli_sname ? info.dli_sname : "?";
			off = info.dli_saddr ? (long)(pc - (uint64_t)info.dli_saddr) : 0;
		}
		mtsay("M5 PAC (%s): %llu at 0x%llx %s %s+%ld", where, (unsigned long long)w[73 + best],
		    (unsigned long long)pc, img, sym, off);
		w[73 + best] = 0;
	}
}

static void
m5_draw_test(id<MTLDevice> dev, id<MTLCommandQueue> q, id<MTLTexture> t, IOSurfaceRef s, const char *suffix)
{
	const char *path = "/System/Library/Frameworks/QuartzCore.framework/default.metallib";
	NSData *file = [NSData dataWithContentsOfFile:@(path)];
	if (file == nil) { mtsay("M5 draw: %s not readable", path); return; }
	dispatch_data_t dd = dispatch_data_create(file.bytes, file.length, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
	NSError *err = nil;
	id<MTLLibrary> lib = [dev newLibraryWithData:dd error:&err];
	if (lib == nil) { mtsay("M5 draw: newLibraryWithData: nil (%s)", err ? err.description.UTF8String : "no error"); return; }
	/* B1L-0087: the _lph (half precision) pair as well: backboardd's shaders are all _lph, every draw of ours was _lpf */
	id<MTLFunction> v = [lib newFunctionWithName:[NSString stringWithFormat:@"std_vert0_%s", suffix]];
	id<MTLFunction> f = [lib newFunctionWithName:[NSString stringWithFormat:@"attachment_clear_frag_%s", suffix]];
	mtsay("M5 draw (%s): QuartzCore library, std_vert0 %s, attachment_clear_frag %s", suffix, v ? "found" : "MISSING",
	    f ? "found" : "MISSING");
	if (v == nil || f == nil) return;
	MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
	pd.vertexFunction = v;
	pd.fragmentFunction = f;
	pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	MTLVertexDescriptor *vd = [MTLVertexDescriptor vertexDescriptor];
	vd.attributes[0].format = MTLVertexFormatFloat4; vd.attributes[0].offset = 0; vd.attributes[0].bufferIndex = 5;
	vd.attributes[3].format = MTLVertexFormatFloat4; vd.attributes[3].offset = 16; vd.attributes[3].bufferIndex = 5;
	vd.layouts[5].stride = 32;
	pd.vertexDescriptor = vd;
	id<MTLRenderPipelineState> ps = [dev newRenderPipelineStateWithDescriptor:pd error:&err];
	mtsay("M5 draw: pipeline %s%s", ps ? "made" : "refused: ", ps ? "" : (err ? err.description.UTF8String : "no error"));
	if (ps == nil) return;
	static const float vtx[6][8] = {
		{ -0.97f, -0.91f, 0, 1, 1, 0, 0, 1 }, { 0.53f, -0.91f, 0, 1, 1, 0, 0, 1 }, { -0.97f, 0.83f, 0, 1, 1, 0, 0, 1 },
		{ 0.13f, 0.97f, 0, 1, 0, 0, 1, 1 }, { 0.97f, 0.97f, 0, 1, 0, 0, 1, 1 }, { 0.97f, -0.61f, 0, 1, 0, 0, 1, 1 },
	};
	static const float ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	id<MTLBuffer> vb = [dev newBufferWithLength:sizeof(vtx) options:MTLResourceStorageModeShared];
	id<MTLBuffer> mb = [dev newBufferWithLength:sizeof(ident) options:MTLResourceStorageModeShared];
	if (vb == nil || mb == nil || vb.contents == NULL || mb.contents == NULL) { mtsay("M5 draw: buffers not made"); return; }
	memcpy(vb.contents, vtx, sizeof(vtx));
	memcpy(mb.contents, ident, sizeof(ident));
	m5_fill(s, 0xa5a5a5a5u);
	MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
	rp.colorAttachments[0].texture = t;
	rp.colorAttachments[0].loadAction = MTLLoadActionClear;
	rp.colorAttachments[0].storeAction = MTLStoreActionStore;
	rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 1, 0, 1);
	id<MTLCommandBuffer> cb = [q commandBuffer];
	id<MTLRenderCommandEncoder> e = [cb renderCommandEncoderWithDescriptor:rp];
	[e setRenderPipelineState:ps];
	[e setVertexBuffer:vb offset:0 atIndex:5];
	[e setVertexBuffer:mb offset:0 atIndex:2];
	[e setVertexBuffer:mb offset:0 atIndex:3];
	[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
	[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:3 vertexCount:3];
	[e endEncoding];
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	[cb commit];
	[cb waitUntilCompleted];
	clock_gettime(CLOCK_MONOTONIC, &t1);
	long us = (t1.tv_sec - t0.tv_sec) * 1000000L + (t1.tv_nsec - t0.tv_nsec) / 1000;
	mtsay("M5 draw: two draws in one pass, status %ld%s%s, %ld us", (long)cb.status, cb.error ? ": " : "",
	    cb.error ? cb.error.description.UTF8String : "", us);
	IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL);
	const uint8_t *base = IOSurfaceGetBaseAddress(s);
	size_t row = IOSurfaceGetBytesPerRow(s);
	unsigned zero = 0, green = 0, other = 0, bad_rows = 0;
	uint32_t first_px = 0;
	uint32_t other_v = 0;
	char diff[400] = { 0 };
	size_t dl = 0;
	for (unsigned y = 0; y < 64; y++) {
		unsigned n = 0;
		for (unsigned x = 0; x < 64; x++) {
			uint32_t px = *(const uint32_t *)(base + y * row + x * 4);
			if (px == 0) n++;
			else if (px == 0xff00ff00u) green++;
			else { other++; other_v = px; }
		}
		zero += n;
		if (n != m5_ref_rows[y]) {
			bad_rows++;
			if (dl < sizeof(diff) - 24)
				dl += snprintf(diff + dl, sizeof(diff) - dl, " y%u:%u/%u", y, n, m5_ref_rows[y]);
		}
	}
	uint32_t p88 = *(const uint32_t *)(base + 8 * row + 8 * 4), p856 = *(const uint32_t *)(base + 8 * row + 56 * 4);
	uint32_t p568 = *(const uint32_t *)(base + 56 * row + 8 * 4);
	IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL);
	mtsay("M5 draw: %u drawn (Mac: 2005), %u green, %u other (%08x); (x8,y8)=%08x (x56,y8)=%08x (x8,y56)=%08x "
	    "(Mac: ff00ff00 00000000 00000000)", zero, green, other, other_v, p88, p856, p568);
	mtsay("M5 draw: %u of 64 rows differ from the Mac's picture%s%s", bad_rows, bad_rows ? ":" : "", diff);
	if (bad_rows == 0 && other == 0)
		mtsay("M5 draw: QuartzCore's shaders drew into the IOSurface exactly as the Mac's GPU did: M5 step 2 done");

	/* B1L-0074: the cost of a pass of many draws, which is what a frame of the interface is -- 200 draws of the same
	 * two triangles, alternately, over a clear: the picture must stay the Mac's, and the time is the GPU's and the
	 * driver's for one command buffer (one wait). Measured three times, after one untimed warm-up.
	 * B1L-0077: wherever the scheduler puts the thread, then pinned to a big core (kern.hakios.bind_cpu=7, an M4), the
	 * last round of each with the kernel counting this thread's PAC traps (kern.hakios.pac_watch). */
	for (int pass = 0; pass < 2; pass++) {
		if (pass == 1) {
			int cpu = 7;
			int err = sysctlbyname("kern.hakios.bind_cpu", NULL, NULL, &cpu, sizeof(cpu));
			mtsay("M5 bench: thread pinned to cpu 7 (big core): %s", err ? "refused" : "yes");
		}
		for (int round = 0; round < 4; round++)
			m5_bench_round(q, rp, ps, vb, mb, s, row, round > 0, pass ? "big core" : "any core", round == 3);
	}
	{
		int cpu = -1;
		(void)sysctlbyname("kern.hakios.bind_cpu", NULL, NULL, &cpu, sizeof(cpu));
	}
}

/*
 * B1L-0072, M5 step 3: a CAMetalLayer's drawable, the way an application (and, in M6, the window server) gets its
 * surface. nextDrawable makes the IOSurface itself (CA::Render::create_iosurface_with_pixel_format) and asks our
 * device for a texture over it; the QuartzCore draw of step 2 goes into that texture, is checked against the Mac's
 * picture through the drawable's own IOSurface, and the drawable is presented.
 */
static void
m5_layer_test(id<MTLDevice> dev, id<MTLCommandQueue> q)
{
	CAMetalLayer *layer = [CAMetalLayer layer];
	mtsay("M5 layer: CAMetalLayer %s", layer ? "made" : "NOT made");
	if (layer == nil) return;
	layer.device = dev;
	layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	layer.framebufferOnly = NO;
	layer.drawableSize = CGSizeMake(64, 64);
	layer.frame = CGRectMake(0, 0, 64, 64);
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	id<CAMetalDrawable> d = [layer nextDrawable];
	clock_gettime(CLOCK_MONOTONIC, &t1);
	long us = (t1.tv_sec - t0.tv_sec) * 1000000L + (t1.tv_nsec - t0.tv_nsec) / 1000;
	id<MTLTexture> t = d.texture;
	IOSurfaceRef surf = (__bridge IOSurfaceRef)t.iosurface;
	mtsay("M5 layer: nextDrawable -> %s in %ld us; texture %s %lux%lu format %lu; IOSurface %p ID %u %zux%zu '%c%c%c%c' "
	    "%zu bytes a row", d ? NSStringFromClass([(id)d class]).UTF8String : "nil", us,
	    t ? NSStringFromClass([(id)t class]).UTF8String : "nil", (unsigned long)t.width, (unsigned long)t.height,
	    (unsigned long)t.pixelFormat, (void *)surf, surf ? IOSurfaceGetID(surf) : 0, surf ? IOSurfaceGetWidth(surf) : 0,
	    surf ? IOSurfaceGetHeight(surf) : 0, surf ? (char)(IOSurfaceGetPixelFormat(surf) >> 24) : '?',
	    surf ? (char)(IOSurfaceGetPixelFormat(surf) >> 16) : '?', surf ? (char)(IOSurfaceGetPixelFormat(surf) >> 8) : '?',
	    surf ? (char)IOSurfaceGetPixelFormat(surf) : '?', surf ? IOSurfaceGetBytesPerRow(surf) : 0);
	if (d == nil || t == nil || surf == NULL) return;
	if (IOSurfaceGetWidth(surf) != 64 || IOSurfaceGetHeight(surf) != 64) {
		mtsay("M5 layer: the drawable is not 64x64, so the Mac's picture does not apply");
		return;
	}
	m5_draw_test(dev, q, t, surf, "lpf");
	id<MTLCommandBuffer> cb = [q commandBuffer];
	[cb presentDrawable:d];
	[cb commit];
	[cb waitUntilCompleted];
	mtsay("M5 layer: presentDrawable committed, status %ld%s%s", (long)cb.status, cb.error ? ": " : "",
	    cb.error ? cb.error.description.UTF8String : "");
	id<CAMetalDrawable> d2 = [layer nextDrawable];
	mtsay("M5 layer: a second nextDrawable -> %s, IOSurface ID %u%s", d2 ? "a drawable" : "nil",
	    d2 && d2.texture.iosurface ? IOSurfaceGetID((__bridge IOSurfaceRef)d2.texture.iosurface) : 0,
	    d2 ? ": M5 step 3 reached the end" : "");
}

static void
m5_surface_test(void)
{
	id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
	mtsay("M5: device %s", dev ? [[dev name] UTF8String] : "none");
	if (dev == nil) return;
	NSDictionary *props = @{
		(id)kIOSurfaceWidth: @64, (id)kIOSurfaceHeight: @64, (id)kIOSurfaceBytesPerElement: @4,
		(id)kIOSurfaceBytesPerRow: @256, (id)kIOSurfacePixelFormat: @((uint32_t)'BGRA'),
		(id)kIOSurfaceIsGlobal: @YES,
	};
	IOSurfaceRef s = IOSurfaceCreate((__bridge CFDictionaryRef)props);
	mtsay("M5: IOSurface %p, ID %u, %zu bytes a row", (void *)s, s ? IOSurfaceGetID(s) : 0,
	    s ? IOSurfaceGetBytesPerRow(s) : 0);
	if (s == NULL) return;
	m5_fill(s, 0xa5a5a5a5u);
	MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
	    width:64 height:64 mipmapped:NO];
	d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	d.storageMode = MTLStorageModeShared;
	id<MTLTexture> t = [dev newTextureWithDescriptor:d iosurface:s plane:0];
	mtsay("M5: newTextureWithDescriptor:iosurface:plane: -> %s", t ? [NSStringFromClass([(id)t class]) UTF8String] : "nil");
	if (t == nil) { CFRelease(s); return; }
	id<MTLCommandQueue> q = [dev newCommandQueue];
	uint32_t first = 0;
	unsigned n;
	n = m5_count(s, 0xa5a5a5a5u, &first);
	mtsay("M5: before: %u of 4096 pixels hold the canary (first %08x)", n, first);
	int ok = m5_clear(q, t, 1.0, 0.0, 0.0);
	n = m5_count(s, 0xffff0000u, &first);      /* BGRA in memory: B 00, G 00, R ff, A ff -> little-endian 0xffff0000 */
	mtsay("M5: clear to red (status %s): %u of 4096 pixels are red (first %08x)%s", ok ? "completed" : "failed", n, first,
	    n == 4096 ? ": M5 step 1 done" : "");
	m5_fill(s, 0x5a5a5a5au);
	ok = m5_clear(q, t, 0.0, 1.0, 0.0);
	n = m5_count(s, 0xff00ff00u, &first);
	mtsay("M5: processor canary, then clear to green (status %s): %u of 4096 pixels are green (first %08x)%s",
	    ok ? "completed" : "failed", n, first, n == 4096 ? ": both directions seen" : "");
	m5_draw_test(dev, q, t, s, "lpf");
	m5_draw_test(dev, q, t, s, "lph");   /* B1L-0087 */
	CFRelease(s);
	m5_layer_test(dev, q);
}

/*
 * B1L-0051, M3: Scion's metal_drv_test (userspace/metal/scion, MIT) as a library: libhakiosmetal's device made by hand,
 * one of the system's compute kernels compiled from Apple's AIR by Mesa (AIR front end, NIR, panfrost's Bifrost compiler)
 * and dispatched on the Mali; "status 4" is completed. Its stdout and stderr go to the kernel's log, a line at a time.
 */
static int drv_pipe[2] = { -1, -1 };
static void *
drv_forward(void *unused)
{
	(void)unused;
	char buf[512], line[600];
	size_t used = 0;
	ssize_t n;
	while ((n = read(drv_pipe[0], buf, sizeof(buf))) > 0) {
		for (ssize_t i = 0; i < n; i++) {
			if (buf[i] == '\n' || used == sizeof(line) - 40) {
				line[used] = 0;
				char out[640];
				snprintf(out, sizeof(out), "drv: %s", line);
				(void)sysctlbyname("kern.hakios.say", NULL, NULL, out, strlen(out));
				used = 0;
				if (buf[i] == '\n') continue;
			}
			line[used++] = buf[i];
		}
	}
	return NULL;
}
static volatile int in_drvtest;
static void
run_drvtest(void)
{
	/* B1L-0056: M3 passed in B1L-0055 (both kernels compiled by Bifrost, dispatched, status 4). M4: a triangle with one
	 * of Apple's fragment shaders (clearFragment20, AppleMetalGLRenderer's library), its pixel compared with softpipe's
	 * (metal_drv_test --draw), then the same with a depth attachment and through an index buffer. */
	/* B1L-0057: in B1L-0056 the draw completed (status 4) and every pixel read back 0 -- seeded texels overwritten, the
	 * shader's own words absent. Panfrost's debug switches before the screen is made: sync (wait for every job, report
	 * GPU faults), linear + noafbc (plain row-major attachments, so the readback cannot be a layout question), nocrc (no
	 * transaction elimination, which skips writing tiles it believes unchanged). Then the draw again, and the attribute
	 * draw (QuartzCore's vertex shader + the GL renderer's fragment shader) as a second picture. */
	/* B1L-0060: softpipe drew every picture right in B1L-0059 (the Metal translation is sound); the Mali drew none. One
	 * draw (the generated full-screen quad + clearFragment20), with panfrost decoding every descriptor it hands the GPU
	 * (pandecode) onto stderr, which goes to the kernel's log a line at a time. */
	/* B1L-0074: no PAN_MESA_DEBUG=sync any more, which waited after every submission and hid the real cost */
	setenv("B2L_PANDECODE_RTS", "3", 1);
	setenv("B2L_PAN_PUSH_ALL", "1", 1);    /* B1L-0063: constants pushed as uniforms, not read with LOAD.ubo */
	setenv("PANDECODE_DUMP_FILE", "stderr", 1);
	/* B1L-0059: the same draws on Mesa's software rasterizer (libhakiosair's RWA_SOFTWARE), to split the question in two:
	 * right pictures here and wrong ones on the Mali put the fault in panfrost's path; wrong ones here too put it in the
	 * Metal-to-Gallium translation itself, on iOS 26. */
	/* (B1L-0059: RWA_SOFTWARE=1 -- done) */
	/* B1L-0058: libhakiosair gathers nir->info before panfrost compiles (userspace/mesa/air-b2l). The same three
	 * pictures: the constant draw, the indexed draws, the attribute draw. */
	static const char *const libs[][2] = {
		/* B1L-0062: constant buffers uploaded by panfrost at each draw; the three pictures again */
		/* B1L-0068: one compute run, which also adds libhakiosmetal's device to this process for the M5 test after it */
		{ "/System/Library/CoreImage/PortraitFilters.cifilter/default.metallib", "image_calcminmax0" },
	};
	void *lib = dlopen(B2L_METAL_DRVTEST, RTLD_NOW);
	int (*drv_main)(int, char **) = lib ? (int (*)(int, char **))dlsym(lib, "b2l_drv_main") : NULL;
	mtsay("M3: metal_drv_test %s (%s)", drv_main ? "loaded" : "not loaded", drv_main ? "" : dlerror());
	if (drv_main == NULL || pipe(drv_pipe) != 0) return;
	pthread_t forwarder;
	pthread_create(&forwarder, NULL, drv_forward, NULL);
	int saved_out = dup(1), saved_err = dup(2);
	setvbuf(stdout, NULL, _IOLBF, 0);
	dup2(drv_pipe[1], 1); dup2(drv_pipe[1], 2);
	in_drvtest = 1;
	for (unsigned i = 0; i < sizeof(libs) / sizeof(libs[0]); i++) {
		const char *file = libs[i][0][0] == '-' ? libs[i][1] : libs[i][0];
		if (access(file, R_OK) != 0) { mtsay("M3/M4: %s is not on this system", file); continue; }
		char *argv[] = { "metal_drv_test", (char *)libs[i][0], (char *)libs[i][1], NULL };
		mtsay("M3: metal_drv_test %s %s ...", libs[i][0], libs[i][1]);
		int r = drv_main(3, argv);
		fflush(stdout); fflush(stderr);
		mtsay("M3: metal_drv_test returned %d", r);
	}
	/* B1L-0069: M5 while the driver's stdout still goes to the kernel's log, so that its refusals are read */
	m5_surface_test();
	fflush(stdout); fflush(stderr);
	in_drvtest = 0;
	dup2(saved_out, 1); dup2(saved_err, 2);
	close(drv_pipe[1]);
	pthread_join(forwarder, NULL);
}
#endif

static pthread_t test_thread;
static int test_started;

/* the host returns from main at once: exit() waits here for the test, 90 s at most */
static void
wait_for_test(void)
{
	for (int i = 0; i < 900 && test_started && !test_finished; i++) usleep(100000);
	mtsay("host exits (test %s)", test_finished ? "finished" : "still running after 90 s");
}

static void *
run(void *unused)
{
	if (unused == NULL)
		sleep(20);                              /* in backboardd: after the window server's own start */
#ifdef B2L_METAL_DRVTEST
	/* B1L-0052: M1 and M2 passed in B1L-0051. M3 alone, with our plug-in hiding itself (no B2L_METAL_TEST), so that
	 * Metal's discovery gives this process no device and metal_drv_test makes libhakiosmetal's own */
	run_drvtest();
	mark();
	return NULL;
#endif
	mtsay("asking Metal for a device ...");
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	mtsay("MTLCreateSystemDefaultDevice -> %p (%s)", (__bridge void *)device,
	    device ? [NSStringFromClass([(id)device class]) UTF8String] : "none");
	if (device == nil) { mark(); return NULL; }
	mtsay("name '%s', registryID %llu, maxBufferLength %llu", [[device name] UTF8String],
	    (unsigned long long)[device registryID], (unsigned long long)[device maxBufferLength]);
	id<MTLCommandQueue> queue = [device newCommandQueue];
	mtsay("newCommandQueue -> %p (%s)", (__bridge void *)queue, queue ? [NSStringFromClass([(id)queue class]) UTF8String] : "none");
	if (queue != nil) {
		id<MTLCommandBuffer> cb = [queue commandBuffer];
		mtsay("commandBuffer -> %p", (__bridge void *)cb);
		if (cb != nil) {
			[cb commit];
			mtsay("committed; waiting ...");
			[cb waitUntilCompleted];
			mtsay("completed: status %ld, error %s", (long)[cb status],
			    [cb error] ? [[[cb error] description] UTF8String] : "none");
		}
	}
	id<MTLBuffer> buffer = [device newBufferWithLength:4096 options:MTLResourceStorageModeShared];
	mtsay("newBufferWithLength:4096 -> %p", (__bridge void *)buffer);
	mtsay("M1 done");

	/* B1L-0048, M2: buffers in the Mali's memory, filled and copied by the GPU, read back by the processor */
	const NSUInteger n = 256 * 1024;
	id<MTLBuffer> a = [device newBufferWithLength:n options:MTLResourceStorageModeShared];
	id<MTLBuffer> b = [device newBufferWithLength:n options:MTLResourceStorageModeShared];
	mtsay("M2: two buffers of %lu bytes: %p %p (contents %p %p)", (unsigned long)n, (__bridge void *)a, (__bridge void *)b,
	    a ? [a contents] : NULL, b ? [b contents] : NULL);
	if (queue == nil || a == nil || b == nil || [a contents] == NULL || [b contents] == NULL) { mark(); return NULL; }
	uint8_t *pa = [a contents], *pb = [b contents];
	memset(pb, 0x11, n);
	for (NSUInteger i = 0; i < n; i++) pa[i] = (uint8_t)(i * 7 + 3);
	struct timespec t0, t1;
	clock_gettime(CLOCK_UPTIME_RAW, &t0);
	id<MTLCommandBuffer> cb2 = [queue commandBuffer];
	id<MTLBlitCommandEncoder> blit = [cb2 blitCommandEncoder];
	mtsay("M2: blit encoder %p", (__bridge void *)blit);
	[blit copyFromBuffer:a sourceOffset:0 toBuffer:b destinationOffset:0 size:n / 2];
	[blit fillBuffer:b range:NSMakeRange(n / 2, n / 2) value:0x5a];
	[blit endEncoding];
	[cb2 commit];
	[cb2 waitUntilCompleted];
	clock_gettime(CLOCK_UPTIME_RAW, &t1);
	NSUInteger copied = 0, filled = 0;
	for (NSUInteger i = 0; i < n / 2; i++) copied += pb[i] == (uint8_t)(i * 7 + 3);
	for (NSUInteger i = n / 2; i < n; i++) filled += pb[i] == 0x5a;
	mtsay("M2: status %ld (%s) in %ld us: copied %lu of %lu bytes right, filled %lu of %lu right%s",
	    (long)[cb2 status], [cb2 error] ? [[[cb2 error] description] UTF8String] : "no error",
	    (long)((t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000),
	    (unsigned long)copied, (unsigned long)(n / 2), (unsigned long)filled, (unsigned long)(n / 2),
	    copied == n / 2 && filled == n / 2 ? ": M2 done" : "");
#ifdef B2L_METAL_DRVTEST
	run_drvtest();
#endif
	mark();
	return NULL;
}

__attribute__((constructor)) static void
b2l_metal_test_install(void)
{
	struct timeval bt = { 0, 0 };
	size_t bl = sizeof(bt);
	if (sysctlbyname("kern.boottime", &bt, &bl, NULL, 0) == 0)
		snprintf(session, sizeof(session), "%ld.%06d", (long)bt.tv_sec, (int)bt.tv_usec);
	char seen[64] = "";
	int fd = open(MARKER, O_RDONLY);
	if (fd >= 0) { ssize_t n = read(fd, seen, sizeof(seen) - 1); close(fd); if (n > 0) seen[n] = 0; }

	/* B1L-0049: the test's host is our bootstrap daemon; in backboardd (an older kernel) CoreAnimation is kept off Metal */
	if (strcmp(getprogname(), "backboardd") != 0) {
		if (session[0] && strcmp(seen, session) == 0) { mtsay("already run this boot (%s)", getprogname()); return; }
#ifndef B2L_METAL_DRVTEST
		setenv("B2L_METAL_TEST", "1", 1);
#endif
		static const int hsigs[] = { SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGTRAP };
		for (unsigned i = 0; i < sizeof(hsigs) / sizeof(hsigs[0]); i++) signal(hsigs[i], crashed);
		/* B1L-0053: in a child of its own. tzinit is a boot task launchd requires to succeed: metal_drv_test's exit(4)
		 * inside it made tzinit exit 4 and the boot panic (B1L-0052). The child runs the test and _exit()s; the host goes
		 * on at once, untouched. fork() here, in a constructor, is before the host has a second thread. */
		/* B1L-0054: no fork -- the forked child of B1L-0053 had PAC emulation refuse a branch ("B2L COMPAT: rejected",
		 * the shared region of a forked task). The host is weatherd now, an ordinary daemon launchd starts and restarts
		 * (not a boot task); the test runs in a thread of it, once a boot (marker first). */
		mark();
		mtsay("in %s (pid %d): the test runs in a thread", getprogname(), getpid());
		pthread_t t;
		if (pthread_create(&t, NULL, run, (void *)1) == 0) pthread_detach(t);
		return;
	}
	/* CoreAnimation stays off Metal in any case (the device list is per process and made once) */
	size_t length = 0;
	const void *cache = _dyld_get_shared_cache_range(&length);
	if (cache == NULL) { mtsay("no shared cache: no test"); return; }
	uintptr_t at = A_RENDERER_BL_METAL + ((uintptr_t)cache - CACHE_BASE);
	if (*(const uint32_t *)at != W_RENDERER_BL_METAL) {
		mtsay("QuartzCore's renderer() is not 23G90's (%08x): no test", *(const uint32_t *)at);
		return;
	}
	uintptr_t cbz = A_RENDERER_CBZ + ((uintptr_t)cache - CACHE_BASE);
	if (*(const uint32_t *)cbz != W_RENDERER_CBZ) {
		mtsay("QuartzCore's renderer() branch is not 23G90's (%08x): no test", *(const uint32_t *)cbz);
		return;
	}
	int e = write_code(at, W_MOV_X0_0);
	if (e == 0) e = write_code(cbz, W_B_NO_METAL);
	mtsay("window server kept off Metal (call and branch): %d", e);
	if (e != 0) return;

	if (session[0] && strcmp(seen, session) == 0) { mtsay("already run this boot"); return; }
	/* B1L-0045: before anything in this process asks Metal (its device list is made once per process; in B1L-0044 something
	 * asked at 207 s and the plug-in, without the variable, kept the device from backboardd for good) */
	setenv("B2L_METAL_TEST", "1", 1);
	static const int sigs[] = { SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGTRAP };
	for (unsigned i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) signal(sigs[i], crashed);
	pthread_t thread;
	if (pthread_create(&thread, NULL, run, NULL) == 0) pthread_detach(thread);
}
