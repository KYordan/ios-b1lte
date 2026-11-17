/* SPDX-License-Identifier: BSD-2-Clause
 * B1L-0079, Metal M6a: Metal on the Mali, on the SCREEN, through the real presentation path, without touching the
 * window server. The kernel inserts this library into the Clock app (/Applications/MobileTimer.app/MobileTimer,
 * b2l_exec_graphics_diag.h; kill switch boot-arg b2l_metal_app=0). Once the app has a window, a full-screen CAMetalLayer
 * goes on top of it; a CADisplayLink draws every frame with QuartzCore's own shaders (std_vert0_lpf +
 * attachment_clear_frag_lpf, validated against the Mac's GPU in M5): a background whose colour cycles and 24 black
 * triangles turning around the centre. Each frame is presented once the GPU is done with it ([drawable present] in
 * the completed handler), and goes to the screen through CoreAnimation's image queue and backboardd's compositor
 * (still the CPU renderer). Every 120 frames: images per second, and where the time went. Log prefix "b2l-mapp:".
 *
 * The device is libhakiosmetal's (Scion's Mali driver), made by hand as metal_drv_test does it
 * ([[HakiosMetalDevice alloc] initWithAcceleratorPort:IOAcceleratorES]); MTLCreateSystemDefaultDevice() is not asked,
 * because the plug-in Metal would load for it hides its device outside tests.
 */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <IOSurface/IOSurfaceRef.h>
#include <IOKit/IOKitLib.h>
#include <dlfcn.h>
#include <math.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdarg.h>
#include <string.h>
#include <sys/sysctl.h>
#include <time.h>

#ifndef B2L_METAL_SET
#define B2L_METAL_SET "/usr/local/lib/b2l-metal-v35"
#endif
#define TRIS 24

static void
msay(const char *format, ...)
{
	char line[480], out[512];
	va_list ap;
	va_start(ap, format);
	vsnprintf(line, sizeof(line), format, ap);
	va_end(ap);
	snprintf(out, sizeof(out), "b2l-mapp: %s", line);
	(void)sysctlbyname("kern.hakios.say", NULL, NULL, out, strlen(out));
}

static double
now_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

@interface B2LMetalAppView : NSObject
@property (nonatomic, retain) CAMetalLayer *layer;
@property (nonatomic, retain) id<MTLDevice> device;
@property (nonatomic, retain) id<MTLCommandQueue> queue;
@property (nonatomic, retain) id<MTLRenderPipelineState> pipeline;
@property (nonatomic, retain) id<MTLBuffer> vertices;
@property (nonatomic, retain) id<MTLBuffer> matrix;
@property (nonatomic, retain) CADisplayLink *link;
@end

@implementation B2LMetalAppView {
	unsigned long _frames, _windowFrames, _failures;
	double _windowStart, _cpu, _wait, _gpu, _worst;
	BOOL _saidFirst;
}

