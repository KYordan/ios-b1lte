/* SPDX-License-Identifier: MIT
 * B1L-0048 (Metal M2): the part of hakios-metal's C interface (hakios_metal.h, HakiOS, MIT) that HakiOS's plug-in uses
 * (userspace/metal/B2LMetal.m with HAKIOS_METAL_GPU), over Mesa's Gallium panfrost instead of HakiOS's Adreno core:
 * the device, buffers in GPU memory with a persistent CPU mapping, queues, and command buffers with copy and fill done by
 * the GPU, committed in order and completed on the queue's own thread.
 *
 * Built into libOSMesa (src/gallium/targets/osmesa, scripts/build-mesa.sh): the screen is osmesa_create_screen()'s, the
 * Mali through src/hakios-drm and the kernel's HakiosMali. One pipe_context for the process, behind a lock (a Gallium
 * context is not thread-safe). Recorded commands run at commit, as Metal's do: the CPU may still write a buffer between
 * encoding and commit.
 */
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <time.h>

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#include "util/box.h"
#include "util/u_inlines.h"

#include "hakios_metal.h"

#define EXPORT __attribute__((visibility("default")))

struct pipe_screen *osmesa_create_screen(void);

static __thread char last_error[256];

static void
fail(const char *format, ...)
{
   va_list ap;
   va_start(ap, format);
   vsnprintf(last_error, sizeof(last_error), format, ap);
   va_end(ap);
}

static void
say(const char *format, ...)
{
   char line[300];
   int at = snprintf(line, sizeof(line), "b2l-mcore: ");
   va_list ap;
   va_start(ap, format);
   vsnprintf(line + at, sizeof(line) - (size_t)at, format, ap);
   va_end(ap);
   (void)sysctlbyname("kern.hakios.say", NULL, NULL, line, strlen(line));
}

static int64_t
now_ns(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_UPTIME_RAW, &ts);
   return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

struct hakios_metal_device {
   struct pipe_screen *screen;
   struct pipe_context *pipe;
   pthread_mutex_t lock;
   uint64_t allocated;
   uint64_t gpu_id;
};

struct hakios_metal_buffer {
   hakios_metal_device *device;
   struct pipe_resource *resource;
   struct pipe_transfer *transfer;
   void *map;
   uint64_t length;
};

enum op_kind { OP_COPY, OP_FILL };
struct op {
   enum op_kind kind;
   hakios_metal_buffer *from, *to;
   uint64_t from_offset, to_offset, length;
   uint8_t value;
};

struct hakios_metal_cb {
   hakios_metal_queue *queue;
   struct op *ops;
   unsigned count, room;
   enum hakios_metal_status status;
   char error[256];
   void (*callback)(hakios_metal_cb *, void *);
   void *context;
   int64_t committed, gpu_ns;
   hakios_metal_cb *next;
};

struct hakios_metal_queue {
   hakios_metal_device *device;
   pthread_t thread;
   pthread_mutex_t lock;
   pthread_cond_t cond;
   hakios_metal_cb *head, *tail;
   int stop;
};

EXPORT const char *
hakios_metal_error(void)
{
   return last_error;
}

EXPORT hakios_metal_device *
hakios_metal_open(void)
{
   static hakios_metal_device *device;
   static pthread_mutex_t once = PTHREAD_MUTEX_INITIALIZER;

   pthread_mutex_lock(&once);
   if (device == NULL) {
      struct pipe_screen *screen = osmesa_create_screen();
      const char *name = screen && screen->get_name ? screen->get_name(screen) : "";
      if (screen == NULL || strstr(name, "Mali") == NULL) {
         fail("no GPU screen (%s)", screen ? name : "none");
         say("open: %s", last_error);
      } else {
         hakios_metal_device *d = calloc(1, sizeof(*d));
         d->screen = screen;
         d->pipe = screen->context_create(screen, NULL, 0);
         pthread_mutex_init(&d->lock, NULL);
         d->gpu_id = 0x7211;
         if (d->pipe == NULL) {
            fail("no Gallium context on %s", name);
            say("open: %s", last_error);
            free(d);
         } else {
            device = d;
            say("open: %s, Gallium context %p", name, (void *)d->pipe);
         }
      }
   }
   pthread_mutex_unlock(&once);
   return device;
}

