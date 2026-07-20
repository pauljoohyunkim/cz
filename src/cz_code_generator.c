#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "cz_code_generator.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)
#define INVALID_NODE_TYPE_TO_GOTO(node, node_type_enum, label) do { if ((node)->node_type != (node_type_enum)) goto label; } while (0)

CZ_Environment_Backend* cz_environment_backend_create(void) {
    CZ_Environment_Backend* env_b = (CZ_Environment_Backend*) calloc(1, sizeof(CZ_Environment_Backend));

    return env_b;
}

void cz_environment_backend_free(CZ_Environment_Backend* env_b) {
    if (env_b != NULL) {
        // Note: We do not free the symbol or type_name as they are owned by symbol table and global type table.
        free(env_b->value_map);
        free(env_b->type_map);
    }
    free(env_b);
}

int cz_environment_backend_push_val_map(CZ_Environment_Backend *env_b, const CZ_Symbol* symbol, LLVMValueRef llvmval) {
    CZ_Symbol_To_LLVMValueRef* new_map = NULL;

    if (env_b == NULL || symbol == NULL) return 0;

    new_map = (CZ_Symbol_To_LLVMValueRef *)realloc(env_b->value_map, sizeof(CZ_Symbol_To_LLVMValueRef) * (env_b->value_count+1));
    if (new_map == NULL) goto error_cleanup;

    env_b->value_map = new_map;
    new_map = NULL;
    env_b->value_map[env_b->value_count].symbol = symbol;
    env_b->value_map[env_b->value_count].ref = llvmval;
    env_b->value_count++;

    return 1;

error_cleanup:
    free(new_map);
    return 0;
}

int cz_environment_backend_push_type_map(CZ_Environment_Backend *env_b, const CZ_Type* type, LLVMTypeRef llvmtype) {
    CZ_Type_To_LLVMTypeRef* new_map = NULL;

    if (env_b == NULL || type == NULL) return 0;

    new_map = (CZ_Type_To_LLVMTypeRef *)realloc(env_b->type_map, sizeof(CZ_Type_To_LLVMTypeRef) * (env_b->type_count+1));
    if (new_map == NULL) goto error_cleanup;

    env_b->type_map = new_map;
    new_map = NULL;
    env_b->type_map[env_b->type_count].type_name = type;
    env_b->type_map[env_b->type_count].ref = llvmtype;
    env_b->type_count++;

    return 1;

error_cleanup:
    free(new_map);
    return 0;
}

const LLVMValueRef cz_environment_backend_lookup(const CZ_Environment_Backend *env_b, const CZ_Symbol* symbol, bool cascade) {
    if (env_b == NULL || symbol == NULL) return NULL;

    // Search the backend's symbol map for a matching entry.
    for (unsigned int i = 0; i < env_b->value_count; ++i) {
        if (env_b->value_map[i].symbol == symbol) {
            return env_b->value_map[i].ref;
        }
    }

    if (env_b->parent != NULL && cascade) {
        return cz_environment_backend_lookup(env_b->parent, symbol, cascade);
    }

    return NULL;
}

const LLVMTypeRef cz_environment_backend_lookup_type(const CZ_Environment_Backend *env_b, const CZ_Type* type, bool cascade) {
    if (env_b == NULL || type == NULL) return NULL;

    // Search the backend's symbol map for a matching entry.
    for (unsigned int i = 0; i < env_b->type_count; ++i) {
        if (env_b->type_map[i].type_name == type) {
            return env_b->type_map[i].ref;
        }
    }

    if (env_b->parent != NULL && cascade) {
        return cz_environment_backend_lookup_type(env_b->parent, type, cascade);
    }

    return NULL;
}