- (void)tick:(CADisplayLink *)link
{
	(void)link;
	double t0 = now_ms();
	id<CAMetalDrawable> d = [self.layer nextDrawable];
	double t1 = now_ms();
	if (d == nil) {
		if (_failures++ < 5) msay("nextDrawable gave nil (frame %lu)", _frames);
		return;
	}
	if (!_saidFirst) {
		_saidFirst = YES;
		id<MTLTexture> t = d.texture;
		IOSurfaceRef s = (IOSurfaceRef)t.iosurface;
		msay("first drawable: texture %s %lux%lu format %lu; IOSurface ID %u, %zu bytes a row",
		    t ? class_getName(object_getClass(t)) : "nil", (unsigned long)t.width, (unsigned long)t.height,
		    (unsigned long)t.pixelFormat, s ? IOSurfaceGetID(s) : 0, s ? IOSurfaceGetBytesPerRow(s) : 0);
	}
	/* the picture: 24 triangles around the centre, turning; the background's colour cycling */
	double phase = _frames / 60.0;
	float *v = (float *)self.vertices.contents;
	CGSize size = self.layer.drawableSize;
	float aspect = size.height > 0 ? (float)(size.width / size.height) : 1.0f;
	for (int i = 0; i < TRIS; i++) {
		double a = phase + i * (2 * M_PI / TRIS), r0 = 0.25 + 0.15 * sin(phase * 2 + i), r1 = r0 + 0.35;
		double w = 0.08;
		float pts[3][2] = {
			{ (float)(cos(a - w) * r0), (float)(sin(a - w) * r0) },
			{ (float)(cos(a + w) * r0), (float)(sin(a + w) * r0) },
			{ (float)(cos(a) * r1), (float)(sin(a) * r1) },
		};
		for (int k = 0; k < 3; k++) {
			float *p = v + (i * 3 + k) * 8;
			p[0] = pts[k][0]; p[1] = pts[k][1] * aspect; p[2] = 0; p[3] = 1;
			p[4] = 1; p[5] = 1; p[6] = 1; p[7] = 1;
		}
	}
	MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
	rp.colorAttachments[0].texture = d.texture;
	rp.colorAttachments[0].loadAction = MTLLoadActionClear;
	rp.colorAttachments[0].storeAction = MTLStoreActionStore;
	rp.colorAttachments[0].clearColor = MTLClearColorMake(0.5 + 0.5 * sin(phase), 0.5 + 0.5 * sin(phase + 2.1),
	    0.5 + 0.5 * sin(phase + 4.2), 1.0);
	id<MTLCommandBuffer> cb = [self.queue commandBuffer];
	id<MTLRenderCommandEncoder> e = [cb renderCommandEncoderWithDescriptor:rp];
	[e setRenderPipelineState:self.pipeline];
	[e setVertexBuffer:self.vertices offset:0 atIndex:5];
	[e setVertexBuffer:self.matrix offset:0 atIndex:2];
	[e setVertexBuffer:self.matrix offset:0 atIndex:3];
	for (int i = 0; i < TRIS; i++)
		[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:i * 3 vertexCount:3];
	[e endEncoding];
	__block double gpuDone = 0;
	[cb addCompletedHandler:^(id<MTLCommandBuffer> b) {
		(void)b;
		gpuDone = now_ms();
		[d present];   /* to the screen only once the Mali is done with the pixels */
	}];
	double t2 = now_ms();
	[cb commit];       /* this driver delivers the completed handler inside -commit, after its fence */
	double t3 = now_ms();
	if (cb.status == MTLCommandBufferStatusError && _failures++ < 5)
		msay("frame %lu failed: %s", _frames, cb.error.description.UTF8String);
	_frames++;
	_windowFrames++;
	_wait += t1 - t0;
	_cpu += t2 - t1;
	_gpu += (gpuDone > 0 ? gpuDone : t3) - t2;
	if (t3 - t0 > _worst) _worst = t3 - t0;
	if (_windowFrames == 120) {
		double span = t3 - _windowStart;
		msay("%lu frames: %.1f images/s over %.0f ms; a frame: nextDrawable %.2f ms, encoding %.2f ms, commit to GPU "
		    "done %.2f ms, worst %.1f ms; drawable %.0fx%.0f, %d draws", _frames, 120 * 1000.0 / span, span,
		    _wait / 120, _cpu / 120, _gpu / 120, _worst, size.width, size.height, TRIS);
		_windowFrames = 0;
		_windowStart = t3;
		_wait = _cpu = _gpu = _worst = 0;
	}
}