EXPORT hakios_metal_device *
hakios_metal_open_fake(void)
{
   fail("no fake GPU in this build");
   return NULL;
}

EXPORT void
hakios_metal_close(hakios_metal_device *device)
{
   (void)device;      /* the process keeps its one device */
}

EXPORT uint64_t
hakios_metal_gpu_id(hakios_metal_device *device)
{
   return device ? device->gpu_id : 0;
}

EXPORT uint64_t
hakios_metal_device_allocated_bytes(hakios_metal_device *device)
{
   return device ? device->allocated : 0;
}

EXPORT hakios_metal_buffer *
hakios_metal_buffer_new(hakios_metal_device *device, uint64_t length)
{
   if (device == NULL) {
      fail("no device");
      return NULL;
   }
   uint64_t size = length ? length : 1;
   struct pipe_resource templ = {
      .target = PIPE_BUFFER,
      .format = PIPE_FORMAT_R8_UNORM,
      .width0 = (unsigned)size,
      .height0 = 1,
      .depth0 = 1,
      .array_size = 1,
      .bind = PIPE_BIND_VERTEX_BUFFER | PIPE_BIND_INDEX_BUFFER | PIPE_BIND_CONSTANT_BUFFER | PIPE_BIND_SHADER_BUFFER,
      .usage = PIPE_USAGE_DEFAULT,
      .flags = PIPE_RESOURCE_FLAG_MAP_PERSISTENT | PIPE_RESOURCE_FLAG_MAP_COHERENT,
   };
   if (size > 0xffffffffull) {
      fail("a buffer of %llu bytes is too long", (unsigned long long)size);
      return NULL;
   }
   hakios_metal_buffer *b = calloc(1, sizeof(*b));
   pthread_mutex_lock(&device->lock);
   b->resource = device->screen->resource_create(device->screen, &templ);
   if (b->resource != NULL) {
      b->map = pipe_buffer_map(device->pipe, b->resource,
                               PIPE_MAP_READ | PIPE_MAP_WRITE | PIPE_MAP_PERSISTENT | PIPE_MAP_COHERENT |
                               PIPE_MAP_UNSYNCHRONIZED, &b->transfer);
   }
   pthread_mutex_unlock(&device->lock);
   if (b->resource == NULL || b->map == NULL) {
      fail("Gallium gave no %s for %llu bytes", b->resource ? "mapping" : "buffer", (unsigned long long)size);
      if (b->resource)
         pipe_resource_reference(&b->resource, NULL);
      free(b);
      return NULL;
   }
   b->device = device;
   b->length = length;
   device->allocated += size;
   return b;
}

EXPORT void
hakios_metal_buffer_free(hakios_metal_buffer *b)
{
   if (b == NULL)
      return;
   hakios_metal_device *d = b->device;
   pthread_mutex_lock(&d->lock);
   if (b->transfer)
      pipe_buffer_unmap(d->pipe, b->transfer);
   pipe_resource_reference(&b->resource, NULL);
   pthread_mutex_unlock(&d->lock);
   d->allocated -= b->length ? b->length : 1;
   free(b);
}

EXPORT void *
hakios_metal_buffer_contents(hakios_metal_buffer *b)
{
   return b ? b->map : NULL;
}

EXPORT uint64_t
hakios_metal_buffer_length(hakios_metal_buffer *b)
{
   return b ? b->length : 0;
}

EXPORT uint64_t
hakios_metal_buffer_gpu_address(hakios_metal_buffer *b)
{
   (void)b;
   return 0;          /* M2: not asked for yet (panfrost keeps it in its own resource) */
}