CZ_CodeGenerator* cz_code_generator_create(CZ_SemanticAnalyzer* sa) {
    CZ_CodeGenerator* cg = NULL;
    LLVMContextRef ctx = NULL;
    LLVMModuleRef mod = NULL;
    LLVMBuilderRef builder = NULL;
    CZ_Environment_Backend* env_b = NULL;
    CZ_ErrorList* error_list = NULL;

    if (sa == NULL) goto error_cleanup;

    cg = (CZ_CodeGenerator*) calloc(1, sizeof(CZ_CodeGenerator));
    if (cg == NULL) goto error_cleanup;

    // Context
    ctx = LLVMContextCreate();
    if (ctx == NULL) goto error_cleanup;
    // Transfer ownership
    cg->ctx = ctx;
    ctx = NULL;

    // Module
    mod = LLVMModuleCreateWithNameInContext(sa->filename, cg->ctx);
    if (mod == NULL) goto error_cleanup;
    // Transfer ownership
    cg->mod = mod;
    mod = NULL;

    // Builder
    builder = LLVMCreateBuilderInContext(cg->ctx);
    if (builder == NULL) goto error_cleanup;
    cg->builder = builder;
    builder = NULL;

    // Backend environment.
    env_b = cz_environment_backend_create();
    if (env_b == NULL) goto error_cleanup;
    cg->global_env_b = env_b;
    env_b = NULL;

    // Error List
    error_list = cz_error_list_create();
    if (error_list == NULL) goto error_cleanup;
    cg->error_list = error_list;
    error_list = NULL;

    // Filename is owned by runtime and not CZ.
    cg->filename = sa->filename;

    // Move the info from sa to cg so that it does not cause double free.
    cg->program = sa->program;
    sa->program = NULL;
    cg->global_env = sa->global_env;
    sa->global_env = NULL;
    cg->gtt = sa->gtt;
    sa->gtt = NULL;
    cg->code = sa->code;
    sa->code = NULL;
    cg->sp = sa->sp;
    sa->sp = NULL;

    return cg;

error_cleanup:
    LLVMContextDispose(ctx);
    LLVMDisposeModule(mod);
    LLVMDisposeBuilder(builder);
    cz_environment_backend_free(env_b);
    cz_error_list_free(error_list);
    cz_code_generator_free(cg);
    return NULL;
}

void cz_code_generator_free(CZ_CodeGenerator* cg) {
    if (cg != NULL) {
        LLVMDisposeBuilder(cg->builder);
        LLVMDisposeModule(cg->mod);
        LLVMContextDispose(cg->ctx);
        cz_ast_root_free(cg->program);
        cz_environment_backend_free(cg->global_env_b);
        cz_environment_free(cg->global_env);
        cz_error_list_free(cg->error_list);
        cz_global_type_table_free(cg->gtt);
        free((char*)cg->code);
        cz_string_pool_free(cg->sp);
    }
    free(cg);
    LLVMShutdown();
}

int cz_code_generator_fill_type_map_primitive_opaque_struct(CZ_CodeGenerator* cg);
int cz_code_generator_fill_type_map_complex(CZ_CodeGenerator* cg);

