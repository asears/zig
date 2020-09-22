/*
 * Copyright (c) 2020 Andrew Kelley
 *
 * This file is part of zig, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "stage1.h"
#include "os.hpp"
#include "all_types.hpp"
#include "codegen.hpp"

void zig_stage1_os_init(void) {
    os_init();
    mem::init();
    init_all_targets();
}

struct ZigStage1 *zig_stage1_create(BuildMode optimize_mode,
    const char *main_pkg_path_ptr, size_t main_pkg_path_len,
    const char *root_src_path_ptr, size_t root_src_path_len,
    const char *zig_lib_dir_ptr, size_t zig_lib_dir_len,
    const ZigTarget *target, bool is_test_build)
{
    Buf *main_pkg_path = (main_pkg_path_len == 0) ?
        nullptr : buf_create_from_mem(main_pkg_path_ptr, main_pkg_path_len);
    Buf *root_src_path = buf_create_from_mem(root_src_path_ptr, root_src_path_len);
    Buf *zig_lib_dir = buf_create_from_mem(zig_lib_dir_ptr, zig_lib_dir_len);
    CodeGen *g = codegen_create(main_pkg_path, root_src_path, target, optimize_mode,
            zig_lib_dir, is_test_build);
    return &g->stage1;
}

void zig_stage1_destroy(struct ZigStage1 *stage1) {
    CodeGen *codegen = reinterpret_cast<CodeGen *>(stage1);
    codegen_destroy(codegen);
}

static void add_package(CodeGen *g, ZigStage1Pkg *stage1_pkg, ZigPackage *pkg) {
    for (size_t i = 0; i < stage1_pkg->children_len; i += 1) {
        ZigStage1Pkg *child_cli_pkg = stage1_pkg->children_ptr[i];

        Buf *dirname = buf_alloc();
        Buf *basename = buf_alloc();
        os_path_split(buf_create_from_mem(child_cli_pkg->path_ptr, child_cli_pkg->path_len), dirname, basename);

        ZigPackage *child_pkg = codegen_create_package(g, buf_ptr(dirname), buf_ptr(basename),
                buf_ptr(buf_sprintf("%s.%.*s", buf_ptr(&pkg->pkg_path),
                        (int)child_cli_pkg->name_len, child_cli_pkg->name_ptr)));
        auto entry = pkg->package_table.put_unique(
                buf_create_from_mem(child_cli_pkg->name_ptr, child_cli_pkg->name_len),
                child_pkg);
        if (entry) {
            ZigPackage *existing_pkg = entry->value;
            Buf *full_path = buf_alloc();
            os_path_join(&existing_pkg->root_src_dir, &existing_pkg->root_src_path, full_path);
            fprintf(stderr, "Unable to add package '%.*s'->'%.*s': already exists as '%s'\n",
                    (int)child_cli_pkg->name_len, child_cli_pkg->name_ptr,
                    (int)child_cli_pkg->path_len, child_cli_pkg->path_ptr,
                    buf_ptr(full_path));
            exit(EXIT_FAILURE);
        }

        add_package(g, child_cli_pkg, child_pkg);
    }
}

void zig_stage1_build_object(struct ZigStage1 *stage1) {
    CodeGen *g = reinterpret_cast<CodeGen *>(stage1);

    g->root_out_name = buf_create_from_mem(stage1->root_name_ptr, stage1->root_name_len);
    g->output_dir = buf_create_from_mem(stage1->output_dir_ptr, stage1->output_dir_len);
    if (stage1->builtin_zig_path_len != 0) {
        g->builtin_zig_path = buf_create_from_mem(stage1->builtin_zig_path_ptr, stage1->builtin_zig_path_len);
    }
    if (stage1->test_filter_len != 0) {
        g->test_filter = buf_create_from_mem(stage1->test_filter_ptr, stage1->test_filter_len);
    }
    if (stage1->test_name_prefix_len != 0) {
        g->test_name_prefix = buf_create_from_mem(stage1->test_name_prefix_ptr, stage1->test_name_prefix_len);
    }

    g->link_mode_dynamic = stage1->link_mode_dynamic;
    g->dll_export_fns = stage1->dll_export_fns;
    g->have_pic = stage1->pic;
    g->have_stack_probing = stage1->enable_stack_probing;
    g->is_single_threaded = stage1->is_single_threaded;
    g->valgrind_enabled = stage1->valgrind_enabled;
    g->link_libc = stage1->link_libc;
    g->link_libcpp = stage1->link_libcpp;
    g->function_sections = stage1->function_sections;

    g->subsystem = stage1->subsystem;

    g->enable_time_report = stage1->enable_time_report;
    g->enable_stack_report = stage1->enable_stack_report;
    g->enable_dump_analysis = stage1->dump_analysis;
    g->enable_doc_generation = stage1->enable_doc_generation;
    g->emit_bin = stage1->emit_bin;
    g->emit_asm = stage1->emit_asm;
    g->emit_llvm_ir = stage1->emit_llvm_ir;
    g->test_is_evented = stage1->test_is_evented;

    g->verbose_tokenize = stage1->verbose_tokenize;
    g->verbose_ast = stage1->verbose_ast;
    g->verbose_ir = stage1->verbose_ir;
    g->verbose_llvm_ir = stage1->verbose_llvm_ir;
    g->verbose_cimport = stage1->verbose_cimport;
    g->verbose_llvm_cpu_features = stage1->verbose_llvm_cpu_features;

    g->err_color = stage1->err_color;
    g->code_model = stage1->code_model;

    {
        g->strip_debug_symbols = stage1->strip;
        if (!target_has_debug_info(g->zig_target)) {
            g->strip_debug_symbols = true;
        }
    }

    g->main_progress_node = stage1->main_progress_node;

    add_package(g, stage1->root_pkg, g->main_pkg);

    codegen_build_object(g);
}