/* ---- queues and command buffers ---- */

static void
run(hakios_metal_cb *cb)
{
   hakios_metal_device *d = cb->queue->device;
   struct pipe_fence_handle *fence = NULL;

   pthread_mutex_lock(&d->lock);
   for (unsigned i = 0; i < cb->count; i++) {
      struct op *op = &cb->ops[i];
      if (op->kind == OP_COPY) {
         struct pipe_box box;
         u_box_1d((int)op->from_offset, (int)op->length, &box);
         d->pipe->resource_copy_region(d->pipe, op->to->resource, 0, (unsigned)op->to_offset, 0, 0,
                                       op->from->resource, 0, &box);
      } else {
         uint32_t word = op->value * 0x01010101u;
         d->pipe->clear_buffer(d->pipe, op->to->resource, (unsigned)op->to_offset, (unsigned)op->length, &word, 1);
      }
   }
   d->pipe->flush(d->pipe, &fence, 0);
   pthread_mutex_unlock(&d->lock);
   if (fence != NULL) {
      bool done = d->screen->fence_finish(d->screen, NULL, fence, 10ull * 1000000000ull);
      d->screen->fence_reference(d->screen, &fence, NULL);
      if (!done)
         snprintf(cb->error, sizeof(cb->error), "the GPU did not finish within 10 s");
   }
   cb->gpu_ns = now_ns() - cb->committed;
}

static void *
queue_thread(void *argument)
{
   hakios_metal_queue *q = argument;

   pthread_mutex_lock(&q->lock);
   for (;;) {
      while (q->head == NULL && !q->stop)
         pthread_cond_wait(&q->cond, &q->lock);
      if (q->head == NULL)
         break;
      hakios_metal_cb *cb = q->head;
      q->head = cb->next;
      if (q->head == NULL)
         q->tail = NULL;
      pthread_mutex_unlock(&q->lock);

      if (cb->error[0] == 0)
         run(cb);
      void (*callback)(hakios_metal_cb *, void *) = cb->callback;
      void *context = cb->context;

      pthread_mutex_lock(&q->lock);
      cb->status = cb->error[0] ? HAKIOS_METAL_ERROR : HAKIOS_METAL_COMPLETED;
      pthread_cond_broadcast(&q->cond);
      pthread_mutex_unlock(&q->lock);
      if (callback)
         callback(cb, context);        /* the last use of cb: it may be freed in there */
      pthread_mutex_lock(&q->lock);
   }
   pthread_mutex_unlock(&q->lock);
   return NULL;
}

EXPORT hakios_metal_queue *
hakios_metal_queue_new(hakios_metal_device *device)
{
   if (device == NULL) {
      fail("no device");
      return NULL;
   }
   hakios_metal_queue *q = calloc(1, sizeof(*q));
   q->device = device;
   pthread_mutex_init(&q->lock, NULL);
   pthread_cond_init(&q->cond, NULL);
   if (pthread_create(&q->thread, NULL, queue_thread, q) != 0) {
      fail("no thread for the queue");
      free(q);
      return NULL;
   }
   return q;
}

EXPORT void
hakios_metal_queue_free(hakios_metal_queue *q)
{
   if (q == NULL)
      return;
   pthread_mutex_lock(&q->lock);
   q->stop = 1;
   pthread_cond_broadcast(&q->cond);
   pthread_mutex_unlock(&q->lock);
   pthread_join(q->thread, NULL);
   free(q);
}

EXPORT void
hakios_metal_queue_set_deferred(hakios_metal_queue *q, int on)
{
   (void)q;
   (void)on;
}

EXPORT hakios_metal_cb *
hakios_metal_cb_new(hakios_metal_queue *q)
{
   if (q == NULL) {
      fail("no queue");
      return NULL;
   }
   hakios_metal_cb *cb = calloc(1, sizeof(*cb));
   cb->queue = q;
   cb->status = HAKIOS_METAL_NOT_ENQUEUED;
   return cb;
}