int cz_code_generator_generate(CZ_CodeGenerator* cg) {
    NULL_POINTER_TO_GOTO(cg, error_cleanup);
    NULL_POINTER_TO_GOTO(cg->program, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(cg->program, CZ_AST_ProgramNodeType, error_cleanup);
    NULL_POINTER_TO_GOTO(cg->program->program.global_declaration_list, error_cleanup);

    // Pass 1 to build type map. (Primitive & Opaque Struct)
    if (cz_code_generator_fill_type_map_primitive_opaque_struct(cg) != 1) {
        goto error_cleanup;
    }
    
    // Pass 2 to build type map. (Reference & Struct Body & Function)

    for (unsigned int i = 0; i < cg->program->program.declaration_count; i++) {
        const CZ_AST_Node* statement = cg->program->program.global_declaration_list[i];
        NULL_POINTER_TO_GOTO(statement, error_cleanup);

        // TODO: error checking
        switch (statement->node_type) {
            case CZ_AST_FunctionDeclarationNodeType:
                //cz_code_generator_declare_function(cg, cg->global_env, cg->global_env_b, statement);
                break;
            case CZ_AST_StructDeclarationNodeType:
                break;
            case CZ_AST_VariableDeclarationNodeType:
                //cz_code_generator_declare_global_variable(cg, cg->global_env, cg->global_env_b, statement);
                break;
            case CZ_AST_TypedefDeclarationNodeType:
            case CZ_AST_NewtypeDeclarationNodeType:
                // Do nothing.
                break;
            default:
                break;
        }
    }

    //for (unsigned int i = 0; i < cg->program->program.declaration_count; i++) {
    //    const CZ_AST_Node* statement = cg->program->program.global_declaration_list[i];
    //    NULL_POINTER_TO_GOTO(statement, error_cleanup);

    //    if (statement->node_type != CZ_AST_FunctionDeclarationNodeType) {
    //        continue;
    //    }

    //    cz_code_generator_declare_function_body(cg, cg->global_env, cg->global_env_b, statement);
    //}

    return 1;
error_cleanup:
    return 0;
}

static inline LLVMTypeRef cz_backend_lower_type(CZ_CodeGenerator* cg, const CZ_Type* type) {
    NULL_POINTER_TO_GOTO(cg, error_cleanup);
    NULL_POINTER_TO_GOTO(type, error_cleanup);
    switch (type->kind) {
        case CZ_TYPE_KIND_PRIMITIVE:
            switch (type->primitive) {
                case CZ_PRIMITIVE_BOOL:
                    return LLVMInt1TypeInContext(cg->ctx);
                case CZ_PRIMITIVE_FLOAT:
                    return LLVMFloatTypeInContext(cg->ctx);
                case CZ_PRIMITIVE_INT32:
                    return LLVMInt32TypeInContext(cg->ctx);
                case CZ_PRIMITIVE_VOID:
                    return LLVMVoidTypeInContext(cg->ctx);
                default:
                    break;
            }
            break;
        case CZ_TYPE_KIND_NEWTYPE:
            return cz_backend_lower_type(cg, type->newtype.underlying);
        case CZ_TYPE_KIND_STRUCT:
            return LLVMStructCreateNamed(cg->ctx, type->structure.name);
        case CZ_TYPE_KIND_REFERENCE:
            // TODO: Opaque pointer 
            return LLVMPointerTypeInContext(cg->ctx, 0);
        default:
            break;
    }

    return NULL;
error_cleanup:
    return NULL;
}

int cz_code_generator_fill_type_map_primitive_opaque_struct(CZ_CodeGenerator* cg) {
    NULL_POINTER_TO_GOTO(cg, error_cleanup);
    NULL_POINTER_TO_GOTO(cg->gtt, error_cleanup);
    NULL_POINTER_TO_GOTO(cg->global_env_b, error_cleanup);

    for (unsigned int i = 0; i < cg->gtt->all_entry_count; i++) {
        const CZ_Type* type = cg->gtt->all_allocations[i];
        // For each type,
        // 1. First of all, skip references.
        if (type->kind == CZ_TYPE_KIND_REFERENCE) {
            continue;
        }
        
        // 2. Then drop constness.
        const CZ_Type* decayed_type = cz_type_decay_type(type);

        // 3. Check if newtype, primitive, or struct.
        if (decayed_type->kind != CZ_TYPE_KIND_PRIMITIVE &&
            decayed_type->kind != CZ_TYPE_KIND_STRUCT &&
            decayed_type->kind != CZ_TYPE_KIND_NEWTYPE) {
            continue;
        }

        // 4. Look up if mapping exists, and if it exists, skip.
        LLVMTypeRef llvm_type_lookup = cz_environment_backend_lookup_type(cg->global_env_b, decayed_type, false);
        if (llvm_type_lookup != NULL) {
            continue;
        }

        // 5. If newtype, check if it is primitive or struct, then query again with inner type.
        if (decayed_type->kind == CZ_TYPE_KIND_NEWTYPE) {
            const CZ_Type* underlying = decayed_type->newtype.underlying;
            // 5.1 If underlying primitive/struct is not in map, build it.
            LLVMTypeRef llvm_underlying_type = cz_environment_backend_lookup_type(cg->global_env_b, underlying, false);
            if (llvm_underlying_type == NULL) {
                llvm_underlying_type = cz_backend_lower_type(cg, underlying);
                NULL_POINTER_TO_GOTO(llvm_underlying_type, error_cleanup);

                if (cz_environment_backend_push_type_map(cg->global_env_b, underlying, llvm_underlying_type) != 1) {
                    cz_error_list_push_error(cg->error_list, cg->filename, 0, 0, "Failed to register underlying type.");
                    goto error_cleanup;
                }
            }
            // Now safely map the newtype alias to that exact same machine type.
            if (cz_environment_backend_push_type_map(cg->global_env_b, decayed_type, llvm_underlying_type) != 1) {
                cz_error_list_push_error(cg->error_list, cg->filename, 0, 0, "Failed to register newtype alias.");
                goto error_cleanup;
            }
            continue;
        }

        // 6. Handle Standard Primitives & Struct Shells.
        const LLVMTypeRef llvm_machine_type = cz_backend_lower_type(cg, decayed_type);
        NULL_POINTER_TO_GOTO(llvm_machine_type, error_cleanup);
        
        if (cz_environment_backend_push_type_map(cg->global_env_b, decayed_type, llvm_machine_type) != 1) {
            cz_error_list_push_error(cg->error_list, cg->filename, 0, 0, "Could not register type to LLVM.");
            goto error_cleanup;
        }
    }

    return 1;
error_cleanup:
    return 0;
}

int cz_code_generator_fill_type_map_complex(CZ_CodeGenerator* cg) {
    LLVMTypeRef* llvm_types = NULL;
    NULL_POINTER_TO_GOTO(cg, error_cleanup);
    NULL_POINTER_TO_GOTO(cg->gtt, error_cleanup);

    // Fill reference, structs, and functions
    for (unsigned int i = 0; i < cg->gtt->all_entry_count; i++) {
        const CZ_Type* type = cg->gtt->all_allocations[i];
        
        // 1. For struct, fill the inner members.
        if (type->kind == CZ_TYPE_KIND_STRUCT) {
            LLVMTypeRef llvm_struct_type = cz_environment_backend_lookup_type(cg->global_env_b, type, false);
            NULL_POINTER_TO_GOTO(llvm_struct_type, error_cleanup);

            llvm_types = (LLVMTypeRef*) calloc(type->structure.layout->field_count, sizeof(LLVMTypeRef));
            NULL_POINTER_TO_GOTO(llvm_types, error_cleanup);

            for (unsigned int j = 0; j < type->structure.layout->field_count; j++) {
                const CZ_Type* member_type = type->structure.layout->fields[j].type;
                LLVMTypeRef llvm_member_type = cz_environment_backend_lookup_type(cg->global_env_b, member_type, false);
                if (llvm_member_type == NULL) {
                    // Check if it is a reference.
                    if (member_type->kind == CZ_TYPE_KIND_REFERENCE) {
                        llvm_member_type = cz_backend_lower_type(cg, member_type);
                        if (cz_environment_backend_push_type_map(cg->global_env_b, member_type, llvm_member_type) != 1) {
                            goto error_cleanup;
                        }
                    }
                }

                llvm_types[j] = llvm_member_type;
            }
            LLVMStructSetBody(llvm_struct_type, llvm_types, type->structure.layout->field_count, 0);
        }
        
    }

    return 1;

error_cleanup:
    free(llvm_types);
    return 0;
}

int cz_code_generator_emit_object_file(LLVMModuleRef module, const char* output_filename) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    char* target_triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(module, target_triple);

    LLVMTargetRef target;
    char* error = NULL;
    if (LLVMGetTargetFromTriple(target_triple, &target, &error)) {
        fprintf(stderr, "LLVM Target Error: %s\n", error);
        LLVMDisposeMessage(error);
        LLVMDisposeMessage(target_triple);
        return 0;
    }

    LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(
        target,
        target_triple,
        "generic",
        "",
        LLVMCodeGenLevelDefault,
        LLVMRelocPIC,
        LLVMCodeModelDefault
    );

    LLVMTargetDataRef data_layout = LLVMCreateTargetDataLayout(target_machine);
    LLVMSetModuleDataLayout(module, data_layout);

    if (LLVMTargetMachineEmitToFile(target_machine, module, (char*)output_filename, LLVMObjectFile, &error)) {
        fprintf(stderr, "Failed to emit object file: %s\n", error);
        LLVMDisposeMessage(error);
        LLVMDisposeTargetData(data_layout);
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeMessage(target_triple);
        return 0;
    }

    LLVMDisposeTargetData(data_layout);
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeMessage(target_triple);
    return 1;
}