- (BOOL)startIn:(UIWindow *)window
{
	void *lib = dlopen(B2L_METAL_SET "/libhakiosmetal.dylib", RTLD_NOW);
	Class cls = objc_getClass("HakiosMetalDevice");
	io_service_t svc = IOServiceGetMatchingService(0, IOServiceMatching("IOAcceleratorES"));
	msay("libhakiosmetal %s, HakiosMetalDevice %s, IOAcceleratorES %s", lib ? "loaded" : dlerror(),
	    cls ? "found" : "missing", svc ? "found" : "missing");
	if (cls == Nil || svc == IO_OBJECT_NULL) return NO;
	id dev = ((id (*)(id, SEL, io_service_t))objc_msgSend)([cls alloc], sel_getUid("initWithAcceleratorPort:"), svc);
	msay("device %s", dev ? [[dev name] UTF8String] : "nil");
	if (dev == nil) return NO;
	self.device = dev;
	self.queue = [dev newCommandQueue];
	NSData *file = [NSData dataWithContentsOfFile:@"/System/Library/Frameworks/QuartzCore.framework/default.metallib"];
	NSError *err = nil;
	id<MTLLibrary> qc = file ? [dev newLibraryWithData:dispatch_data_create(file.bytes, file.length, NULL,
	    DISPATCH_DATA_DESTRUCTOR_DEFAULT) error:&err] : nil;
	MTLRenderPipelineDescriptor *pd = [[MTLRenderPipelineDescriptor new] autorelease];
	pd.vertexFunction = [qc newFunctionWithName:@"std_vert0_lpf"];
	pd.fragmentFunction = [qc newFunctionWithName:@"attachment_clear_frag_lpf"];
	pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	MTLVertexDescriptor *vd = [MTLVertexDescriptor vertexDescriptor];
	vd.attributes[0].format = MTLVertexFormatFloat4; vd.attributes[0].offset = 0; vd.attributes[0].bufferIndex = 5;
	vd.attributes[3].format = MTLVertexFormatFloat4; vd.attributes[3].offset = 16; vd.attributes[3].bufferIndex = 5;
	vd.layouts[5].stride = 32;
	pd.vertexDescriptor = vd;
	double c0 = now_ms();
	self.pipeline = (pd.vertexFunction && pd.fragmentFunction) ?
	    [dev newRenderPipelineStateWithDescriptor:pd error:&err] : nil;
	msay("QuartzCore library %s, pipeline %s in %.0f ms%s%s", qc ? "loaded" : "NOT loaded",
	    self.pipeline ? "made" : "NOT made", now_ms() - c0, err ? ": " : "", err ? err.description.UTF8String : "");
	if (self.pipeline == nil) return NO;
	self.vertices = [dev newBufferWithLength:TRIS * 3 * 32 options:MTLResourceStorageModeShared];
	self.matrix = [dev newBufferWithLength:64 options:MTLResourceStorageModeShared];
	float *m = (float *)self.matrix.contents;
	memset(m, 0, 64);
	m[0] = m[5] = m[10] = m[15] = 1;

	CAMetalLayer *layer = [CAMetalLayer layer];
	layer.device = dev;
	layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	layer.framebufferOnly = NO;
	layer.opaque = YES;
	CGFloat scale = window.screen.scale;
	layer.contentsScale = scale;
	layer.frame = window.layer.bounds;
	layer.drawableSize = CGSizeMake(window.layer.bounds.size.width * scale, window.layer.bounds.size.height * scale);
	layer.zPosition = 10000;
	[window.layer addSublayer:layer];
	self.layer = layer;
	msay("CAMetalLayer on the window: %.0fx%.0f points at scale %.1f, drawable %.0fx%.0f",
	    layer.frame.size.width, layer.frame.size.height, scale, layer.drawableSize.width, layer.drawableSize.height);
	_windowStart = now_ms();
	self.link = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
	[self.link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
	return YES;
}
@end

static B2LMetalAppView *view;

static UIWindow *
find_window(void)
{
	UIApplication *app = [UIApplication sharedApplication];
	for (UIScene *scene in app.connectedScenes) {
		if (![scene isKindOfClass:[UIWindowScene class]]) continue;
		for (UIWindow *w in ((UIWindowScene *)scene).windows)
			if (w.isKeyWindow) return w;
		if (((UIWindowScene *)scene).windows.count) return ((UIWindowScene *)scene).windows.firstObject;
	}
	return nil;
}

static void
try_start(int attempt)
{
	UIWindow *w = find_window();
	if (w == nil || w.layer.bounds.size.width < 1) {
		if (attempt < 30)
			dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
				try_start(attempt + 1);
			});
		else
			msay("no window after 30 s");
		return;
	}
	view = [[B2LMetalAppView alloc] init];
	BOOL ok = [view startIn:w];
	msay("M6a %s", ok ? "running: the Clock app shows the Mali's frames" : "did not start");
}

__attribute__((constructor)) static void
b2l_metal_app_init(void)
{
	msay("in %s (pid %d)", getprogname(), getpid());
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
		try_start(0);
	});
}
