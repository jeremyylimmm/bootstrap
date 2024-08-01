#include <stdio.h>

#include "frontend.h"
#include "containers.h"

static ScratchLibrary scratch_library;

ScratchLibrary* get_global_scratch_library() {
    return &scratch_library;
}

int main() {
    Arena* arena = arena_new();
    scratch_library = scratch_library_new();
    
    char* source_path = "examples/test.bs";

    FILE* file;
    if (fopen_s(&file, source_path, "r")) {
        printf("Failed to read '%s'\n", source_path);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t file_length = ftell(file);
    rewind(file);

    char* source = arena_push(arena, (file_length + 1) * sizeof(char));
    size_t source_length = fread(source, 1, file_length, file);
    source[source_length] = '\0';

    HIR_Proc* proc = parse_source(arena, source, source_path);
    if (!proc) { return 1; }
    
    hir_print(proc, "main");

    SB_Context* sbc = sb_init();
    SB_Proc* ll_proc = hir_lower(sbc, proc);
    sb_opt(sbc, ll_proc);

    sb_graphviz(ll_proc);
    sb_generate_win64(sbc, ll_proc);

    return 0;
}