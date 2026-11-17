/*
 * B1L-0088: b2l-bicomp -- one AIR module through what the phone does with it, on the Mac:
 *   Scion's AIR front end (air_to_nir, every air-b2l patch), the I/O lowering libhakiosair's finalize_for_gallium
 *   does, bifrost_preprocess_nir and bifrost_compile_shader_nir for a Mali-G76 (gpu_id 0x7211), built with
 *   assertions ON (build-mesa/build-host-pan, buildtype debug) -- then the Bifrost disassembly of the binary.
 * Its purpose: find the shader whose code a Mali job stopped on with INSTR_INVALID_ENC (0x51), from the modules
 * backboardd sent through the log (scripts/extract-b2lmod.py).
 *   b2l-bicomp MODULE.air [--disasm] [--gpu 0xNNNN]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/nir/nir.h"
#include "compiler/nir/nir_builder.h"
#include "compiler/air/nir_air.h"
#include "panfrost/compiler/bifrost_compile.h"
#include "panfrost/compiler/bifrost/disassemble.h"
#include "util/ralloc.h"
#include "util/u_dynarray.h"

static int
type_size_vec4(const struct glsl_type *type, bool bindless)
{
   (void)bindless;
   return glsl_count_attribute_slots(type, false);
}

/* What panfrost's Gallium lowering would give these, which this tool does not reproduce: zero, for an encoding check */
static bool
b2l_zero_gallium_sysvals(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   (void)data;
   switch (intr->intrinsic) {
   case nir_intrinsic_load_rt_conversion_pan:
   case nir_intrinsic_load_viewport_scale:
   case nir_intrinsic_load_viewport_offset:
   case nir_intrinsic_load_base_instance:
   case nir_intrinsic_load_base_vertex:
   case nir_intrinsic_load_first_vertex:
   case nir_intrinsic_load_ssbo_address:
   case nir_intrinsic_load_noperspective_varyings_pan:
      break;
   default:
      return false;
   }
   b->cursor = nir_before_instr(&intr->instr);
   nir_def_replace(&intr->def, nir_imm_zero(b, intr->def.num_components, intr->def.bit_size));
   return true;
}

int
main(int argc, char **argv)
{
   const char *path = NULL;
   bool disasm = false;
   unsigned gpu_id = 0x7211;
   for (int i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--disasm"))
         disasm = true;
      else if (!strcmp(argv[i], "--gpu") && i + 1 < argc)
         gpu_id = strtoul(argv[++i], NULL, 0);
      else
         path = argv[i];
   }
   if (!path) {
      fprintf(stderr, "usage: b2l-bicomp MODULE.air [--disasm] [--gpu 0xNNNN]\n");
      return 2;
   }
   FILE *f = fopen(path, "rb");
   if (!f) {
      perror(path);
      return 2;
   }
   fseek(f, 0, SEEK_END);
   long size = ftell(f);
   fseek(f, 0, SEEK_SET);
   void *data = malloc(size);
   if (fread(data, 1, size, f) != (size_t)size)
      return 2;
   fclose(f);

   glsl_type_singleton_init_or_ref();
   void *ctx = ralloc_context(NULL);
   struct air_constant_value *fc;
   unsigned nfc = air_module_function_constants(ctx, data, size, &fc);
   struct air_to_nir_options opts = { .constants = fc, .num_constants = nfc };
   struct air_shader_info info;
   const char *err = NULL;
   nir_shader *nir = air_to_nir(ctx, data, size, NULL, &opts, &bifrost_nir_options_v6, &info, &err);
   if (!nir) {
      printf("%s: translate FAILED: %s\n", path, err ? err : "?");
      return 1;
   }
   /* what libhakiosair's finalize_for_gallium does before Gallium sees the shader */
   NIR_PASS(_, nir, nir_lower_io, nir_var_shader_in | nir_var_shader_out, type_size_vec4,
            nir_lower_io_use_interpolated_input_intrinsics);
   NIR_PASS(_, nir, nir_lower_system_values);   /* Gallium's state tracker does it on the phone */
   nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));
   bifrost_preprocess_nir(nir, gpu_id);

   NIR_PASS(_, nir, nir_shader_intrinsics_pass, b2l_zero_gallium_sysvals, nir_metadata_control_flow, NULL);
   struct panfrost_compile_inputs inputs = { .gpu_id = gpu_id, .pushable_ubos = 1 };
   struct util_dynarray binary;
   util_dynarray_init(&binary, NULL);
   struct pan_shader_info pinfo;
   memset(&pinfo, 0, sizeof(pinfo));
   bifrost_compile_shader_nir(nir, &inputs, &binary, &pinfo);
   printf("%s: %s, %u bytes of Bifrost code, %u work registers\n", path,
          gl_shader_stage_name(nir->info.stage), binary.size, pinfo.work_reg_count);
   if (disasm)
      disassemble_bifrost(stdout, binary.data, binary.size, false);
   util_dynarray_fini(&binary);
   ralloc_free(ctx);
   return 0;
}