static int
record(hakios_metal_cb *cb, struct op op)
{
   if (cb->count == cb->room) {
      unsigned room = cb->room ? cb->room * 2 : 8;
      struct op *ops = realloc(cb->ops, room * sizeof(*ops));
      if (ops == NULL) {
         fail("no memory to record");
         return -1;
      }
      cb->ops = ops;
      cb->room = room;
   }
   cb->ops[cb->count++] = op;
   return 0;
}

EXPORT int
hakios_metal_cb_copy(hakios_metal_cb *cb, hakios_metal_buffer *from, uint64_t from_offset, hakios_metal_buffer *to,
                     uint64_t to_offset, uint64_t length)
{
   if (cb == NULL || from == NULL || to == NULL || from_offset + length > (from->length ? from->length : 1) ||
       to_offset + length > (to->length ? to->length : 1)) {
      fail("a copy outside its buffers");
      return -1;
   }
   if (length == 0)
      return 0;
   return record(cb, (struct op) { OP_COPY, from, to, from_offset, to_offset, length, 0 });
}

EXPORT int
hakios_metal_cb_fill(hakios_metal_cb *cb, hakios_metal_buffer *buffer, uint64_t offset, uint64_t length, uint8_t value)
{
   if (cb == NULL || buffer == NULL || offset + length > (buffer->length ? buffer->length : 1)) {
      fail("a fill outside its buffer");
      return -1;
   }
   if (length == 0)
      return 0;
   return record(cb, (struct op) { OP_FILL, NULL, buffer, 0, offset, length, value });
}

EXPORT void
hakios_metal_cb_fail(hakios_metal_cb *cb, const char *why)
{
   if (cb != NULL && cb->error[0] == 0)
      snprintf(cb->error, sizeof(cb->error), "%s", why ? why : "failed");
}

EXPORT void
hakios_metal_cb_commit(hakios_metal_cb *cb, void (*callback)(hakios_metal_cb *, void *), void *context)
{
   hakios_metal_queue *q = cb->queue;

   cb->callback = callback;
   cb->context = context;
   cb->committed = now_ns();
   pthread_mutex_lock(&q->lock);
   cb->status = HAKIOS_METAL_COMMITTED;
   cb->next = NULL;
   if (q->tail)
      q->tail->next = cb;
   else
      q->head = cb;
   q->tail = cb;
   pthread_cond_broadcast(&q->cond);
   pthread_mutex_unlock(&q->lock);
}

EXPORT void
hakios_metal_cb_wait(hakios_metal_cb *cb)
{
   hakios_metal_queue *q = cb->queue;

   pthread_mutex_lock(&q->lock);
   while (cb->status == HAKIOS_METAL_COMMITTED || cb->status == HAKIOS_METAL_SCHEDULED)
      pthread_cond_wait(&q->cond, &q->lock);
   pthread_mutex_unlock(&q->lock);
}

EXPORT enum hakios_metal_status
hakios_metal_cb_status(hakios_metal_cb *cb)
{
   return cb ? cb->status : HAKIOS_METAL_ERROR;
}

EXPORT const char *
hakios_metal_cb_error(hakios_metal_cb *cb)
{
   return cb ? cb->error : "";
}

EXPORT int64_t
hakios_metal_cb_gpu_nanoseconds(hakios_metal_cb *cb)
{
   return cb ? cb->gpu_ns : 0;
}

EXPORT void
hakios_metal_cb_free(hakios_metal_cb *cb)
{
   if (cb == NULL)
      return;
   if (cb->status == HAKIOS_METAL_COMMITTED || cb->status == HAKIOS_METAL_SCHEDULED)
      hakios_metal_cb_wait(cb);
   free(cb->ops);
   free(cb);
}
