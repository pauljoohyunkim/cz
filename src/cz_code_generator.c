#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "cz_code_generator.h"

#define NULL_POINTER_ERROR_HANDLE(ptr) do { if ((ptr) == NULL) goto error_cleanup; } while (0)
#define INVALID_NODE_TYPE_ERROR_HANDLE(node, node_type_enum) do { if ((node)->node_type != (node_type_enum)) goto error_cleanup; } while (0)

static CZ_Environment_Backend* cz_environment_backend_enter_scope(const CZ_Environment_Backend* current_env_b) {
    CZ_Environment_Backend* child_scope = NULL;
    NULL_POINTER_ERROR_HANDLE(current_env_b);

    child_scope = cz_environment_backend_create();
    NULL_POINTER_ERROR_HANDLE(child_scope);

    child_scope->parent = current_env_b;

    return child_scope;

error_cleanup:
    cz_environment_backend_free(child_scope);
    return NULL;
}

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

    // Const unwrapping (avoid recursion by loop)
    while (type->kind == CZ_TYPE_KIND_CONST) {
        type = type->const_of;
    }

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

const LLVMValueRef cz_environment_backend_lookup_val(const CZ_Environment_Backend *env_b, const CZ_Symbol* symbol, bool cascade) {
    if (env_b == NULL || symbol == NULL) return NULL;

    // Search the backend's symbol map for a matching entry.
    for (unsigned int i = 0; i < env_b->value_count; ++i) {
        if (env_b->value_map[i].symbol == symbol) {
            return env_b->value_map[i].ref;
        }
    }

    if (env_b->parent != NULL && cascade) {
        return cz_environment_backend_lookup_val(env_b->parent, symbol, cascade);
    }

    return NULL;
}

