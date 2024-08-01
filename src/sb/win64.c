#include "internal.h"

void sb_generate_win64(SB_Context* ctx, SB_Proc* proc) {
    Scratch* scratch = scratch_get(&ctx->scratch_lib, 0, 0);

    schedule(scratch->arena, proc);

    scratch_release(scratch);
}