const LLVMTypeRef cz_environment_backend_lookup_type(const CZ_CodeGenerator* cg, const CZ_Type* type) {
    if (cg == NULL || type == NULL) return NULL;

    // Const unwrapping (Avoid recursion by loop)
    while (type->kind == CZ_TYPE_KIND_CONST) {
        type = type->const_of;
    }

    // Search the global environment's type map for a matching entry.
    CZ_Environment_Backend* env_b = cg->global_env_b;
    for (unsigned int i = 0; i < env_b->type_count; ++i) {
        if (env_b->type_map[i].type_name == type) {
            return env_b->type_map[i].ref;
        }
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

static int cz_code_generator_fill_type_map_primitive_opaque_struct(CZ_CodeGenerator* cg);
static int cz_code_generator_fill_type_map_complex(CZ_CodeGenerator* cg);

static int cz_code_generator_emit_function(CZ_CodeGenerator* cg, const CZ_Environment* env, CZ_Environment_Backend* env_b, const CZ_AST_Node* stmt);
static int cz_code_generator_emit_global_variable(CZ_CodeGenerator* cg, const CZ_Environment* env, CZ_Environment_Backend* env_b, const CZ_AST_Node* stmt);
static int cz_code_generator_generate_function_body(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);


static int cz_code_generator_generate_statement(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);
static int cz_code_generator_generate_return_statement(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);

static LLVMValueRef cz_code_generator_generate_lvalue(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node);

static LLVMValueRef cz_code_generator_generate_expr(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);
static LLVMValueRef cz_code_generator_generate_expr_binary(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);
static LLVMValueRef cz_code_generator_generate_expr_unary(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);
static LLVMValueRef cz_code_generator_generate_expr_cast(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);
static LLVMValueRef cz_code_generator_generate_expr_literal(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);
static LLVMValueRef cz_code_generator_generate_expr_identifier(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);
static LLVMValueRef cz_code_generator_generate_expr_struct_init(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time);

int cz_code_generator_generate(CZ_CodeGenerator* cg) {
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(cg->program);
    INVALID_NODE_TYPE_ERROR_HANDLE(cg->program, CZ_AST_ProgramNodeType);
    NULL_POINTER_ERROR_HANDLE(cg->program->program.global_declaration_list);

    // Pass 1 to build type map. (Primitive & Opaque Struct & Reference)
    if (cz_code_generator_fill_type_map_primitive_opaque_struct(cg) != 1) {
        goto error_cleanup;
    }
    
    // Pass 2 to build type map. (Struct Body & Function)
    if (cz_code_generator_fill_type_map_complex(cg) != 1) {
        goto error_cleanup;
    }

    for (unsigned int i = 0; i < cg->program->program.declaration_count; i++) {
        const CZ_AST_Node* statement = cg->program->program.global_declaration_list[i];
        NULL_POINTER_ERROR_HANDLE(statement);

        // TODO: error checking
        switch (statement->node_type) {
            case CZ_AST_FunctionDeclarationNodeType:
                cz_code_generator_emit_function(cg, cg->global_env, cg->global_env_b, statement);
                break;
            case CZ_AST_StructDeclarationNodeType:
                break;
            case CZ_AST_VariableDeclarationNodeType:
                cz_code_generator_emit_global_variable(cg, cg->global_env, cg->global_env_b, statement);
                break;
            case CZ_AST_TypedefDeclarationNodeType:
            case CZ_AST_NewtypeDeclarationNodeType:
                // Do nothing.
                break;
            default:
                break;
        }
    }

    for (unsigned int i = 0; i < cg->program->program.declaration_count; i++) {
        const CZ_AST_Node* statement = cg->program->program.global_declaration_list[i];
        NULL_POINTER_ERROR_HANDLE(statement);

        if (statement->node_type != CZ_AST_FunctionDeclarationNodeType) {
            continue;
        }

        const char* func_name = statement->function_declaration.function_identifier->identifier.name;

        if (cz_code_generator_generate_function_body(cg, cg->global_env, cg->global_env_b, statement, false) != 1) {
            cz_error_list_push_error(cg->error_list, cg->filename, statement->line, statement->col, "Function \"%s\" has a problem in body generation.", func_name);
        }
    }

    return 1;
error_cleanup:
    return 0;
}

static inline LLVMTypeRef cz_backend_lower_type(CZ_CodeGenerator* cg, const CZ_Type* type) {
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(type);
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
        case CZ_TYPE_KIND_CONST:
            return cz_backend_lower_type(cg, type->const_of);
        case CZ_TYPE_KIND_STRUCT:
            return LLVMStructCreateNamed(cg->ctx, type->structure.name);
        case CZ_TYPE_KIND_REFERENCE:
            return LLVMPointerTypeInContext(cg->ctx, 0);
        default:
            break;
    }

    return NULL;
error_cleanup:
    return NULL;
}

/**
 * @brief This creates full mapping for primitives
 * 
 * @param cg 
 * @return int 
 */
static int cz_code_generator_fill_type_map_primitive_opaque_struct(CZ_CodeGenerator* cg) {
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(cg->gtt);
    NULL_POINTER_ERROR_HANDLE(cg->global_env_b);

    for (unsigned int i = 0; i < cg->gtt->all_entry_count; i++) {
        const CZ_Type* type = cg->gtt->all_allocations[i];

        // Skip top-level const wrappers and functions in Pass 1
        // Since GTT registers base types before const types, const types can be ignored.
        if (type->kind == CZ_TYPE_KIND_CONST || type->kind == CZ_TYPE_KIND_FUNCTION) {
            continue;
        }

        // Lookup base type
        if (cz_environment_backend_lookup_type(cg, type) != NULL) {
            continue;
        }

        // Lower and insert
        LLVMTypeRef llvm_type = cz_backend_lower_type(cg, type);
        NULL_POINTER_ERROR_HANDLE(llvm_type);

        if (cz_environment_backend_push_type_map(cg->global_env_b, type, llvm_type) != 1) {
            goto error_cleanup;
        }
    }

    return 1;
error_cleanup:
    return 0;
}

static int cz_code_generator_fill_type_map_complex(CZ_CodeGenerator* cg) {
    LLVMTypeRef* llvm_types = NULL;
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(cg->gtt);

    for (unsigned int i = 0; i < cg->gtt->all_entry_count; i++) {
        const CZ_Type* type = cg->gtt->all_allocations[i];

        switch (type->kind) {
            // Structs Members
            case CZ_TYPE_KIND_STRUCT: {
                LLVMTypeRef llvm_struct_type = cz_environment_backend_lookup_type(cg, type);
                NULL_POINTER_ERROR_HANDLE(llvm_struct_type);

                unsigned int field_count = type->structure.layout->field_count;
                if (field_count > 0) {
                    llvm_types = (LLVMTypeRef*) calloc(field_count, sizeof(LLVMTypeRef));
                    NULL_POINTER_ERROR_HANDLE(llvm_types);

                    for (unsigned int j = 0; j < field_count; j++) {
                        const CZ_Type* member_type = type->structure.layout->fields[j].type;

                        // Lookup automatically unwraps const and handles references
                        LLVMTypeRef llvm_member_type = cz_environment_backend_lookup_type(cg, member_type);
                        
                        if (llvm_member_type == NULL) {
                            cz_error_list_push_error(cg->error_list, cg->filename, 0, 0, 
                                "Developer Error: Struct field type missing from backend map.");
                            goto error_cleanup;
                        }

                        llvm_types[j] = llvm_member_type;
                    }
                }

                LLVMStructSetBody(llvm_struct_type, llvm_types, field_count, 0);
                
                free(llvm_types);
                llvm_types = NULL;
                break;
            }

            case CZ_TYPE_KIND_FUNCTION: {
                // Return Type
                LLVMTypeRef llvm_ret_type = cz_environment_backend_lookup_type(cg, type->function.return_type);
                NULL_POINTER_ERROR_HANDLE(llvm_ret_type);

                // Parameters
                unsigned int param_count = type->function.param_count;
                if (param_count > 0) {
                    llvm_types = (LLVMTypeRef*) calloc(param_count, sizeof(LLVMTypeRef));
                    NULL_POINTER_ERROR_HANDLE(llvm_types);

                    for (unsigned int j = 0; j < param_count; j++) {
                        const CZ_Type* param_type = type->function.param_types[j];
                        LLVMTypeRef llvm_param_type = cz_environment_backend_lookup_type(cg, param_type);
                        NULL_POINTER_ERROR_HANDLE(llvm_param_type);
                        
                        llvm_types[j] = llvm_param_type;
                    }
                }

                LLVMTypeRef llvm_func_type = LLVMFunctionType(llvm_ret_type, llvm_types, param_count, 0);
                NULL_POINTER_ERROR_HANDLE(llvm_func_type);

                if (cz_environment_backend_push_type_map(cg->global_env_b, type, llvm_func_type) != 1) {
                    goto error_cleanup;
                }

                free(llvm_types);
                llvm_types = NULL;
                break;
            }

            case CZ_TYPE_KIND_PRIMITIVE:
            case CZ_TYPE_KIND_REFERENCE:
            case CZ_TYPE_KIND_CONST:
            case CZ_TYPE_KIND_NEWTYPE:
                // Nothing to do in Pass 2 for these kinds (already in map)
                break;

            default:
                cz_error_list_push_error(cg->error_list, cg->filename, 0, 0, 
                    "Developer Error: Unexpected type kind in cz_code_generator_fill_type_map_complex.");
                goto error_cleanup;
        }
    }

    return 1;

error_cleanup:
    free(llvm_types);
    return 0;
}

static int cz_code_generator_emit_function(CZ_CodeGenerator* cg, const CZ_Environment* env, CZ_Environment_Backend* env_b, const CZ_AST_Node* stmt) {
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(env_b);
    NULL_POINTER_ERROR_HANDLE(stmt);
    INVALID_NODE_TYPE_ERROR_HANDLE(stmt, CZ_AST_FunctionDeclarationNodeType);

    const char* func_name = stmt->function_declaration.function_identifier->identifier.name;
    const CZ_Symbol* func_symbol = cz_environment_lookup(env, func_name, true);

    const CZ_Type* func_type = func_symbol->data.value.type;
    LLVMTypeRef llvm_func_type = cz_environment_backend_lookup_type(cg, func_type);

    LLVMValueRef llvm_func_val = LLVMAddFunction(cg->mod, func_name, llvm_func_type);

    if (cz_environment_backend_push_val_map(env_b, func_symbol, llvm_func_val) != 1) {
        goto error_cleanup;
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_code_generator_emit_global_variable(CZ_CodeGenerator* cg, const CZ_Environment* env, CZ_Environment_Backend* env_b, const CZ_AST_Node* stmt) {
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(env_b);
    NULL_POINTER_ERROR_HANDLE(stmt);
    INVALID_NODE_TYPE_ERROR_HANDLE(stmt, CZ_AST_VariableDeclarationNodeType);

    const char* var_name = stmt->variable_declaration.identifier->identifier.name;
    const CZ_Symbol* var_symbol = cz_environment_lookup(env, var_name, false);
    NULL_POINTER_ERROR_HANDLE(var_symbol);

    const CZ_Type* var_type = var_symbol->data.value.type;
    LLVMTypeRef llvm_var_type = cz_environment_backend_lookup_type(cg, var_type);

    LLVMValueRef llvm_global_var = LLVMAddGlobal(cg->mod, llvm_var_type, var_name);

    // TODO: Add initializer from RHS if it exists.
    if (stmt->variable_declaration.expression == NULL) {
        LLVMSetInitializer(llvm_global_var, LLVMConstNull(llvm_var_type));
    } else {
        LLVMValueRef llvm_initializer = cz_code_generator_generate_expr(cg, env, env_b, stmt->variable_declaration.expression, true);
        if (llvm_initializer == NULL) {
            cz_error_list_push_error(cg->error_list, cg->filename, stmt->line, stmt->col, "Could not get initializer for %s", var_name);
            goto error_cleanup;
        }
        LLVMSetInitializer(llvm_global_var, llvm_initializer);
    }

    if (cz_environment_backend_push_val_map(env_b, var_symbol, llvm_global_var) != 1) {
        cz_error_list_push_error(cg->error_list, cg->filename, stmt->line, stmt->col, "Could not create map.");
        goto error_cleanup;
    }

    return 1;
error_cleanup:
    return 0;
}

static int cz_code_generator_generate_function_body(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    CZ_Environment_Backend* body_env_b;
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(env_b);
    NULL_POINTER_ERROR_HANDLE(node);
    INVALID_NODE_TYPE_ERROR_HANDLE(node, CZ_AST_FunctionDeclarationNodeType);

    const char* func_name = node->function_declaration.function_identifier->identifier.name;
    const CZ_Symbol* func_symbol = cz_environment_lookup(env, func_name, true);
    cg->current_function_return = func_symbol->data.value.type->function.return_type;

    LLVMValueRef llvm_func = cz_environment_backend_lookup_val(env_b, func_symbol, true);
    
    LLVMBasicBlockRef llvm_entry_block = LLVMAppendBasicBlockInContext(cg->ctx, llvm_func, "entry");
    LLVMPositionBuilderAtEnd(cg->builder, llvm_entry_block);

    body_env_b = cz_environment_backend_enter_scope(env_b);
    NULL_POINTER_ERROR_HANDLE(body_env_b);

    // Populating environment with parameters.
    unsigned int param_count = func_symbol->data.value.type->function.param_count;
    for (unsigned int i = 0; i < param_count; i++) {
        LLVMValueRef llvm_param_val = LLVMGetParam(llvm_func, i);
        const char* param_name = node->function_declaration.function.parameter_list->parameter_list.params[i]->variable_declaration.identifier->identifier.name;
        const CZ_Symbol* param_symbol = cz_environment_lookup(node->function_declaration.function.parameter_list->parameter_list.scope, param_name, false);
        const CZ_Type* param_type = func_symbol->data.value.type->function.param_types[i];
        LLVMTypeRef llvm_param_type = cz_environment_backend_lookup_type(cg, param_type);

        // TODO: Deal with references
        LLVMValueRef llvm_alloca_slot = LLVMBuildAlloca(cg->builder, llvm_param_type, param_name);
        LLVMBuildStore(cg->builder, llvm_param_val, llvm_alloca_slot);

        if (cz_environment_backend_push_val_map(body_env_b, param_symbol, llvm_alloca_slot) != 1) {
            goto error_cleanup;
        }
    }

    unsigned int stmt_count = node->function_declaration.body->statement_list.statement_count;
    const CZ_AST_Node** stmt_list = node->function_declaration.body->statement_list.statements;
    for (unsigned int i = 0; i < stmt_count; i++) {
        if (cz_code_generator_generate_statement(cg, node->function_declaration.body->statement_list.scope, body_env_b, stmt_list[i], false) != 1) {
            cz_error_list_push_error(cg->error_list, cg->filename, stmt_list[i]->line, stmt_list[i]->col, "Could not generate statement index %i for function \"%s\"", i, func_name);
            goto error_cleanup;
        }
    }

    cz_environment_backend_free(body_env_b);
    cg->current_function_return = NULL;
    return 1;

error_cleanup:
    cz_environment_backend_free(body_env_b);
    cg->current_function_return = NULL;
    return 0;
}

static int cz_code_generator_generate_statement(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(env_b);

    switch (node->node_type) {
        case CZ_AST_VariableDeclarationNodeType:
            goto error_cleanup;
        case CZ_AST_AssignmentStatementNodeType:
            goto error_cleanup;
        case CZ_AST_ReturnStatementNodeType:
            cz_code_generator_generate_return_statement(cg, env, env_b, node, false);
            goto error_cleanup;
        case CZ_AST_IfStatementNodeType:
            goto error_cleanup;
        case CZ_AST_ForStatementNodeType:
            goto error_cleanup;
        case CZ_AST_WhileStatementNodeType:
            goto error_cleanup;
        case CZ_AST_BlockStatementNodeType:
            goto error_cleanup;
        default:
            goto error_cleanup;
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_code_generator_generate_return_statement(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(env_b);
    NULL_POINTER_ERROR_HANDLE(node);
    NULL_POINTER_ERROR_HANDLE(cg->current_function_return);

    if (cg->current_function_return->kind == CZ_TYPE_KIND_REFERENCE) {
        // TODO: Generate l-value here.
        goto error_cleanup;
    } else {
        LLVMValueRef llvm_return_val_ref = cz_code_generator_generate_expr(cg, env, env_b, node->return_statement.expression, is_compile_time);
        LLVMBuildRet(cg->builder, llvm_return_val_ref);
    }

    return 1;
error_cleanup:
    return 0;
}

static LLVMValueRef cz_code_generator_generate_lvalue(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node) {
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(env_b);

    LLVMValueRef llvm_val = NULL;

    switch (node->node_type) {
        case CZ_AST_IdentifierNodeType:
            goto error_cleanup;
        case CZ_AST_StructMemberAccessNodeType:
            goto error_cleanup;
        case CZ_AST_FunctionCallNodeType:
            goto error_cleanup;
    }

    return llvm_val;

error_cleanup:
    return NULL;
}

static LLVMValueRef cz_code_generator_generate_expr(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    NULL_POINTER_ERROR_HANDLE(cg);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(env_b);
    NULL_POINTER_ERROR_HANDLE(node);

    LLVMValueRef llvm_val = NULL;

    switch (node->node_type) {
        case CZ_AST_BinaryExpressionNodeType:
            llvm_val = cz_code_generator_generate_expr_binary(cg, env, env_b, node, is_compile_time);
            break;
        case CZ_AST_UnaryExpressionNodeType:
            llvm_val = cz_code_generator_generate_expr_unary(cg, env, env_b, node, is_compile_time);
            break;
        case CZ_AST_CastExpressionNodeType:
            llvm_val = cz_code_generator_generate_expr_cast(cg, env, env_b, node, is_compile_time);
            break;
        case CZ_AST_LiteralNodeType:
            llvm_val = cz_code_generator_generate_expr_literal(cg, env, env_b, node, is_compile_time);
            break;
        case CZ_AST_IdentifierNodeType:
            if (is_compile_time) {
                goto error_cleanup;
            }
            llvm_val = cz_code_generator_generate_expr_identifier(cg, env, env_b, node, is_compile_time);
            break;
        case CZ_AST_StructInitNodeType:
            llvm_val = cz_code_generator_generate_expr_struct_init(cg, env, env_b, node, is_compile_time);
            break;
        default:
            goto error_cleanup;
    }

    return llvm_val;

error_cleanup:
    return NULL;
}

static LLVMValueRef cz_code_generator_generate_expr_binary(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    LLVMValueRef llvm_val = NULL;

    LLVMValueRef llvm_lhs = cz_code_generator_generate_expr(cg, env, env_b, node->binary_expression.left, is_compile_time);
    LLVMValueRef llvm_rhs = cz_code_generator_generate_expr(cg, env, env_b, node->binary_expression.right, is_compile_time);
    LLVMTypeRef llvm_lhs_type = cz_environment_backend_lookup_type(cg, node->binary_expression.left->decoration->resolved_type);
    LLVMTypeRef llvm_rhs_type = cz_environment_backend_lookup_type(cg, node->binary_expression.right->decoration->resolved_type);
    LLVMTypeKind llvm_lhs_type_kind = LLVMGetTypeKind(llvm_lhs_type);
    LLVMTypeKind llvm_rhs_type_kind = LLVMGetTypeKind(llvm_rhs_type);
    switch (node->binary_expression.op) {
        case CZ_TT_PLUS:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFAdd(cg->builder, llvm_lhs, llvm_rhs, "faddtmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildAdd(cg->builder, llvm_lhs, llvm_rhs, "addtmp");
            }
            break;
        case CZ_TT_MINUS:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFSub(cg->builder, llvm_lhs, llvm_rhs, "fsubtmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildSub(cg->builder, llvm_lhs, llvm_rhs, "subtmp");
            }
            break;
        case CZ_TT_STAR:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFMul(cg->builder, llvm_lhs, llvm_rhs, "fmultmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildMul(cg->builder, llvm_lhs, llvm_rhs, "multmp");
            }
            break;
        case CZ_TT_SLASH:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFDiv(cg->builder, llvm_lhs, llvm_rhs, "fdivtmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildSDiv(cg->builder, llvm_lhs, llvm_rhs, "divtmp");
            }
            break;
        case CZ_TT_PERCENT:
            if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildSRem(cg->builder, llvm_lhs, llvm_rhs, "remtmp");
            }
            break;
        case CZ_TT_AMPERSAND:
            if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildAnd(cg->builder, llvm_lhs, llvm_rhs, "andtmp");
            }
            break;
        case CZ_TT_BAR:
            if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildOr(cg->builder, llvm_lhs, llvm_rhs, "ortmp");
            }
            break;
        case CZ_TT_CARET:
            if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildXor(cg->builder, llvm_lhs, llvm_rhs, "xortmp");
            }
            break;
        case CZ_TT_GREATER:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFCmp(cg->builder, LLVMRealOGT, llvm_lhs, llvm_rhs, "fgttmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildICmp(cg->builder, LLVMIntSGT, llvm_lhs, llvm_rhs, "gttmp");
            }
            break;
        case CZ_TT_LESS:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFCmp(cg->builder, LLVMRealOLT, llvm_lhs, llvm_rhs, "flttmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildICmp(cg->builder, LLVMIntSLT, llvm_lhs, llvm_rhs, "lttmp");
            }
            break;
        case CZ_TT_GREATER_EQUAL:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFCmp(cg->builder, LLVMRealOGE, llvm_lhs, llvm_rhs, "fgetmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildICmp(cg->builder, LLVMIntSGE, llvm_lhs, llvm_rhs, "getmp");
            }
            break;
        case CZ_TT_LESS_EQUAL:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFCmp(cg->builder, LLVMRealOLE, llvm_lhs, llvm_rhs, "fletmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildICmp(cg->builder, LLVMIntSLE, llvm_lhs, llvm_rhs, "letmp");
            }
            break;
        case CZ_TT_EQUAL_EQUAL:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFCmp(cg->builder, LLVMRealOEQ, llvm_lhs, llvm_rhs, "feqtmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildICmp(cg->builder, LLVMIntEQ, llvm_lhs, llvm_rhs, "eqtmp");
            }
            break;
        case CZ_TT_EXCLAMATION_EQUAL:
            if (llvm_lhs_type_kind == LLVMFloatTypeKind && llvm_rhs_type_kind == LLVMFloatTypeKind) {
                llvm_val = LLVMBuildFCmp(cg->builder, LLVMRealONE, llvm_lhs, llvm_rhs, "fneqtmp");
            } else if (llvm_lhs_type_kind == LLVMIntegerTypeKind && llvm_rhs_type_kind == LLVMIntegerTypeKind) {
                llvm_val = LLVMBuildICmp(cg->builder, LLVMIntNE, llvm_lhs, llvm_rhs, "neqtmp");
            }
            break;
        default:
            goto error_cleanup;
    }

    return llvm_val;

error_cleanup:
    return NULL;
}

static LLVMValueRef cz_code_generator_generate_expr_unary(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    LLVMValueRef llvm_val = NULL;

    LLVMValueRef llvm_operand = cz_code_generator_generate_expr(cg, env, env_b, node->unary_expression.operand, is_compile_time);
    LLVMTypeRef llvm_operand_type = cz_environment_backend_lookup_type(cg, node->unary_expression.operand->decoration->resolved_type);
    LLVMTypeKind llvm_operand_type_kind = LLVMGetTypeKind(llvm_operand_type);
    switch (node->unary_expression.op) {
        case CZ_TT_MINUS:
            switch (llvm_operand_type_kind) {
                case LLVMFloatTypeKind:
                    {
                        const LLVMValueRef zero_const = LLVMConstReal(LLVMFloatTypeInContext(cg->ctx), 0);
                        llvm_val = LLVMBuildFSub(cg->builder, zero_const, llvm_operand, "fnegtmp");
                    }
                    break;
                case LLVMIntegerTypeKind:
                    {
                        const LLVMValueRef zero_const = LLVMConstInt(LLVMInt32TypeInContext(cg->ctx), 0, true);
                        llvm_val = LLVMBuildSub(cg->builder, zero_const, llvm_operand, "negtmp");
                    }
                    break;
            }
            break;
        case CZ_TT_EXCLAMATION:
            {
                if (llvm_operand_type_kind == LLVMIntegerTypeKind) {
                    const LLVMValueRef one_const = LLVMConstInt(LLVMInt1TypeInContext(cg->ctx), 1, true);
                    llvm_val = LLVMBuildXor(cg->builder, one_const, llvm_operand, "nottmp");
                }
            }
            break;
        default:
            goto error_cleanup;
    }

    return llvm_val;

error_cleanup:
    return NULL;
}

static LLVMValueRef cz_code_generator_generate_expr_cast(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    LLVMValueRef llvm_val = NULL;

    LLVMValueRef llvm_val_to_cast = cz_code_generator_generate_expr(cg, env, env_b, node->cast_expression.expression, is_compile_time);
    if (llvm_val_to_cast == NULL) {
        goto error_cleanup;
    }

    LLVMTypeRef llvm_src_type = cz_environment_backend_lookup_type(cg, node->cast_expression.expression->decoration->resolved_type);
    LLVMTypeRef llvm_dest_type = cz_environment_backend_lookup_type(cg, node->decoration->resolved_type);

    if (llvm_src_type == NULL || llvm_dest_type == NULL) {
        goto error_cleanup;
    }

    if (llvm_src_type == llvm_dest_type) {
        llvm_val = llvm_val_to_cast;
    } else {
        LLVMTypeKind llvm_src_type_kind = LLVMGetTypeKind(llvm_src_type);
        LLVMTypeKind llvm_dest_type_kind = LLVMGetTypeKind(llvm_dest_type);

        if (llvm_src_type_kind == LLVMIntegerTypeKind && llvm_dest_type_kind == LLVMFloatTypeKind) {
            llvm_val = LLVMBuildSIToFP(cg->builder, llvm_val_to_cast, llvm_dest_type, "sitofp_tmp");
        } else if (llvm_src_type_kind == LLVMFloatTypeKind && llvm_dest_type_kind == LLVMIntegerTypeKind) {
            llvm_val = LLVMBuildFPToSI(cg->builder, llvm_val_to_cast, llvm_dest_type, "fptosi_tmp");
        } else {
            goto error_cleanup;
        }
    }

    return llvm_val;

error_cleanup:
    return NULL;
}

static LLVMValueRef cz_code_generator_generate_expr_literal(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    LLVMValueRef llvm_val = NULL;

    switch (node->literal.literal_type) {
        case CZ_TT_NUMERICAL_LITERAL:
            {
                if (strchr(node->literal.lexeme, '.') != NULL) {
                    // Float
                    float value = strtof(node->literal.lexeme, NULL);
                    llvm_val = LLVMConstReal(LLVMFloatTypeInContext(cg->ctx), value);
                } else {
                    // Integer
                    int32_t value = strtol(node->literal.lexeme, NULL, 10);
                    llvm_val = LLVMConstInt(LLVMInt32TypeInContext(cg->ctx), value, true);
                }
            }
            break;
        case CZ_TT_TRUE:
            llvm_val = LLVMConstInt(LLVMInt1TypeInContext(cg->ctx), 1, false);
            break;
        case CZ_TT_FALSE:
            llvm_val = LLVMConstInt(LLVMInt1TypeInContext(cg->ctx), 0, false);
            break;
        default:
            goto error_cleanup;
    }

    return llvm_val;

error_cleanup:
    return NULL;
}

static LLVMValueRef cz_code_generator_generate_expr_identifier(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    LLVMValueRef llvm_val = NULL;

    const CZ_Symbol* sym = cz_environment_lookup(env, node->identifier.name, true);
    NULL_POINTER_ERROR_HANDLE(sym);
    if (sym->kind == CZ_TYPE_KIND_FUNCTION) {
        cz_error_list_push_error(cg->error_list, cg->filename, node->line, node->col, "Function as identifier (r-value) generation not yet supported.");
        goto error_cleanup;
    } else {
        LLVMValueRef llvm_var_loc = cz_environment_backend_lookup_val(env_b, sym, true);
        NULL_POINTER_ERROR_HANDLE(llvm_var_loc);
        LLVMTypeRef llvm_type = cz_environment_backend_lookup_type(cg, sym->data.value.type);
        NULL_POINTER_ERROR_HANDLE(llvm_type);
        llvm_val = LLVMBuildLoad2(cg->builder, llvm_type, llvm_var_loc, "load_tmp");
    }

    return llvm_val;
error_cleanup:
    return NULL;
}

static LLVMValueRef cz_code_generator_generate_expr_struct_init(CZ_CodeGenerator* cg, const CZ_Environment* env, const CZ_Environment_Backend* env_b, const CZ_AST_Node* node, bool is_compile_time) {
    LLVMValueRef* llvm_field_vals = NULL;
    LLVMValueRef llvm_val = NULL;

    // 1. Fetch the LLVM struct type (e.g. %Vector or %Matrix)
    LLVMTypeRef struct_type = cz_environment_backend_lookup_type(
        cg, 
        node->decoration->resolved_type
    );
    NULL_POINTER_ERROR_HANDLE(struct_type);

    unsigned int field_count = node->decoration->resolved_type->structure.layout->field_count;
    llvm_field_vals = (LLVMValueRef*) calloc(field_count, sizeof(LLVMValueRef));

    // For each initializer, evaluate the expression. (Unfilled members will be generated by the default expressions later.)
    for (unsigned int i = 0; i < node->struct_declaration.member_count; i++) {
        const CZ_AST_Node* initializer_member_node = node->struct_declaration.members[i];
        const char* initializer_member_name = initializer_member_node->struct_init_member.identifier->identifier.name;
        
        int field_idx = -1;
        // Seek the field that it corresponds to.
        for (unsigned int j = 0; j < field_count; j++) {
            if (node->decoration->resolved_type->structure.layout->fields[j].name == initializer_member_name) {
                field_idx = (int) j;
                break;
            }
        }
        if (field_idx < 0) {
            cz_error_list_push_error(cg->error_list, cg->filename, node->line, node->col, "Could not find %s in one of the members of the struct.", initializer_member_name);
            goto error_cleanup;
        }
        LLVMValueRef llvm_initializer_val = cz_code_generator_generate_expr(cg, env, env_b, initializer_member_node->struct_init_member.expression, is_compile_time);
        NULL_POINTER_ERROR_HANDLE(llvm_initializer_val);
        llvm_field_vals[field_idx] = llvm_initializer_val;
    }

    // Fill the rest of the field initializers.
    for (unsigned int i = 0; i < field_count; i++) {
        if (llvm_field_vals[i] != NULL) {
            // Already filled.
            continue;
        }

        if (node->decoration->resolved_type->structure.layout->fields[i].default_initializer == NULL) {
            // If default expression is not given, it is zero.
            const CZ_Type* field_type = node->decoration->resolved_type->structure.layout->fields[i].type;
            LLVMTypeRef llvm_field_type = cz_environment_backend_lookup_type(cg, field_type);
            llvm_field_vals[i] = LLVMConstNull(llvm_field_type);
        } else {
            // Otherwise, generate using default initializer.
            LLVMValueRef llvm_field_val = cz_code_generator_generate_expr(cg, env, env_b, node->decoration->resolved_type->structure.layout->fields[i].default_initializer, is_compile_time);
            llvm_field_vals[i] = llvm_field_val;
        }
    }

    //// 3. Construct the named constant struct
    if (is_compile_time) {
        llvm_val = LLVMConstNamedStruct(struct_type, llvm_field_vals, (unsigned int)field_count);
    } else {
        // Runtime struct building.
    }

    free(llvm_field_vals);
    return llvm_val;

error_cleanup:
    free(llvm_field_vals);
    return NULL;
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
