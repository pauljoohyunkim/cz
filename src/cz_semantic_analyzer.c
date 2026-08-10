#include <string.h>
#include "cz_parser.h"
#include "cz_type.h"
#include "cz_semantic_analyzer.h"

#define SCOPE_LEVEL_GLOBAL (0)
#define SCOPE_LEVEL_FUNCTION_PARAMETER (1)
#define SCOPE_LEVEL_FUNCTION_BODY (2)

#define NULL_POINTER_ERROR_HANDLE(ptr) do { if ((ptr) == NULL) goto error_cleanup; } while (0)
#define INVALID_NODE_TYPE_ERROR_HANDLE(node, node_type_enum) do { if ((node)->node_type != (node_type_enum)) goto error_cleanup; } while (0)
#define MAX(x,y) ((x) > (y) ? (x) : (y))

const CZ_Type* cz_type_from_type_node(const CZ_AST_Node* type_node, CZ_GlobalTypeTable* gtt) {
    const CZ_Type* base_type = NULL;
    const char* query_name = NULL;

    NULL_POINTER_ERROR_HANDLE(type_node);
    NULL_POINTER_ERROR_HANDLE(gtt);
    if (type_node->node_type != CZ_AST_TypeNodeType) goto error_cleanup;

    if (type_node->type_expression.is_function_type) {
        printf("Currently function type not supported.\n");
        goto error_cleanup;
    }

    if (type_node->type_expression.is_array) {
        // Inspect internal first.
        base_type = cz_type_from_type_node(type_node->type_expression.array.element_type, gtt);
        NULL_POINTER_ERROR_HANDLE(base_type);

    } else {
        // --- PHASE 1: Resolve the base, non-const canonical type ---
        switch (type_node->type_expression.primitive.kind) {
            case CZ_AST_TYPE_KIND_VOID:
            {
                CZ_Type query = {
                    .kind = CZ_TYPE_KIND_PRIMITIVE,
                    .primitive = CZ_PRIMITIVE_VOID
                };
                base_type = cz_global_type_table_find_type(gtt, &query);
                if (base_type == NULL) {
                    CZ_Type* new_type = cz_type_create(CZ_TYPE_KIND_PRIMITIVE);
                    new_type->primitive = CZ_PRIMITIVE_VOID;
                    NULL_POINTER_ERROR_HANDLE(new_type);
                    if (cz_global_type_table_push_type(gtt, NULL, new_type) != 1) {
                        cz_type_free(new_type);
                        goto error_cleanup;
                    }
                    base_type = new_type;
                }
                return base_type;
            }

            case CZ_AST_TYPE_KIND_INT32:
            {
                CZ_Type query = {
                    .kind = CZ_TYPE_KIND_PRIMITIVE,
                    .primitive = CZ_PRIMITIVE_INT32
                };
                base_type = cz_global_type_table_find_type(gtt, &query);
                if (base_type == NULL) {
                    CZ_Type* new_type = cz_type_create(CZ_TYPE_KIND_PRIMITIVE);
                    new_type->primitive = CZ_PRIMITIVE_INT32;
                    NULL_POINTER_ERROR_HANDLE(new_type);
                    if (cz_global_type_table_push_type(gtt, NULL, new_type) != 1) {
                        cz_type_free(new_type);
                        goto error_cleanup;
                    }
                    base_type = new_type;
                }
                break;
            }
            case CZ_AST_TYPE_KIND_UINT32:
            {
                CZ_Type query = {
                    .kind = CZ_TYPE_KIND_PRIMITIVE,
                    .primitive = CZ_PRIMITIVE_UINT32
                };
                base_type = cz_global_type_table_find_type(gtt, &query);
                if (base_type == NULL) {
                    CZ_Type* new_type = cz_type_create(CZ_TYPE_KIND_PRIMITIVE);
                    new_type->primitive = CZ_PRIMITIVE_UINT32;
                    NULL_POINTER_ERROR_HANDLE(new_type);
                    if (cz_global_type_table_push_type(gtt, NULL, new_type) != 1) {
                        cz_type_free(new_type);
                        goto error_cleanup;
                    }
                    base_type = new_type;
                }
                break;
            }
            case CZ_AST_TYPE_KIND_BOOL:
            {
                CZ_Type query = {
                    .kind = CZ_TYPE_KIND_PRIMITIVE,
                    .primitive = CZ_PRIMITIVE_BOOL
                };
                base_type = cz_global_type_table_find_type(gtt, &query);
                if (base_type == NULL) {
                    CZ_Type* new_type = cz_type_create(CZ_TYPE_KIND_PRIMITIVE);
                    new_type->primitive = CZ_PRIMITIVE_BOOL;
                    NULL_POINTER_ERROR_HANDLE(new_type);
                    if (cz_global_type_table_push_type(gtt, NULL, new_type) != 1) {
                        cz_type_free(new_type);
                        goto error_cleanup;
                    }
                    base_type = new_type;
                }
                break;
            }
            case CZ_AST_TYPE_KIND_FLOAT:
            {
                CZ_Type query = {
                    .kind = CZ_TYPE_KIND_PRIMITIVE,
                    .primitive = CZ_PRIMITIVE_FLOAT
                };
                base_type = cz_global_type_table_find_type(gtt, &query);
                if (base_type == NULL) {
                    CZ_Type* new_type = cz_type_create(CZ_TYPE_KIND_PRIMITIVE);
                    new_type->primitive = CZ_PRIMITIVE_FLOAT;
                    NULL_POINTER_ERROR_HANDLE(new_type);
                    if (cz_global_type_table_push_type(gtt, NULL, new_type) != 1) {
                        cz_type_free(new_type);
                        goto error_cleanup;
                    }
                    base_type = new_type;
                }
                break;
            }
            case CZ_AST_TYPE_KIND_IDENTIFIER:
            {
                query_name = type_node->type_expression.primitive.name;
                NULL_POINTER_ERROR_HANDLE(query_name);

                // Lookup the named type (which is inherently non-const in the registry)
                base_type = cz_global_type_table_find_type_by_name(gtt, query_name);
                NULL_POINTER_ERROR_HANDLE(base_type);
                break;
            }
            default:
                goto error_cleanup;
        }
    }

    // Keep a tracking pointer for our working type state
    const CZ_Type* current_type = base_type;

    // --- PHASE 2: Apply the const wrapper if requested by the AST ---
    if (type_node->type_expression.is_const) {
        CZ_Type const_query = {
            .kind = CZ_TYPE_KIND_CONST,
            .const_of = (CZ_Type*)current_type
        };

        const CZ_Type* existing_const = cz_global_type_table_find_type(gtt, &const_query);
        if (existing_const != NULL) {
            current_type = existing_const; // Update tracking pointer
        } else {
            CZ_Type* new_const_type = cz_type_create(CZ_TYPE_KIND_CONST);
            NULL_POINTER_ERROR_HANDLE(new_const_type);
            new_const_type->const_of = current_type;

            if (cz_global_type_table_push_type(gtt, NULL, new_const_type) != 1) {
                cz_type_free(new_const_type);
                goto error_cleanup;
            }
            current_type = new_const_type; // Update tracking pointer
        }
    }

    // --- PHASE 3: Apply the array/list wrapper if requested by the AST ---
    if (type_node->type_expression.is_array) {
        // Query if it exists or not.
        CZ_Type query = {
            .kind = CZ_TYPE_KIND_ARRAY,
            .array_info = {
                .element_type = current_type,
                .size = 10                      // TODO: FIX THIS SO THAT IT PARSES.
            }
        };
        const CZ_Type* array_type = cz_global_type_table_find_type(gtt, &query);
        if (array_type == NULL) {
            CZ_Type* new_array_type = cz_type_create(CZ_TYPE_KIND_ARRAY);
            new_array_type->array_info.element_type = query.array_info.element_type;
            new_array_type->array_info.size = query.array_info.size;
            if (cz_global_type_table_push_type(gtt, NULL, new_array_type) != 1) {
                cz_type_free(new_array_type);
                goto error_cleanup;
            }
            array_type = new_array_type;
        }
        current_type = array_type;
    }

    // --- PHASE 4: Apply the reference wrapper if requested by the AST ---
    if (type_node->type_expression.is_reference) {
        CZ_Type ref_query = {
            .kind = CZ_TYPE_KIND_REFERENCE,
            .reference_to = (CZ_Type*)current_type
        };

        const CZ_Type* existing_ref = cz_global_type_table_find_type(gtt, &ref_query);
        if (existing_ref != NULL) {
            current_type = existing_ref;
        } else {
            CZ_Type* new_ref_type = cz_type_create(CZ_TYPE_KIND_REFERENCE);
            NULL_POINTER_ERROR_HANDLE(new_ref_type);
            new_ref_type->reference_to = (CZ_Type*)current_type;

            if (cz_global_type_table_push_type(gtt, NULL, new_ref_type) != 1) {
                cz_type_free(new_ref_type);
                goto error_cleanup;
            }
            current_type = new_ref_type;
        }
    }

    return current_type;

error_cleanup:
    return NULL;
}

static const CZ_Type* cz_type_table_get_or_create_const(CZ_GlobalTypeTable* gtt, const CZ_Type* base_type) {
    CZ_Type* const_type = NULL;
    if (gtt == NULL || base_type == NULL) return NULL;

    // 1. Already const
    if (cz_type_is_const(base_type)) {
        return base_type;
    }

    // 2. See if we already have a const version
    for (unsigned int i = 0; i < gtt->all_entry_count; i++) {
        const CZ_Type* type = gtt->all_allocations[i];
        if (type->kind == CZ_TYPE_KIND_CONST && type->const_of == base_type) {
            return type;
        }
    }

    // 3. Allocate a new const.
    const_type = cz_type_create(CZ_TYPE_KIND_CONST);
    NULL_POINTER_ERROR_HANDLE(const_type);

    const_type->const_of = base_type;

    // 4. Register with GTT
    if (cz_global_type_table_push_type(gtt, NULL, const_type) != 1) {
        goto error_cleanup;
    }

    return const_type;

error_cleanup:
    cz_type_free(const_type);
    return NULL;
}

static bool cz_ast_node_returns_on_all_paths(const CZ_AST_Node* node) {
    if (node == NULL) return false;

    switch (node->node_type) {
        case CZ_AST_ReturnStatementNodeType:
            return true;
        
        case CZ_AST_BlockStatementNodeType:
            // If any statement in sequence is guaranteed a return, the block statement is guaranteed to return.
            for (unsigned int i = 0; i < node->statement_list.statement_count; i++) {
                if (cz_ast_node_returns_on_all_paths(node->statement_list.statements[i])) {
                    return true;
                }
            }
            return false;
        
        case CZ_AST_IfStatementNodeType:
            // If there is no "else" branch, it is not guaranteed at all.
            if (node->if_statement.else_branch == NULL) {
                return false;
            }

            // Both if-else and then must be guaranteed.
            {
                bool then_returns = cz_ast_node_returns_on_all_paths(node->if_statement.if_branch);
                bool else_returns = cz_ast_node_returns_on_all_paths(node->if_statement.else_branch);
                return then_returns && else_returns;
            }
            break;
        
        case CZ_AST_WhileStatementNodeType:
        case CZ_AST_ForStatementNodeType:
            return false;
        default:
            return false;
    }

    return false;
}

CZ_SemanticAnalyzer* cz_semantic_analyzer_create(CZ_Parser* parser) {
    CZ_SemanticAnalyzer* sa = NULL;
    CZ_Environment* global_env = NULL;
    CZ_GlobalTypeTable* gtt = NULL;
    CZ_ErrorList* error_list = NULL;
    if (parser == NULL) return NULL;

    sa = (CZ_SemanticAnalyzer*) calloc(1, sizeof(CZ_SemanticAnalyzer));
    NULL_POINTER_ERROR_HANDLE(sa);

    global_env = cz_environment_create();
    NULL_POINTER_ERROR_HANDLE(global_env);

    gtt = cz_global_type_table_create();
    NULL_POINTER_ERROR_HANDLE(gtt);

    error_list = cz_error_list_create();
    NULL_POINTER_ERROR_HANDLE(error_list);

    // Transfer global_env inside the semantic analyzer.
    sa->global_env = global_env;
    global_env = NULL;

    sa->gtt = gtt;
    gtt = NULL;

    // Transfer error list created.
    sa->error_list = error_list;
    error_list = NULL;

    // Copy & Transfer ownership from parser.
    sa->filename = parser->filename;

    sa->code = parser->code;
    parser->code = NULL;
    sa->code_length = parser->code_length;

    sa->tokens = parser->tokens;
    parser->tokens = NULL;
    sa->n_tokens = parser->n_tokens;

    sa->program = parser->program;
    parser->program = NULL;

    sa->sp = parser->sp;
    parser->sp = NULL;

    return sa;
error_cleanup:
    cz_environment_free(global_env);
    cz_global_type_table_free(gtt);
    cz_error_list_free(error_list);
    cz_semantic_analyzer_free(sa);
    return NULL;
}

void cz_semantic_analyzer_free(CZ_SemanticAnalyzer* sa) {
    if (sa != NULL) {
        free((char*)sa->code);
        free(sa->tokens);
        cz_ast_root_free(sa->program);
        cz_string_pool_free(sa->sp);
        cz_environment_free(sa->global_env);
        cz_global_type_table_free(sa->gtt);
        cz_error_list_free(sa->error_list);
    }
    free(sa);
}

static int cz_semantic_analyzer_build_global_table(CZ_SemanticAnalyzer* sa);
static int cz_semantic_analyzer_full_analyze(CZ_SemanticAnalyzer* sa);

// Will be invoking pass 1 and pass 2.
int cz_semantic_analyzer_analyze(CZ_SemanticAnalyzer* sa) {
    NULL_POINTER_ERROR_HANDLE(sa);

    cz_semantic_analyzer_build_global_table(sa);
    cz_semantic_analyzer_full_analyze(sa);

    return 1;
error_cleanup:
    return 0;
}

/* --- PASS 1 ---*/
static int cz_semantic_analyzer_register_function_decl(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl);
static int cz_semantic_analyzer_register_struct_decl(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl);
static int cz_semantic_analyzer_register_variable_decl(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl);
static int cz_semantic_analyzer_register_typedef(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl);
static int cz_semantic_analyzer_register_newtypedef(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl);

/**
 * @brief Pass 1 Function: Scans through global statements.
 * 
 * @param sa Pointer to CZ_SemanticAnalyzer
 * @return int 1 on success, 0 on failure
 * 
 * Type checking is not done at this stage.
 */
static int cz_semantic_analyzer_build_global_table(CZ_SemanticAnalyzer* sa) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(sa->program);
    INVALID_NODE_TYPE_ERROR_HANDLE(sa->program, CZ_AST_ProgramNodeType);
    NULL_POINTER_ERROR_HANDLE(sa->program->program.global_declaration_list);

    for (unsigned int i = 0; i < sa->program->program.declaration_count; i++) {
        const CZ_AST_Node* statement = sa->program->program.global_declaration_list[i];
        if (statement == NULL) return 0;

        // TODO: Log errors.
        switch (statement->node_type) {
            case CZ_AST_FunctionDeclarationNodeType:
                cz_semantic_analyzer_register_function_decl(sa, sa->global_env, statement);
                break;
            case CZ_AST_StructDeclarationNodeType:
                cz_semantic_analyzer_register_struct_decl(sa, sa->global_env, statement);
                break;
            case CZ_AST_VariableDeclarationNodeType:
                cz_semantic_analyzer_register_variable_decl(sa, sa->global_env, statement);
                break;
            case CZ_AST_TypedefDeclarationNodeType:
                cz_semantic_analyzer_register_typedef(sa, sa->global_env, statement);
                break;
            case CZ_AST_NewtypeDeclarationNodeType:
                cz_semantic_analyzer_register_newtypedef(sa, sa->global_env, statement);
                break;
            default:
                cz_error_list_push_error(sa->error_list, sa->filename, statement->line, statement->col, "Unrecognized global statement.");
                break;
        }
    }

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_register_function_decl(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl) {
    CZ_Type func_type_query;
    const CZ_Type** func_param_types = NULL;
    CZ_Symbol* func_symbol = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(decl);
    INVALID_NODE_TYPE_ERROR_HANDLE(decl, CZ_AST_FunctionDeclarationNodeType);

    // func addone :: (x :: int32, y :: int32) -> int

    // 1. Check if symbol exists. (If yes, bad: duplicate symbol)
    const CZ_Symbol* func_symbol_lookup = cz_environment_lookup(env, decl->function_declaration.function_identifier->identifier.name, true);
    if (func_symbol_lookup != NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                                 "Symbol \"%s\" previously defined.", func_symbol_lookup->name);
        goto error_cleanup;
    }

    // 2. Build type for function.
    // 2.1 Build return type
    const CZ_Type* func_ret_type = cz_type_from_type_node(decl->function_declaration.function.return_type, sa->gtt);
    if (func_ret_type == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                                 "Return type unrecognized.");
        goto error_cleanup;
    }
    // 2.2 Build param types

    // 2.2.1 Create parameter types list.
    const CZ_AST_Node* func_param_list = decl->function_declaration.function.parameter_list;
    unsigned int func_param_count = func_param_list->parameter_list.param_count;
    func_param_types = (const CZ_Type**) calloc(func_param_count, sizeof(CZ_Type*));
    NULL_POINTER_ERROR_HANDLE(func_param_types);

    // 2.2.2 Populate the parameter types list.
    for (unsigned int i = 0; i < func_param_count; i++) {
        const CZ_Type* func_param_type = cz_type_from_type_node(func_param_list->parameter_list.params[i]->variable_declaration.type, sa->gtt);
        NULL_POINTER_ERROR_HANDLE(func_param_type);
        func_param_types[i] = func_param_type;
    }

    // 2.2.3 Build type for function.
    func_type_query = (CZ_Type) {
        .kind = CZ_TYPE_KIND_FUNCTION,
        .function = {
            .return_type = func_ret_type,
            .param_types = func_param_types,
            .param_count = func_param_count,
        }
    };
    func_param_types = NULL;

    // 3. Check if GTT contains it. If not add it. Otherwise get the function type from GTT, and free the query type.
    const CZ_Type* func_type_lookup = cz_global_type_table_find_type(sa->gtt, &func_type_query);
    CZ_Type* func_type = NULL;
    if (func_type_lookup == NULL) {
        // 3.1 Add to GTT. Need to create type to add.
        func_type = cz_type_create(CZ_TYPE_KIND_FUNCTION);
        *func_type = func_type_query;
        if (cz_global_type_table_push_type(sa->gtt, NULL, func_type) != 1) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                                    "Function type for \"%s\" cannot be added to global type table", decl->function_declaration.function_identifier->identifier.name);
            cz_type_free(func_type);
            goto error_cleanup;
        }
    } else {
        // 3.2 GTT already contains the function signature. Use the lookup and free the param_types
        free(func_type_query.function.param_types);
        func_type = func_type_lookup;
    }
    
    // 4. Add symbol.
    func_symbol = cz_symbol_create(CZ_SYMBOL_KIND_VALUE, decl->function_declaration.function_identifier->identifier.name, 0);
    func_symbol->data.value.type = func_type;
    func_symbol->data.value.is_escapable_ref = true;
    if (cz_environment_push_symbol(env, func_symbol) != 1) {
        goto error_cleanup;
    }

    return 1;

error_cleanup:
    free(func_param_types);
    cz_symbol_free(func_symbol);
    return 0;
}

static int cz_semantic_analyzer_register_struct_decl(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl) {
    CZ_Type* struct_type = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(decl);
    INVALID_NODE_TYPE_ERROR_HANDLE(decl, CZ_AST_StructDeclarationNodeType);

    //struct Vector {
    //    x :: int32 = 1,
    //    y :: int32 = 0,
    //    z :: int32
    //};

    const char* struct_name = decl->struct_declaration.identifier->identifier.name;
    NULL_POINTER_ERROR_HANDLE(struct_name);

    // 1. Check if struct name exists in the global type table. If so, bad.
    const CZ_Type* struct_type_lookup = cz_global_type_table_find_type_by_name(sa->gtt, struct_name);
    if (struct_type_lookup != NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                                 "Type \"%s\" already defined previously", struct_name);
        goto error_cleanup;
    }

    // 2. Create struct type with NULL layout (to be populated in pass 2)
    struct_type = cz_type_create(CZ_TYPE_KIND_STRUCT);
    NULL_POINTER_ERROR_HANDLE(struct_type);

    struct_type->structure.name = struct_name;
    struct_type->structure.layout = NULL;   // Layout will be created in pass 2

    // 3. Add to GTT
    if (cz_global_type_table_push_type(sa->gtt, struct_name, struct_type) != 1) {
        cz_type_free(struct_type);
        struct_type = NULL;
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                                "Error adding struct \"%s\" to the global type table", struct_name);
        goto error_cleanup;
    }

    return 1;

error_cleanup:
    cz_type_free(struct_type);
    return 0;
}

static int cz_semantic_analyzer_register_variable_decl(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl) {
    CZ_Symbol* variable_symbol = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(decl);
    INVALID_NODE_TYPE_ERROR_HANDLE(decl, CZ_AST_VariableDeclarationNodeType);

    // x :: variable_type [ = 3 ];
    const CZ_AST_Node* variable_node = decl->variable_declaration.identifier;
    const CZ_AST_Node* type_node = decl->variable_declaration.type;

    // 1. Check symbol table. If variable exists, duplicate.
    const CZ_Symbol* variable_symbol_lookup = cz_environment_lookup(env, variable_node->identifier.name, false);
    if (variable_symbol_lookup != NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, variable_node->line, variable_node->col,
                                 "Symbol \"%s\" already defined previously", variable_node->identifier.name);
        goto error_cleanup;
    }

    // 2. Create type from type node.
    const CZ_Type* variable_type = cz_type_from_type_node(type_node, sa->gtt);
    if (variable_type == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, type_node->line, type_node->col,
                                 "Type could not be deduced");
        goto error_cleanup;
    }

    // 2.5 (TBD) Do not allow reference for now.
    if (variable_type->kind == CZ_TYPE_KIND_REFERENCE) {
        cz_error_list_push_error(sa->error_list, sa->filename, type_node->line, type_node->col,
                                 "References not supported in global variables");
        goto error_cleanup;
    }

    // 3. Create symbol and add it to symbol table.
    variable_symbol = cz_symbol_create(CZ_SYMBOL_KIND_VALUE, variable_node->identifier.name, 0);
    if (variable_symbol == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, type_node->line, type_node->col,
                                 "Symbol \"%s\" could not be created", variable_node->identifier.name);
        goto error_cleanup;
    }
    variable_symbol->data.value.type = variable_type;
    variable_symbol->data.value.is_escapable_ref = true;
    if (cz_environment_push_symbol(env, variable_symbol) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, type_node->line, type_node->col,
                                 "Symbol \"%s\" could not be added to symbol table", variable_node->identifier.name);
        goto error_cleanup;
    }
    

    return 1;

error_cleanup:
    cz_symbol_free(variable_symbol);
    return 0;
}

static int cz_semantic_analyzer_register_typedef(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(decl);
    INVALID_NODE_TYPE_ERROR_HANDLE(decl, CZ_AST_TypedefDeclarationNodeType);

    // typedef x y;
    const CZ_AST_Node* base_type_node = decl->typedef_declaration.type;
    const CZ_AST_Node* alias_type_node = decl->typedef_declaration.new_type;

    // 1. Check if x exists in the global type table. (If not, this is bad)
    const CZ_Type* base_type = cz_type_from_type_node(base_type_node, sa->gtt);
    if (base_type == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, base_type_node->line, base_type_node->col,
                                 "Base type could not be deduced.");
        goto error_cleanup;
    }

    // 2. Check if y exists in the symbol table. (If yes, this is bad since duplicate definition)
    const CZ_Type* alias_type_attempt = cz_global_type_table_find_type_by_name(sa->gtt, alias_type_node->identifier.name);
    if (alias_type_attempt != NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, base_type_node->line, base_type_node->col,
                                 "Type \"%s\" previously defined.", alias_type_node->identifier.name);
        goto error_cleanup;
    }

    // 3. Add y to symbol table, where in the global type table, it is added with name.
    if (cz_global_type_table_push_type(sa->gtt, alias_type_node->identifier.name, base_type) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, base_type_node->line, base_type_node->col,
                                 "Type \"%s\" could not be registered.", alias_type_node->identifier.name);
        goto error_cleanup;
    }

    return 1;

error_cleanup:
    return 0;
}

static bool cz_semantic_analyzer_newtypedef_detect_cycle(const CZ_Type* type, const char* name) {
    NULL_POINTER_ERROR_HANDLE(type);
    NULL_POINTER_ERROR_HANDLE(name);

    switch (type->kind) {
        case CZ_TYPE_KIND_NEWTYPE:
            // 1. Base Case: If this newtype's name matches the target name, we've found a cycle!
            // (Using pointer equality assuming your names are pooled in your String Pool)
            if (type->newtype.name == name || strcmp(type->newtype.name, name) == 0) {
                return true;
            }
            // 2. Recursive Case: Check the underlying type this newtype wraps
            return cz_semantic_analyzer_newtypedef_detect_cycle(type->newtype.underlying, name);

        case CZ_TYPE_KIND_CONST:
            // A const wrapper (e.g. const T) is a cycle if T is a cycle
            return cz_semantic_analyzer_newtypedef_detect_cycle(type->const_of, name);

        case CZ_TYPE_KIND_REFERENCE:
            // A reference type (e.g. ref T) is a cycle if T is a cycle
            return cz_semantic_analyzer_newtypedef_detect_cycle(type->reference_to, name);

        case CZ_TYPE_KIND_FUNCTION:
            // Check return type
            if (cz_semantic_analyzer_newtypedef_detect_cycle(type->function.return_type, name)) {
                return true;
            }
            // Check all parameter types
            for (unsigned int i = 0; i < type->function.param_count; i++) {
                if (cz_semantic_analyzer_newtypedef_detect_cycle(type->function.param_types[i], name)) {
                    return true;
                }
            }
            return false;

        case CZ_TYPE_KIND_PRIMITIVE:
        case CZ_TYPE_KIND_STRUCT:
            // Primitives and nominal structs are leaf types; they can't recursively contain 'name'
            return false;
    }

    return false;
error_cleanup:
    return false;
}

static int cz_semantic_analyzer_register_newtypedef(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl) {
    CZ_Type* new_type = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(decl);
    INVALID_NODE_TYPE_ERROR_HANDLE(decl, CZ_AST_NewtypeDeclarationNodeType);

    // newtype x y;
    const CZ_AST_Node* base_type_node = decl->typedef_declaration.type;
    const CZ_AST_Node* alias_type_node = decl->typedef_declaration.new_type;

    // 1. Check if x is in global type table. (If not, this is bad)
    const CZ_Type* base_type = cz_type_from_type_node(base_type_node, sa->gtt);
    if (base_type == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, base_type_node->line, base_type_node->col,
                                 "Base type could not be deduced.");
        goto error_cleanup;
    }

    // 2. Check if y is not in global type table. (If yes, this is bad: duplicate definition)
    const CZ_Type* new_type_lookup_attempt = cz_global_type_table_find_type_by_name(sa->gtt, alias_type_node->identifier.name);
    if (new_type_lookup_attempt != NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, base_type_node->line, base_type_node->col,
                                 "Type \"%s\" previously defined.", alias_type_node->identifier.name);
        goto error_cleanup;
    }

    // 3. Newtype cycle detection. (If cycle, bad)
    // While this is not possible, added for future extension.
    if (cz_semantic_analyzer_newtypedef_detect_cycle(base_type, alias_type_node->identifier.name)) {
        cz_error_list_push_error(sa->error_list, sa->filename, base_type_node->line, base_type_node->col,
                                 "Type \"%s\" cannot be a newtype due to it being cyclically defined.", alias_type_node->identifier.name);
        goto error_cleanup;
    }

    // 4. Construct and push.
    new_type = cz_type_create(CZ_TYPE_KIND_NEWTYPE);
    new_type->newtype.name = alias_type_node->identifier.name;
    new_type->newtype.underlying = base_type;
    if (cz_global_type_table_push_type(sa->gtt, alias_type_node->identifier.name, new_type) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, base_type_node->line, base_type_node->col,
                                 "Newtype \"%s\" could not be registered.", alias_type_node->identifier.name);
        goto error_cleanup;
    }

    return 1;

error_cleanup:
    cz_type_free(new_type);
    return 0;
}


/* --- PASS 2 ---*/
// Needs to check for constness, constexpr, and references.

static int cz_semantic_analyzer_check_function_body(CZ_SemanticAnalyzer* sa, CZ_AST_Node* decl);
static int cz_semantic_analyzer_check_struct_fields(CZ_SemanticAnalyzer* sa, CZ_AST_Node* decl);
static int cz_semantic_analyzer_check_global_var_init(CZ_SemanticAnalyzer* sa, CZ_AST_Node* decl);
static int cz_semantic_analyzer_check_statement_list(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* block);
static int cz_semantic_analyzer_check_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt);

/**
 * @brief Checks expression and decorates the AST node with CZ_AST_Decoration
 * 
 * @param sa Pointer to CZ_SemanticAnalyzer
 * @param env Pointer to CZ_Environment
 * @param expr Pointer to CZ_AST_Node
 * @return int 1 if success, 0 if failure.
 */
static int cz_semantic_analyzer_check_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);

/**
 * @brief Pass 2 Function: Type checking after global statements have been resolved.
 * 
 * @param sa Pointer to CZ_SemanticAnalyzer
 * @return int 1 on success, 0 on failure
 */
typedef enum {
    CZ_STRUCT_RECURSIVE_CYCLE_STATE_UNVISITED = 0,
    CZ_STRUCT_RECURSIVE_CYCLE_STATE_RESOLVING,
    CZ_STRUCT_RECURSIVE_CYCLE_STATE_RESOLVED
} CZ_StructRecursiveCycleState;

/**
 * @brief A helper to determine struct cycle.
 *
 * @param sa Pointer to CZ_SemanticAnalyzer
 * @param type Pointer to CZ_Type
 * @param states Allocated list of CZ_StructRecursiveCycleState. Length must be at least gtt->all_entry_count.
 * @return int 1 if no cycle (safe), or 0 if error or there is cycle.
 */
static int cz_semantic_analyzer_struct_cycle_detect(CZ_SemanticAnalyzer* sa, const CZ_Type* type, CZ_StructRecursiveCycleState* states) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(type);
    NULL_POINTER_ERROR_HANDLE(states);

    if (type->kind != CZ_TYPE_KIND_STRUCT) {
        // Not a struct. Primitive, references, etc. cannot form a struct cycle.
        return 1;
    }

    // Determine a unique ID (or index) for this struct.
    int struct_idx = -1;
    for (unsigned int i = 0; i < sa->gtt->all_entry_count; i++) {
        if (sa->gtt->all_allocations[i] == type) {
            struct_idx = (int) i;
            break;
        }
    }
    if (struct_idx < 0) {
        // Error: could not find struct in GTT
        return 0;
    }

    if (states[struct_idx] == CZ_STRUCT_RECURSIVE_CYCLE_STATE_RESOLVING) {
        // This was visited previously before.
        return 0;
    }

    if (states[struct_idx] == CZ_STRUCT_RECURSIVE_CYCLE_STATE_RESOLVED) {
        return 1;   // Verified safe before.
    }

    // Mark it as being visited.
    states[struct_idx] = CZ_STRUCT_RECURSIVE_CYCLE_STATE_RESOLVING;

    // Loop through each member
    for (unsigned int i = 0; i < type->structure.layout->field_count; i++) {
        const CZ_Type* field_type = type->structure.layout->fields[i].type;

        if (field_type->kind == CZ_TYPE_KIND_STRUCT) {
            if (cz_semantic_analyzer_struct_cycle_detect(sa, field_type, states) != 1) {
                // Recursive dependency.
                return 0;
            }
        }
    }

    states[struct_idx] = CZ_STRUCT_RECURSIVE_CYCLE_STATE_RESOLVED;

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_full_analyze(CZ_SemanticAnalyzer* sa) {
    CZ_StructRecursiveCycleState* states = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(sa->program);
    INVALID_NODE_TYPE_ERROR_HANDLE(sa->program, CZ_AST_ProgramNodeType);
    NULL_POINTER_ERROR_HANDLE(sa->program->program.global_declaration_list);

    for (unsigned int i = 0; i < sa->program->program.declaration_count; i++) {
        CZ_AST_Node* statement = sa->program->program.global_declaration_list[i];
        if (statement == NULL) return 0;

        // TODO: Log errors.
        switch (statement->node_type) {
            case CZ_AST_FunctionDeclarationNodeType:
                cz_semantic_analyzer_check_function_body(sa, statement);
                break;
            case CZ_AST_StructDeclarationNodeType:
                cz_semantic_analyzer_check_struct_fields(sa, statement);
                break;
            case CZ_AST_VariableDeclarationNodeType:
                cz_semantic_analyzer_check_global_var_init(sa, statement);
                break;
            case CZ_AST_TypedefDeclarationNodeType:
            case CZ_AST_NewtypeDeclarationNodeType:
                // Do nothing. Pass 1 dealt with everything.
                break;
            default:
                cz_error_list_push_error(sa->error_list, sa->filename, statement->line, statement->col, "Unrecognized global statement.");
                break;
        }
    }

    // Cycle check for structs (moved from pass 2 to pass 3)
    for (unsigned int i = 0; i < sa->program->program.declaration_count; i++) {
        const CZ_AST_Node* statement = sa->program->program.global_declaration_list[i];
        if (statement == NULL) return 0;

        if (statement->node_type == CZ_AST_StructDeclarationNodeType) {
            const char* struct_name = statement->struct_declaration.identifier->identifier.name;

            // Look up the struct type
            CZ_Type* struct_type = (CZ_Type*) cz_global_type_table_find_type_by_name(sa->gtt, struct_name);
            if (struct_type == NULL) {
                cz_error_list_push_error(sa->error_list, sa->filename, statement->line, statement->col, "Struct name \"%s\" is not recognized", struct_name);
                goto error_cleanup;
            }
            if (struct_type->kind != CZ_TYPE_KIND_STRUCT) {
                cz_error_list_push_error(sa->error_list, sa->filename, statement->line, statement->col, "\"%s\" is not a struct", struct_name);
                goto error_cleanup;
            }

            // Check if struct layout exists (should have been populated by cz_semantic_analyzer_check_struct_fields)
            if (struct_type->structure.layout == NULL) {
                cz_error_list_push_error(sa->error_list, sa->filename, statement->line, statement->col, "Struct layout not populated for \"%s\"", struct_name);
                goto error_cleanup;
            }

            // Allocate states array for cycle detection
            states = (CZ_StructRecursiveCycleState*) calloc(sa->gtt->all_entry_count, sizeof(CZ_StructRecursiveCycleState));
            if (states == NULL) {
                cz_error_list_push_error(sa->error_list, sa->filename, statement->line, statement->col, "Out of memory");
                goto error_cleanup;
            }

            // Perform cycle detection
            if (cz_semantic_analyzer_struct_cycle_detect(sa, struct_type, states) != 1) {
                cz_error_list_push_error(sa->error_list, sa->filename, statement->line, statement->col, "\"%s\" has circular dependency", struct_name);
                goto error_cleanup;
            }
            free(states);
            states = NULL;
        }
    }

    return 1;

error_cleanup:
    free(states);
    return 0;
}

static int cz_semantic_analyzer_check_function_body(CZ_SemanticAnalyzer* sa, CZ_AST_Node* decl) {
    CZ_Environment* func_param_env = NULL;
    CZ_Environment* func_body_env = NULL;
    CZ_Symbol* param_symbol = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(decl);
    INVALID_NODE_TYPE_ERROR_HANDLE(decl, CZ_AST_FunctionDeclarationNodeType);

    const char* func_name = decl->function_declaration.function_identifier->identifier.name;
    CZ_AST_Node* param_list_node = decl->function_declaration.function.parameter_list;

    // 1. Look up function from symbol table.
    const CZ_Symbol* func_symbol = cz_environment_lookup(sa->global_env, func_name, true);
    if (func_symbol == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Could not find function symbol \"%s\".", func_name);
        goto error_cleanup;
    }
    if (func_symbol->kind != CZ_SYMBOL_KIND_VALUE || func_symbol->data.value.type->kind != CZ_TYPE_KIND_FUNCTION) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Symbol \"%s\" might be declared as something that is not a function.", func_name);
        goto error_cleanup;
    }

    // Current function.
    sa->current_function_return = func_symbol->data.value.type->function.return_type;

    // 1.5 If not void function, all exit path must return something.
    if (!(sa->current_function_return->kind == CZ_TYPE_KIND_PRIMITIVE && sa->current_function_return->primitive == CZ_PRIMITIVE_VOID)) {
        if (!cz_ast_node_returns_on_all_paths(decl->function_declaration.body)) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Function \"%s\" returns non-void, but there may exist a path that it exits without returning something.", func_name);
            goto error_cleanup;
        }
    }

    // 2. Create parameter environment (scope = 1)
    func_param_env = cz_environment_create();
    if (func_param_env == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Could not create environment for function parameters of \"%s\".", func_name);
        goto error_cleanup;
    }
    func_param_env->parent = sa->global_env;
    func_param_env->scope_level = 1;
    // 2.1 Add symbol for each parameter
    // 2.1.1 Check that the number of parameters is equal.
    if (param_list_node->parameter_list.param_count != func_symbol->data.value.type->function.param_count) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
        "Parameter count in definition of function \"%s\" doesn't match its declaration.", func_name);
        goto error_cleanup;
    }

    for (unsigned int i = 0; i < param_list_node->parameter_list.param_count; i++) {
        const char* param_name = param_list_node->parameter_list.params[i]->variable_declaration.identifier->identifier.name;

        // 2.1.2 Check duplicate parameter names.
        if (cz_environment_lookup(func_param_env, param_name, false) != NULL) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, 
                "Redefinition of parameter \"%s\" in function \"%s\".", param_name, func_name);
            goto error_cleanup;
        }

        param_symbol = cz_symbol_create(CZ_SYMBOL_KIND_VALUE, param_name, 1);
        if (param_symbol == NULL) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Could not create symbol for function parameter \"%s\".", param_name);
            goto error_cleanup;
        }
        param_symbol->data.value.type = func_symbol->data.value.type->function.param_types[i];
        param_symbol->data.value.is_escapable_ref = param_symbol->data.value.type->kind == CZ_TYPE_KIND_REFERENCE;

        if (cz_environment_push_symbol(func_param_env, param_symbol) != 1) {
            cz_symbol_free(param_symbol);
            param_symbol = NULL;
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Could not push symbol for function parameter \"%s\".", param_name);
            goto error_cleanup;
        }
        param_symbol = NULL;
    }

    // 3. Create body environment.
    func_body_env = cz_environment_create();
    if (func_body_env == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Could not create environment for function body of \"%s\".", func_name);
        goto error_cleanup;
    }
    func_body_env->scope_level = 2;
    func_body_env->parent = func_param_env;

    // 4. Go through each statement.
    if (cz_semantic_analyzer_check_statement_list(sa, func_body_env, decl->function_declaration.body) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Function body of \"%s\" has a problem.", func_name);
        goto error_cleanup;
    }


    // Wire everything up.
    param_list_node->parameter_list.scope = func_param_env;
    func_param_env = NULL;
    decl->function_declaration.body->statement_list.scope = func_body_env;
    func_body_env = NULL;
    

    sa->current_function_return = NULL;
    return 1;

error_cleanup:
    cz_environment_free(func_param_env);
    cz_environment_free(func_body_env);
    cz_symbol_free(param_symbol);
    sa->current_function_return = NULL;
    return 0;
}

static int cz_semantic_analyzer_check_struct_fields(CZ_SemanticAnalyzer* sa, CZ_AST_Node* decl) {
    CZ_StructLayout* struct_layout = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(decl);
    INVALID_NODE_TYPE_ERROR_HANDLE(decl, CZ_AST_StructDeclarationNodeType);

    const char* struct_name = decl->struct_declaration.identifier->identifier.name;

    // global_y :: int32 = 3;
    //struct Vector {
    //    x :: int32 = 1,
    //    y :: int32& = global_y,
    //    z :: int32
    //};

    // 1. Resolve Struct Symbol
    CZ_Type* struct_type = (CZ_Type*) cz_global_type_table_find_type_by_name(sa->gtt, struct_name);
    if (struct_type == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Struct name \"%s\" is not recognized", struct_name);
        goto error_cleanup;
    }
    if (struct_type->kind != CZ_TYPE_KIND_STRUCT) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "\"%s\" is not a struct", struct_name);
        goto error_cleanup;
    }

    // 2. Populate the struct layout with the actual members
    unsigned int member_count = decl->struct_declaration.member_count;
    struct_layout = cz_struct_layout_create(member_count);
    if (struct_layout == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Out of memory");
        goto error_cleanup;
    }

    for (unsigned int i = 0; i < member_count; i++) {
        const CZ_AST_Node* member_node = decl->struct_declaration.members[i];
        const char* member_name = member_node->variable_declaration.identifier->identifier.name;

        // 2.1 Check for duplicate member names in the new layout
        for (unsigned int j = 0; j < i; j++) {
            if (member_name == struct_layout->fields[j].name) {
                cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                                        "Duplicate member name \"%s\" detected", member_name);
                goto error_cleanup;
            }
        }

        // 2.2 Get the type of the member
        const CZ_Type* member_type = cz_type_from_type_node(member_node->variable_declaration.type, sa->gtt);
        if (member_type == NULL) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                                    "Type for struct field \"%s\" cannot be deduced", member_name);
            goto error_cleanup;
        }

        // 2.3 Set the field in the new layout
        struct_layout->fields[i] = (CZ_StructField) {
            .idx = i,
            .name = member_node->variable_declaration.identifier->identifier.name,
            .type = member_type
        };
    }
    struct_type->structure.layout = struct_layout;
    struct_layout = NULL;

    // 3. For each member type,
    for (unsigned int i = 0; i < member_count; i++) {
        const CZ_AST_Node* member_node = decl->struct_declaration.members[i];
        // 3.1 Check if types are well-defined.
        const CZ_StructField* field = &struct_type->structure.layout->fields[i];
        const CZ_Type* field_type = field->type;
        const CZ_Type* gtt_field_type = cz_global_type_table_find_type(sa->gtt, field_type);
        if (gtt_field_type == NULL) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Type of member idx %d is ill-defined.", i+1);
            goto error_cleanup;
        }

        // 3.2 Does it have initializer?
        if (member_node->variable_declaration.expression != NULL) {
            // Check RHS.
            if (cz_semantic_analyzer_check_expression(sa, sa->global_env, member_node->variable_declaration.expression) != 1) {
                cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Type of initializer for member idx %d cannot be deduced.", i+1);
                goto error_cleanup;
            }

            // 3.2.1 Check if it is constexpr
            const CZ_AST_Decoration* initializer_decor = member_node->variable_declaration.expression->decoration;
            if (!initializer_decor->is_constexpr) {
                cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Initializer for member idx %d is not constexpr.", i+1);
                goto error_cleanup;
            }

            // 3.2.2 Decay type match
            const CZ_Type* decay_member_type = cz_type_decay_type(field_type);
            const CZ_Type* decay_expr_type = cz_type_decay_type(initializer_decor->resolved_type);
            if (!cz_type_equals(decay_member_type, decay_expr_type)) {
                cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Type for member and initializer for member idx %d does not match.", i+1);
                goto error_cleanup;
            }

            // 3.2.3 If LHS is reference
            if (field_type->kind == CZ_TYPE_KIND_REFERENCE) {
                // 3.2.3.1 Expression must be l-value.
                if (initializer_decor->value_category != CZ_VALUE_CATEGORY_LVALUE) {
                    cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Initializer for member idx %d is not an l-value even though it is reference.", i+1);
                    goto error_cleanup;
                }

                const CZ_Type* referenced_field_type = field_type->reference_to;

                // 3.2.3.2 If expression is const, field should be const
                if (referenced_field_type->kind != CZ_TYPE_KIND_CONST && initializer_decor->resolved_type->kind == CZ_TYPE_KIND_CONST) {
                    cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                        "Cannot bind non-const reference to a const value.");
                    goto error_cleanup;
                }
            }

            // Set the initializer to a "read-only" default_initializer in field.
            struct_type->structure.layout->fields[i].default_initializer = member_node->variable_declaration.expression;
        }
    }


    return 1;

error_cleanup:
    cz_struct_layout_free(struct_layout);
    return 0;
}

static int cz_semantic_analyzer_check_global_var_init(CZ_SemanticAnalyzer* sa, CZ_AST_Node* decl) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(decl);
    INVALID_NODE_TYPE_ERROR_HANDLE(decl, CZ_AST_VariableDeclarationNodeType);

    // x :: int32 = 1 + 2;
    // y :: int32& = x;

    // 1. Look up symbol for LHS.
    const char* var_name = decl->variable_declaration.identifier->identifier.name;
    CZ_Symbol* var_symbol = (CZ_Symbol*) cz_environment_lookup(sa->global_env, var_name, false);
    NULL_POINTER_ERROR_HANDLE(var_symbol);
    const CZ_Type* var_decl_type = var_symbol->data.value.type;

    const CZ_Type* decayed_lhs_type = cz_type_decay_type(var_decl_type);
    // 2. Check if there is an initializer.
    // 2.1 If no initializer, check if variable is either const or reference. (If either is true, error)
    if (decl->variable_declaration.expression == NULL) {
        if (var_decl_type->kind == CZ_TYPE_KIND_CONST || var_decl_type->kind == CZ_TYPE_KIND_REFERENCE) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                "\"%s\" is declared without initializer, but is const or reference.", var_name);
            goto error_cleanup;
        }

        // Nothing more to check since RHS does not exist.
        // 5. If variable is struct but it contains reference or const, RHS is required.
        if (decayed_lhs_type->kind == CZ_TYPE_KIND_STRUCT && decl->variable_declaration.expression == NULL) {
            for (unsigned int i = 0; i < decayed_lhs_type->structure.layout->field_count; i++) {
                const CZ_StructField* field = &decayed_lhs_type->structure.layout->fields[i];
                
                if ((cz_type_is_const(field->type) || field->type->kind == CZ_TYPE_KIND_REFERENCE) &&
                    field->default_initializer == NULL) {
                    
                    cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                        "Variable \"%s\" of struct type \"%s\" requires an initializer because field \"%s\" is a reference or const with no default value.",
                        decl->variable_declaration.identifier->identifier.name,
                        decayed_lhs_type->structure.name,
                        field->name);
                    goto error_cleanup;
                }
            }
        }
        return 1;
    }
    
    // 3. Check expression RHS.
    if (cz_semantic_analyzer_check_expression(sa, sa->global_env, decl->variable_declaration.expression) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
            "Could not determine the initializer for %s.", var_name);
        goto error_cleanup;
    }

    const CZ_AST_Decoration* rhs_decoration = decl->variable_declaration.expression->decoration;
    NULL_POINTER_ERROR_HANDLE(rhs_decoration);
    // 3.1. Global initializers must be compile-time constants (constexpr)
    if (!rhs_decoration->is_constexpr) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
            "Global variable \"%s\" initializer must be a compile-time constant expression.", var_name);
        goto error_cleanup;
    }

    // 4. If variable is reference
    if (var_decl_type->kind == CZ_TYPE_KIND_REFERENCE) {

        // 4.1 RHS must be l-value.
        if (rhs_decoration->value_category != CZ_VALUE_CATEGORY_LVALUE) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                "Variable  \"%s\" is declared as reference, but RHS is not l-value.", var_name);
            goto error_cleanup;
        }

        // 4.2 If RHS is const, then LHS cannot be const.
        const CZ_Type* referenced_type = var_decl_type->reference_to;
    
        // 4.3 Check if the underlying referenced type is non-const, but the RHS is const
        if (referenced_type->kind != CZ_TYPE_KIND_CONST && rhs_decoration->resolved_type->kind == CZ_TYPE_KIND_CONST) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                "Cannot bind non-const reference \"%s\" to a const value.", var_name);
            goto error_cleanup;
        }

        // 4.4 Flag if RHS is escapable.
        //var_symbol->data.value.is_escapable_ref = rhs_decoration->is_escapable_ref;
        var_symbol->data.value.is_escapable_ref = true;
    }


    // 6. If explicit type, do types match after decaying.
    const CZ_Type* decayed_rhs_type = cz_type_decay_type(rhs_decoration->resolved_type);
    if (!cz_type_equals(decayed_lhs_type, decayed_rhs_type)) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
            "Variable  \"%s\" type does not match the type of RHS.", var_name);
        goto error_cleanup;
    }
    
    // 7. Update symbol.
    var_symbol->data.value.is_constexpr = rhs_decoration->is_constexpr && cz_type_is_const(var_symbol->data.value.type);

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_statement_list(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* block) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(block);
    INVALID_NODE_TYPE_ERROR_HANDLE(block, CZ_AST_BlockStatementNodeType);

    for (unsigned int i = 0; i < block->statement_list.statement_count; i++) {
        CZ_AST_Node* stmt = block->statement_list.statements[i];
        if (cz_semantic_analyzer_check_statement(sa, env, stmt) != 1) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col, "An error at statement.");
        }
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_variable_declaration_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt);
static int cz_semantic_analyzer_check_assignment_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt);
static int cz_semantic_analyzer_check_return_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt);
static int cz_semantic_analyzer_check_if_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt);
static int cz_semantic_analyzer_check_for_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt);
static int cz_semantic_analyzer_check_while_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt);

static int cz_semantic_analyzer_check_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt) {
    CZ_Environment* inner_env = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(stmt);

    switch (stmt->node_type) {
        case CZ_AST_VariableDeclarationNodeType:
            if (cz_semantic_analyzer_check_variable_declaration_statement(sa, env, stmt) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_AssignmentStatementNodeType:
            if (cz_semantic_analyzer_check_assignment_statement(sa, env, stmt) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_ReturnStatementNodeType:
            if (cz_semantic_analyzer_check_return_statement(sa, env, stmt) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_IfStatementNodeType:
            if (cz_semantic_analyzer_check_if_statement(sa, env, stmt) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_ForStatementNodeType:
            if (cz_semantic_analyzer_check_for_statement(sa, env, stmt) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_WhileStatementNodeType:
            if (cz_semantic_analyzer_check_while_statement(sa, env, stmt) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_BlockStatementNodeType:
            inner_env = cz_environment_create();
            if (inner_env == NULL) {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col, "Could not allocate environment for block statement.");
                goto error_cleanup;
            }
            inner_env->parent = env;
            inner_env->scope_level = env->scope_level + 1;

            if (cz_semantic_analyzer_check_statement_list(sa, inner_env, stmt) != 1) {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col, "Block statement has an error.");
                goto error_cleanup;
            }

            stmt->statement_list.scope = inner_env;
            inner_env = NULL;
            break;
        default:
            if (cz_semantic_analyzer_check_expression(sa, env, stmt) != 1) {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col, "Expression checking failure.");
                goto error_cleanup;
            }
            break;
    }

    return 1;

error_cleanup:
    cz_environment_free(inner_env);
    return 0;
}

static int cz_semantic_analyzer_check_variable_declaration_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt) {
    CZ_Symbol* symbol = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(stmt);
    INVALID_NODE_TYPE_ERROR_HANDLE(stmt, CZ_AST_VariableDeclarationNodeType);

    const char* var_name = stmt->variable_declaration.identifier->identifier.name;

    // 1. Look up variable name. If var name exists, and it happens to be parameter, disallow it.
    const CZ_Symbol* var_name_symbol = cz_environment_lookup(env, var_name, true);
    if (var_name_symbol != NULL) {
        if (var_name_symbol->scope_level == SCOPE_LEVEL_FUNCTION_PARAMETER) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col, "%s is a function parameter, and it cannot be shadowed by variable declaration", var_name);
            goto error_cleanup;
        }
        if (var_name_symbol->scope_level == env->scope_level) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col, "Redeclaration of %s in the same scope not allowed.", var_name);
            goto error_cleanup;
        }
    }

    // Getting the variable type.
    const CZ_Type* var_type = cz_type_from_type_node(stmt->variable_declaration.type, sa->gtt);
    if (var_type == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col, "Type for \"%s\" cannot be deduced.", var_name);
        goto error_cleanup;
    }

    // 2. If variable is reference or const, RHS must exist. If not, fail.
    if (var_type->kind == CZ_TYPE_KIND_REFERENCE || cz_type_is_const(var_type)) {
        if (stmt->variable_declaration.expression == NULL) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col, "\"%s\" is declared either reference or const, but is not given an initializer.", var_name);
            goto error_cleanup;
        }
    }

    // 3. Check RHS and decorate.
    if (stmt->variable_declaration.expression != NULL) {
        if (cz_semantic_analyzer_check_expression(sa, env, stmt->variable_declaration.expression) != 1) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col, "Type for initializer of \"%s\" cannot be deduced.", var_name);
            goto error_cleanup;
        }
    }

    // 4. If reference variable, RHS must be l-value.
    if (var_type->kind == CZ_TYPE_KIND_REFERENCE) {
        if (stmt->variable_declaration.expression == NULL || 
            stmt->variable_declaration.expression->decoration->value_category != CZ_VALUE_CATEGORY_LVALUE) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "\"%s\" declared as reference but initializer is not l-value.", var_name);
            goto error_cleanup;
        }
    }

    // 5. Const-correctness
    if (var_type->kind == CZ_TYPE_KIND_REFERENCE && !cz_type_is_const(var_type)) {
        if (stmt->variable_declaration.expression != NULL && 
            cz_type_is_const(stmt->variable_declaration.expression->decoration->resolved_type)) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "\"%s\" declared as mutable reference but initializer is const.", var_name);
            goto error_cleanup;
        }
    }

    const CZ_Type* decayed_var_type = cz_type_decay_type(var_type);

    // 6. If variable is struct but it contains reference or const, RHS is required.
    if (decayed_var_type->kind == CZ_TYPE_KIND_STRUCT && stmt->variable_declaration.expression == NULL) {
        for (unsigned int i = 0; i < decayed_var_type->structure.layout->field_count; i++) {
            const CZ_StructField* field = &decayed_var_type->structure.layout->fields[i];
            
            if ((cz_type_is_const(field->type) || field->type->kind == CZ_TYPE_KIND_REFERENCE) &&
                field->default_initializer == NULL) {
                
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "Variable \"%s\" of struct type \"%s\" requires an initializer because field \"%s\" is a reference or const with no default value.",
                    stmt->variable_declaration.identifier->identifier.name,
                    decayed_var_type->structure.name,
                    field->name);
                goto error_cleanup;
            }
        }
    }

    // 7. Decay type match
    if (stmt->variable_declaration.expression != NULL) {
        const CZ_Type* decayed_expr_type = cz_type_decay_type(stmt->variable_declaration.expression->decoration->resolved_type);
        if (!cz_type_equals(decayed_var_type, decayed_expr_type)) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Declared type for \"%s\" does not match the decayed type of initializer.", var_name);
            goto error_cleanup;
        }
    }

    symbol = cz_symbol_create(CZ_SYMBOL_KIND_VALUE, var_name, env->scope_level);
    if (symbol == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Symbol for \"%s\" could not be created.", var_name);
        goto error_cleanup;
    }
    symbol->data.value.type = var_type;
    symbol->data.value.is_constexpr = cz_type_is_const(var_type) &&
                                      stmt->variable_declaration.expression != NULL &&
                                      stmt->variable_declaration.expression->decoration->is_constexpr;
    symbol->data.value.is_escapable_ref = var_type->kind == CZ_TYPE_KIND_REFERENCE && stmt->variable_declaration.expression->decoration->is_escapable_ref;

    if (cz_environment_push_symbol(env, symbol) != 1) {
        cz_symbol_free(symbol);
        symbol = NULL;
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Symbol for \"%s\" could not be added to symbol table.", var_name);
        goto error_cleanup;
    }
    symbol = NULL;

    return 1;

error_cleanup:
    cz_symbol_free(symbol);
    return 0;
}

static int cz_semantic_analyzer_check_assignment_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(stmt);
    INVALID_NODE_TYPE_ERROR_HANDLE(stmt, CZ_AST_AssignmentStatementNodeType);

    // 0. Operation Type
    if (!cz_token_type_is_assignment(stmt->binary_expression.op)) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Assignment operation but assignment operator is not given.");
        goto error_cleanup;
    }

    // 1. Check LHS and RHS
    if (cz_semantic_analyzer_check_expression(sa, env, stmt->binary_expression.left) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Type of LHS cannot be deduced.");
        goto error_cleanup;
    }
    if (cz_semantic_analyzer_check_expression(sa, env, stmt->binary_expression.right) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Type of RHS cannot be deduced.");
        goto error_cleanup;
    }

    // Check that LHS and RHS decorations are not NULL
    if (stmt->binary_expression.left->decoration == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "LHS decoration is missing.");
        goto error_cleanup;
    }
    if (stmt->binary_expression.right->decoration == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "RHS decoration is missing.");
        goto error_cleanup;
    }
    const CZ_Type* lhs_type = stmt->binary_expression.left->decoration->resolved_type;
    const CZ_Type* rhs_type = stmt->binary_expression.right->decoration->resolved_type;
    NULL_POINTER_ERROR_HANDLE(lhs_type);
    NULL_POINTER_ERROR_HANDLE(rhs_type);

    // 2. LHS must be l-value.
    if (stmt->binary_expression.left->decoration->value_category != CZ_VALUE_CATEGORY_LVALUE) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Assignment requires LHS to be l-value.");
        goto error_cleanup;
    }

    // 3. LHS must be non-const
    if (cz_type_is_const(stmt->binary_expression.left->decoration->resolved_type)) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Assignment requires LHS to be mutable.");
        goto error_cleanup;
    }

    // 4. For assignment operators, check that the underlying operation is valid
    // Pre-fetch basic types (same as in binary expression checker)
    const CZ_Type* int32_type = cz_global_type_table_find_type_by_name(sa->gtt, "int32");
    const CZ_Type* uint32_type = cz_global_type_table_find_type_by_name(sa->gtt, "uint32");
    const CZ_Type* float_type = cz_global_type_table_find_type_by_name(sa->gtt, "float");
    const CZ_Type* bool_type = cz_global_type_table_find_type_by_name(sa->gtt, "bool");
    NULL_POINTER_ERROR_HANDLE(int32_type);
    NULL_POINTER_ERROR_HANDLE(uint32_type);
    NULL_POINTER_ERROR_HANDLE(float_type);
    NULL_POINTER_ERROR_HANDLE(bool_type);

    // Decay and strip const for operation validation (same as binary expression checker)
    const CZ_Type* decayed_lhs_type = cz_type_decay_type(lhs_type);
    const CZ_Type* decayed_rhs_type = cz_type_decay_type(rhs_type);
    NULL_POINTER_ERROR_HANDLE(decayed_lhs_type);
    NULL_POINTER_ERROR_HANDLE(decayed_rhs_type);

    // Check that after decaying, we have primitive types (same validation as binary expressions)
    if (decayed_lhs_type->kind != CZ_TYPE_KIND_PRIMITIVE || decayed_rhs_type->kind != CZ_TYPE_KIND_PRIMITIVE) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Invalid type for assignment operation. (After stripping reference and const, not a primitive)");
        goto error_cleanup;
    }

    // 5. Decay type must match for simple assignment
    if (!cz_type_equals(decayed_lhs_type, decayed_rhs_type)) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Assignment requires types of LHS and RHS to match.");
        goto error_cleanup;
    }

    // 6. Check validity of the underlying operation for compound assignment operators
    switch (stmt->binary_expression.op) {
        case CZ_TT_EQUAL:
            // Simple assignment - already validated type match above
            break;

        case CZ_TT_PLUS_EQUAL:
            // int32 + int32 -> int32
            // uint32 + uint32 -> uint32
            // float + float -> float
            if (decayed_lhs_type->primitive == CZ_PRIMITIVE_INT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_UINT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_FLOAT && decayed_rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                // Valid
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "+= only defined for int32, uint32, or float.");
                goto error_cleanup;
            }
            break;

        case CZ_TT_MINUS_EQUAL:
            // int32 - int32 -> int32
            // uint32 - uint32 -> uint32
            // float - float -> float
            if (decayed_lhs_type->primitive == CZ_PRIMITIVE_INT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_UINT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_FLOAT && decayed_rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                // Valid
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "-= only defined for int32, uint32, or float.");
                goto error_cleanup;
            }
            break;

        case CZ_TT_STAR_EQUAL:
            // int32 * int32 -> int32
            // uint32 * uint32 -> uint32
            // float * float -> float
            if (decayed_lhs_type->primitive == CZ_PRIMITIVE_INT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_UINT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_FLOAT && decayed_rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                // Valid
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "*= only defined for int32 or float.");
                goto error_cleanup;
            }
            break;

        case CZ_TT_SLASH_EQUAL:
            // int32 / int32 -> int32
            // uint32 / uint32 -> uint32
            // float / float -> float
            if (decayed_lhs_type->primitive == CZ_PRIMITIVE_INT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_UINT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_FLOAT && decayed_rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                // Valid
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "/= only defined for int32 or float.");
                goto error_cleanup;
            }
            break;

        case CZ_TT_PERCENT_EQUAL:
            // int32 % int32 -> int32
            // uint32 % uint32 -> uint32
            if (decayed_lhs_type->primitive == CZ_PRIMITIVE_INT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_UINT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                // Valid
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "%= only defined for int32");
                goto error_cleanup;
            }
            break;

        case CZ_TT_AMPERSAND_EQUAL:
            // int32 & int32 -> int32
            // uint32 & uint32 -> uint32
            if (decayed_lhs_type->primitive == CZ_PRIMITIVE_INT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_UINT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                // Valid
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "&= only defined for int32");
                goto error_cleanup;
            }
            break;

        case CZ_TT_BAR_EQUAL:
            // int32 | int32 -> int32
            // uint32 | uint32 -> uint32
            if (decayed_lhs_type->primitive == CZ_PRIMITIVE_INT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_UINT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                // Valid
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "|= only defined for int32");
                goto error_cleanup;
            }
            break;

        case CZ_TT_CARET_EQUAL:
            // int32 ^ int32 -> int32
            // uint32 ^ uint32 -> uint32
            if (decayed_lhs_type->primitive == CZ_PRIMITIVE_INT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                // Valid
            } else if (decayed_lhs_type->primitive == CZ_PRIMITIVE_UINT32 && decayed_rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                // Valid
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "^= only defined for int32");
                goto error_cleanup;
            }
            break;

        default:
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Unknown assignment operator.");
            goto error_cleanup;
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_return_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(stmt);
    INVALID_NODE_TYPE_ERROR_HANDLE(stmt, CZ_AST_ReturnStatementNodeType);
    NULL_POINTER_ERROR_HANDLE(sa->current_function_return);

    // 1. If void return, then expr must be empty. If not void return, then expression must not be empty.
    if (sa->current_function_return->kind == CZ_TYPE_KIND_PRIMITIVE && sa->current_function_return->primitive == CZ_PRIMITIVE_VOID) {
        if (stmt->return_statement.expression != NULL) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Return type of function is void, but an expression is given");
            goto error_cleanup;
        }
        // Done
        return 1;
    } else {
        if (stmt->return_statement.expression == NULL) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Return type of function is not void, but return expression is not given.");
            goto error_cleanup;
        }
    }

    // Check expression
    if (cz_semantic_analyzer_check_expression(sa, env, stmt->return_statement.expression) != 1) {
        goto error_cleanup;
    }

    // 2. Decay type match
    const CZ_Type* decayed_return_type = cz_type_decay_type(sa->current_function_return);
    NULL_POINTER_ERROR_HANDLE(decayed_return_type);
    const CZ_Type* decayed_expr_type = cz_type_decay_type(stmt->return_statement.expression->decoration->resolved_type);
    NULL_POINTER_ERROR_HANDLE(decayed_expr_type);
    if (!cz_type_equals(decayed_return_type, decayed_expr_type)) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Return type of function does not match the expression type of return statement.");
        goto error_cleanup;
    }

    // 3. Reference handling
    if (sa->current_function_return->kind == CZ_TYPE_KIND_REFERENCE) {
        // 3.1 Returning expression must be L-value
        if (stmt->return_statement.expression->decoration->value_category != CZ_VALUE_CATEGORY_LVALUE) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Return type of reference returning function should be l-value.");
            goto error_cleanup;
        }


        /*
        // 3.1 & 3.2 Unified Scope and Lifetime validation via Decoration
        if (stmt->return_statement.expression->decoration->scope_level >= 2) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Cannot return local scope expressions by reference.");
            goto error_cleanup;
        }

        // If it's at level 1 (parameter space), it MUST stem from an actual reference
        if (stmt->return_statement.expression->decoration->scope_level == 1 && 
            !stmt->return_statement.expression->decoration->is_reference_source) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Cannot return a reference to data owned by a non-reference parameter.");
            goto error_cleanup;
        }
        */

        if (!stmt->return_statement.expression->decoration->is_escapable_ref) {
            cz_error_list_push_error(
                sa->error_list, 
                sa->filename, 
                stmt->line, 
                stmt->col,
                "Cannot return a reference to local stack memory."
            );
            goto error_cleanup;
        }

        // 3.4 Const dropping block: If non-const reference returning, expression must not be a const.
        if (sa->current_function_return->reference_to->kind != CZ_TYPE_KIND_CONST &&
            cz_type_is_const(stmt->return_statement.expression->decoration->resolved_type)
        ) {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "Cannot strip away constness when function return type is non-const.");
                goto error_cleanup;
        }
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_if_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(stmt);
    INVALID_NODE_TYPE_ERROR_HANDLE(stmt, CZ_AST_IfStatementNodeType);

    // Check condition. See if it is boolean.
    if (cz_semantic_analyzer_check_expression(sa, env, stmt->if_statement.condition) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Could not check the type inside the if condition");
        goto error_cleanup;
    }
    const CZ_Type* decayed_condition_type = cz_type_decay_type(stmt->if_statement.condition->decoration->resolved_type);
    NULL_POINTER_ERROR_HANDLE(decayed_condition_type);
    if (decayed_condition_type->kind != CZ_TYPE_KIND_PRIMITIVE || decayed_condition_type->primitive != CZ_PRIMITIVE_BOOL) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Condition for while is not a boolean.");
        goto error_cleanup;
    }
    
    // 2. Check then block.
    if (cz_semantic_analyzer_check_statement(sa, env, stmt->if_statement.if_branch) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "If statement then body has a problem");
        goto error_cleanup;
    }

    // 3. Check else block
    if (stmt->if_statement.else_branch != NULL) {
        if (cz_semantic_analyzer_check_statement(sa, env, stmt->if_statement.else_branch) != 1) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "If statement else body has a problem");
            goto error_cleanup;
        }
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_for_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt) {
    CZ_Environment* sub_env = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(stmt);
    INVALID_NODE_TYPE_ERROR_HANDLE(stmt, CZ_AST_ForStatementNodeType);

    // 1. Create a sub-environment as there could be a declaration.
    sub_env = cz_environment_create();
    if (sub_env == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Subenvironment for for-loop could not be created");
        goto error_cleanup;
    }
    sub_env->parent = env;
    sub_env->scope_level = env->scope_level + 1;

    // 2. Check initializer.
    if (stmt->for_statement.initialization != NULL) {
        CZ_AST_Node* initializer_node = stmt->for_statement.initialization;

        // 2.1 If initializer is declaration, add to the sub-environment just created.
        if (initializer_node->node_type == CZ_AST_VariableDeclarationNodeType || initializer_node->node_type == CZ_AST_AssignmentStatementNodeType) {
            if (cz_semantic_analyzer_check_statement(sa, sub_env, stmt->for_statement.initialization) != 1) {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "Initializer statement must be either a variable declaration or assignment (for now, maybe?).");
                goto error_cleanup;
            }
        }
    }
    // 3. Check condition if it exists and see if it resolves to boolean.
    if (stmt->for_statement.condition != NULL) {
        if (cz_semantic_analyzer_check_expression(sa, sub_env, stmt->for_statement.condition) != 1) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Could not check the type inside the for condition");
            goto error_cleanup;
        }
        const CZ_Type* decayed_condition_type = cz_type_decay_type(stmt->for_statement.condition->decoration->resolved_type);
        NULL_POINTER_ERROR_HANDLE(decayed_condition_type);
        if (decayed_condition_type->kind != CZ_TYPE_KIND_PRIMITIVE || decayed_condition_type->primitive != CZ_PRIMITIVE_BOOL) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Condition for for is not a boolean.");
            goto error_cleanup;
        }
    }
    // 4. Check assignment or expression.
    if (stmt->for_statement.iteration_step != NULL) {
        if (cz_semantic_analyzer_check_statement(sa, sub_env, stmt->for_statement.iteration_step) != 1) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Invalid condition for iteration step.");
            goto error_cleanup;
        }
    }
    // 5. Check block statement.
    if (cz_semantic_analyzer_check_statement(sa, sub_env, stmt->for_statement.body) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "For statement body has a problem.");
        goto error_cleanup;
    }

    stmt->for_statement.scope = sub_env;
    sub_env = NULL;

    return 1;

error_cleanup:
    cz_environment_free(sub_env);
    return 0;
}

static int cz_semantic_analyzer_check_while_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(stmt);
    INVALID_NODE_TYPE_ERROR_HANDLE(stmt, CZ_AST_WhileStatementNodeType);

    // 1. Check condition. See if it is boolean.
    if (cz_semantic_analyzer_check_expression(sa, env, stmt->while_statement.condition) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Could not check the type inside the while condition");
        goto error_cleanup;
    }
    const CZ_Type* decayed_condition_type = cz_type_decay_type(stmt->while_statement.condition->decoration->resolved_type);
    NULL_POINTER_ERROR_HANDLE(decayed_condition_type);
    if (decayed_condition_type->kind != CZ_TYPE_KIND_PRIMITIVE || decayed_condition_type->primitive != CZ_PRIMITIVE_BOOL) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Condition for while is not a boolean.");
        goto error_cleanup;
    }

    // 2. Check block.
    if (cz_semantic_analyzer_check_statement(sa, env, stmt->while_statement.body) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "While statement body has a problem");
        goto error_cleanup;
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_function_call_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_check_binary_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_check_unary_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_check_literal_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_check_identifier_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_check_struct_access(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_check_struct_init(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_cast_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);

static int cz_semantic_analyzer_check_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(expr);

    switch (expr->node_type) {
        case CZ_AST_FunctionCallNodeType:
            if (cz_semantic_analyzer_check_function_call_expression(sa, env, expr) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_BinaryExpressionNodeType:
            if (cz_semantic_analyzer_check_binary_expression(sa, env, expr) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_UnaryExpressionNodeType:
            if (cz_semantic_analyzer_check_unary_expression(sa, env, expr) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_LiteralNodeType:
            if (cz_semantic_analyzer_check_literal_expression(sa, env, expr) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_IdentifierNodeType:
            if (cz_semantic_analyzer_check_identifier_expression(sa, env, expr) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_StructMemberAccessNodeType:
            if (cz_semantic_analyzer_check_struct_access(sa, env, expr) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_StructInitNodeType:
            if (cz_semantic_analyzer_check_struct_init(sa, env, expr) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_CastExpressionNodeType:
            if (cz_semantic_analyzer_cast_expression(sa, env, expr) != 1) {
                goto error_cleanup;
            }
            break;
        default:
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Not yet supported.");
            goto error_cleanup;
            
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_function_call_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    CZ_AST_Decoration* decor = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(expr);
    INVALID_NODE_TYPE_ERROR_HANDLE(expr, CZ_AST_FunctionCallNodeType);

    // 1. Check callee and see if it is a function (supports higher-order expressions like (f(3))(2))
    if (cz_semantic_analyzer_check_expression(sa, env, expr->function_call.callee) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Function callee type could not be deduced.");
        goto error_cleanup;
    }
    
    // 2. Check if it is function.
    if (expr->function_call.callee->decoration->resolved_type->kind != CZ_TYPE_KIND_FUNCTION) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Callee is not a function.");
        goto error_cleanup;
    }
    
    // 3. Param count validation.
    if (expr->function_call.callee->decoration->resolved_type->function.param_count != expr->function_call.arg_count) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Function expects %d arguments but only received %d arguments.",
            expr->function_call.callee->decoration->resolved_type->function.param_count,
            expr->function_call.arg_count);
        goto error_cleanup;
    }
    
    unsigned int param_count = expr->function_call.callee->decoration->resolved_type->function.param_count;
    
    // 4. Argument type checking loop
    for (unsigned int i = 0; i < param_count; i++) {
        if (cz_semantic_analyzer_check_expression(sa, env, expr->function_call.arguments[i]) != 1) {
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                "Function argument type could not be deduced.");
            goto error_cleanup;
        }
        
        // 4.1 Decayed type check
        const CZ_Type* param_type = expr->function_call.callee->decoration->resolved_type->function.param_types[i];
        const CZ_Type* arg_type = expr->function_call.arguments[i]->decoration->resolved_type;
        const CZ_Type* decayed_param_type = cz_type_decay_type(param_type);
        const CZ_Type* decayed_arg_type = cz_type_decay_type(arg_type);
        if (!cz_type_equals(decayed_param_type, decayed_arg_type)) {
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                "Function argument does not match parameter type.");
            goto error_cleanup;
        }
        
        // 4.2 Reference Semantics Enforcement
        if (param_type->kind == CZ_TYPE_KIND_REFERENCE) {
            // 4.2.1 Argument must be an assignable l-value
            if (expr->function_call.arguments[i]->decoration->value_category != CZ_VALUE_CATEGORY_LVALUE) {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "Function parameter is reference, but argument passed is not an l-value.");
                goto error_cleanup;
            }

            // 4.2.2 Check mutability compatibility (Prevent casting away constness)
            if (cz_type_is_const(arg_type) && !cz_type_is_const(param_type)) {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "Function is defined with mutable parameter but you passed a const argument. Cannot cast away constness.");
                goto error_cleanup;
            }
        }
    }

    // 5. Determine if returning a reference context (LVALUE) or copy context (RVALUE)
    const CZ_Type* return_type = expr->function_call.callee->decoration->resolved_type->function.return_type;
    CZ_ValueCategory value_cat = (return_type->kind == CZ_TYPE_KIND_REFERENCE) ? CZ_VALUE_CATEGORY_LVALUE : CZ_VALUE_CATEGORY_RVALUE;

    decor = cz_ast_decoration_create(
        return_type,
        value_cat,
        false, // is_constexpr (Function results are computed at runtime)
        return_type->kind == CZ_TYPE_KIND_REFERENCE, 
        env->scope_level
    );
    decor->is_escapable_ref = return_type->kind == CZ_TYPE_KIND_REFERENCE;
    if (decor == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Allocating AST decorator failure.");
        goto error_cleanup;
    }

    expr->decoration = decor;
    decor = NULL;

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_binary_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    CZ_AST_Decoration* decor = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(expr);
    INVALID_NODE_TYPE_ERROR_HANDLE(expr, CZ_AST_BinaryExpressionNodeType);

    if (cz_semantic_analyzer_check_expression(sa, env, expr->binary_expression.left) != 1) {
        goto error_cleanup;
    }
    if (cz_semantic_analyzer_check_expression(sa, env, expr->binary_expression.right) != 1) {
        goto error_cleanup;
    }

    // Pre-fetch basic types
    const CZ_Type* int32_type = cz_global_type_table_find_type_by_name(sa->gtt, "int32");
    const CZ_Type* uint32_type = cz_global_type_table_find_type_by_name(sa->gtt, "uint32");
    const CZ_Type* float_type = cz_global_type_table_find_type_by_name(sa->gtt, "float");
    const CZ_Type* bool_type = cz_global_type_table_find_type_by_name(sa->gtt, "bool");
    NULL_POINTER_ERROR_HANDLE(int32_type);
    NULL_POINTER_ERROR_HANDLE(uint32_type);
    NULL_POINTER_ERROR_HANDLE(float_type);
    NULL_POINTER_ERROR_HANDLE(bool_type);

    const CZ_AST_Node* lhs_node = expr->binary_expression.left;
    const CZ_Type* lhs_type = lhs_node->decoration->resolved_type;
    const CZ_AST_Node* rhs_node = expr->binary_expression.right;
    const CZ_Type* rhs_type = rhs_node->decoration->resolved_type;

    // Strip away reference and const.
    lhs_type = cz_type_decay_type(lhs_type);
    rhs_type = cz_type_decay_type(rhs_type);

    if (lhs_type->kind != CZ_TYPE_KIND_PRIMITIVE || rhs_type->kind != CZ_TYPE_KIND_PRIMITIVE) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Invalid type for either the LHS or RHS of binary operation. (Even after stripping reference and const, not a primitive)");
        goto error_cleanup;
    }

    const CZ_Type* result_type = NULL;
    switch (expr->binary_expression.op) {
        case CZ_TT_PLUS:
            // int32 + int32 -> int32
            // uint32 + uint32 -> uint32
            // float + float -> float
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = uint32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = float_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "+ only defined for int32, uint32, or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_MINUS:
            // int32 - int32 -> int32
            // uint32 - uint32 -> uint32
            // float - float -> float
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = uint32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = float_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "- only defined for int32, uint32, or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_STAR:
            // int32 * int32 -> int32
            // uint32 * uint32 -> uint32
            // float * float -> float
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = uint32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = float_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "* only defined for int32, uint32, or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_SLASH:
            // int32 / int32 -> int32
            // uint32 / uint32 -> uint32
            // float / float -> float
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = uint32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = float_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "/ only defined for int32 or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_PERCENT:
            // int32 % int32 -> int32
            // uint32 % uint32 -> uint32
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = uint32_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "%% only defined for int32 or uint32");
                goto error_cleanup;
            }
            break;
        case CZ_TT_AMPERSAND:
            // int32 & int32 -> int32
            // uint32 & uint32 -> uint32
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = uint32_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "& only defined for int32 or uint32");
                goto error_cleanup;
            }
            break;
        case CZ_TT_BAR:
            // int32 | int32 -> int32
            // uint32 | uint32 -> uint32
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = uint32_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "| only defined for int32, uint32");
                goto error_cleanup;
            }
            break;
        case CZ_TT_CARET:
            // int32 ^ int32 -> int32
            // uint32 ^ uint32 -> uint32
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = uint32_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "^ only defined for int32, uint32");
                goto error_cleanup;
            }
            break;
        case CZ_TT_GREATER:
        case CZ_TT_LESS:
        case CZ_TT_GREATER_EQUAL:
        case CZ_TT_LESS_EQUAL:
            // int32 >= int32 -> bool
            // uint32 >= uint32 -> bool
            // float >= float -> bool
            // bool >= bool -> bool
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = bool_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = bool_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = bool_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "Comparison only defined for int32, uint32, or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_EQUAL_EQUAL:
        case CZ_TT_EXCLAMATION_EQUAL:
            // int32 == int32 -> bool
            // uint32 == uint32 -> bool
            // float == float -> bool
            // bool == bool -> bool
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = bool_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_UINT32 && rhs_type->primitive == CZ_PRIMITIVE_UINT32) {
                result_type = bool_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = bool_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_BOOL && rhs_type->primitive == CZ_PRIMITIVE_BOOL) {
                result_type = bool_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "Unordered comparison only defined for int32, uint32, or float.");
                goto error_cleanup;
            }
            break;
        default:
            break;
    }

    decor = cz_ast_decoration_create(result_type,
        CZ_VALUE_CATEGORY_RVALUE,
        lhs_node->decoration->is_constexpr && rhs_node->decoration->is_constexpr,
        false,
        MAX(lhs_node->decoration->scope_level, rhs_node->decoration->scope_level)
    );
    
    // Transfer decoration
    expr->decoration = decor;
    decor = NULL;

    return 1;

error_cleanup:
    cz_ast_decoration_free(decor);
    return 0;
}

static int cz_semantic_analyzer_check_unary_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    CZ_AST_Decoration* decor = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(expr);
    INVALID_NODE_TYPE_ERROR_HANDLE(expr, CZ_AST_UnaryExpressionNodeType);

    if (cz_semantic_analyzer_check_expression(sa, env, expr->unary_expression.operand) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Could not check operand for unary operation.");
        goto error_cleanup;
    }

    // Pre-fetch basic types
    const CZ_Type* int32_type = cz_global_type_table_find_type_by_name(sa->gtt, "int32");
    const CZ_Type* uint32_type = cz_global_type_table_find_type_by_name(sa->gtt, "uint32");
    const CZ_Type* float_type = cz_global_type_table_find_type_by_name(sa->gtt, "float");
    const CZ_Type* bool_type = cz_global_type_table_find_type_by_name(sa->gtt, "bool");
    NULL_POINTER_ERROR_HANDLE(int32_type);
    NULL_POINTER_ERROR_HANDLE(uint32_type);
    NULL_POINTER_ERROR_HANDLE(float_type);
    NULL_POINTER_ERROR_HANDLE(bool_type);

    const CZ_AST_Node* operand_node = expr->unary_expression.operand;
    const CZ_Type* operand_type = operand_node->decoration->resolved_type;

    // Decay and strip const
    operand_type = cz_type_decay_type(operand_type);

    if (operand_type->kind != CZ_TYPE_KIND_PRIMITIVE) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Invalid type. (Even after stripping reference and const, not a primitive)");
        goto error_cleanup;
    }

    // Now we have the primitive type in operand_type->primitive
    const CZ_Type* result_type = NULL;
    switch (expr->unary_expression.op) {
        case CZ_TT_MINUS:
            switch (operand_type->primitive) {
                case CZ_PRIMITIVE_INT32:
                    result_type = int32_type;
                    break;
                case CZ_PRIMITIVE_FLOAT:
                    result_type = float_type;
                    break;
                default:
                    cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "- unary operator only operates on signed numerical data");
                    goto error_cleanup;
            }
            break;
        case CZ_TT_EXCLAMATION:
            if (operand_type->primitive != CZ_PRIMITIVE_BOOL) {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "! unary operator only operates on boolean data");
                goto error_cleanup;
            }
            result_type = bool_type;
            break;
        default:
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Unknown unary operator");
            goto error_cleanup;
    }

    // Create decoration
    decor = cz_ast_decoration_create(result_type, CZ_VALUE_CATEGORY_RVALUE, operand_node->decoration->is_constexpr, false, expr->unary_expression.operand->decoration->scope_level);
    if (decor == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Allocating AST decorator failure.");
        goto error_cleanup;
    }

    // Transfer decoration
    expr->decoration = decor;
    decor = NULL;

    return 1;

error_cleanup:
    cz_ast_decoration_free(decor);
    return 0;
}

static int cz_semantic_analyzer_check_literal_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    CZ_AST_Decoration* decor = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(expr);
    INVALID_NODE_TYPE_ERROR_HANDLE(expr, CZ_AST_LiteralNodeType);

    const CZ_Type* type = NULL;

    switch (expr->literal.literal_type) {
        case CZ_TT_NUMERICAL_LITERAL:
            {
                const char* literal = expr->literal.lexeme;
                const char* decimal_point = strchr(literal, '.');
                if (decimal_point == NULL) {
                    // Integer
                    if (strchr(literal, 'u') == NULL) {
                        // Signed
                        type = cz_global_type_table_find_type_by_name(sa->gtt, "int32");
                    } else {
                        // Unsigned
                        type = cz_global_type_table_find_type_by_name(sa->gtt, "uint32");
                    }
                } else {
                    // Float
                    type = cz_global_type_table_find_type_by_name(sa->gtt, "float");
                }
            }
            break;
        case CZ_TT_TRUE:
        case CZ_TT_FALSE:
            // Bool
            type = cz_global_type_table_find_type_by_name(sa->gtt, "bool");
            break;
        case CZ_TT_STRING_LITERAL:
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "String literal not yet supported.");
            goto error_cleanup;
            break;
        default:
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Unknown literal type.");
            goto error_cleanup;
    }

    NULL_POINTER_ERROR_HANDLE(type);

    // Create decoration
    decor = cz_ast_decoration_create(type, CZ_VALUE_CATEGORY_RVALUE, true, false, 0);
    if (decor == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Allocating AST decorator failure.");
        goto error_cleanup;
    }

    // Transfer ownership to node.
    expr->decoration = decor;
    decor = NULL;

    return 1;

error_cleanup:
    cz_ast_decoration_free(decor);
    return 0;
}

static int cz_semantic_analyzer_check_identifier_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    CZ_AST_Decoration* decor = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(expr);
    INVALID_NODE_TYPE_ERROR_HANDLE(expr, CZ_AST_IdentifierNodeType);

    // 1. Look up symbol.
    const CZ_Symbol* symbol = cz_environment_lookup(env, expr->identifier.name, true);
    if (symbol == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Identifier %s is not in the symbol table.", expr->identifier.name);
        goto error_cleanup;
    }
    if (symbol->kind != CZ_SYMBOL_KIND_VALUE) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Identifier %s is not a value symbol.", expr->identifier.name);
        goto error_cleanup;
    }

    // 2. Decoration
    CZ_ValueCategory value_cat = CZ_VALUE_CATEGORY_LVALUE;

    if (symbol->data.value.type->kind == CZ_TYPE_KIND_FUNCTION) {
        value_cat = CZ_VALUE_CATEGORY_RVALUE;
    } else {
        // Both mutable variables AND named const/constexpr variables are L-values!
        value_cat = CZ_VALUE_CATEGORY_LVALUE;
    }

    //decor = cz_ast_decoration_create(symbol->data.value.type, value_cat, symbol->data.value.is_constexpr, symbol->data.value.type->kind == CZ_TYPE_KIND_REFERENCE, symbol->scope_level);
    decor = cz_ast_decoration_create(symbol->data.value.type, value_cat, false, symbol->data.value.type->kind == CZ_TYPE_KIND_REFERENCE, symbol->scope_level);
    if (decor == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Allocating AST decorator failure.");
        goto error_cleanup;
    }
    decor->is_escapable_ref = symbol->data.value.is_escapable_ref;

    // Transfer decoration
    expr->decoration = decor;
    decor = NULL;

    return 1;
error_cleanup:
    cz_ast_decoration_free(decor);
    return 0;
}

static int cz_semantic_analyzer_check_struct_access(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    CZ_AST_Decoration* decor = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(expr);
    INVALID_NODE_TYPE_ERROR_HANDLE(expr, CZ_AST_StructMemberAccessNodeType);

    // 1. Check base expression
    if (cz_semantic_analyzer_check_expression(sa, env, expr->struct_member_access.object) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, 
            "Could not deduce type of the base object for struct access.");
        goto error_cleanup;
    }

    // 2. Decay check base expression to strip references safely
    const CZ_Type* struct_type = expr->struct_member_access.object->decoration->resolved_type;
    const CZ_Type* decayed_struct_type = cz_type_decay_type(struct_type);
    NULL_POINTER_ERROR_HANDLE(decayed_struct_type);
    
    if (decayed_struct_type->kind != CZ_TYPE_KIND_STRUCT) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, 
            "Struct access requires base to be a struct");
        goto error_cleanup;
    }

    // 3. Check if struct member exists (Reading field count off the DECAYED type)
    const char* member_name = expr->struct_member_access.member->identifier.name;
    int member_idx = -1;
    for (unsigned int i = 0; i < decayed_struct_type->structure.layout->field_count; i++) {
        if (decayed_struct_type->structure.layout->fields[i].name == member_name ||
            strcmp(member_name, decayed_struct_type->structure.layout->fields[i].name) == 0) {
                member_idx = (int) i;
                break;
        }
    }
    if (member_idx < 0) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, 
            "Member name %s does not exist", member_name);
        goto error_cleanup; // Protect against out of bounds index checks below
    }

    // 4. Inherit L-value and propagate constness
    const CZ_Type* member_type = decayed_struct_type->structure.layout->fields[member_idx].type;
    CZ_ValueCategory value_cat = expr->struct_member_access.object->decoration->value_category;
    
    if (cz_type_is_const(struct_type) && !cz_type_is_const(member_type)) {
        // Safe lookups off your Global Type Table manager
        member_type = cz_type_table_get_or_create_const(sa->gtt, member_type);
        if (member_type == NULL) {
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, 
                "Failed to resolve internal const variant type mapping.");
            goto error_cleanup;
        }
    }

    // 5. Decorate the AST node safely
    decor = cz_ast_decoration_create(
        member_type,
        value_cat,
        expr->struct_member_access.object->decoration->is_constexpr,
        member_type->kind == CZ_TYPE_KIND_REFERENCE,
        env->scope_level
    );
    decor->is_escapable_ref = expr->struct_member_access.object->decoration->is_escapable_ref;
    if (decor == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, 
            "Allocating AST decorator failure.");
        goto error_cleanup;
    }

    expr->decoration = decor;
    decor = NULL;

    return 1;

error_cleanup:
    cz_ast_decoration_free(decor);
    return 0;
}

static int cz_semantic_analyzer_check_struct_init(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    CZ_AST_Decoration* decor = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(expr);
    INVALID_NODE_TYPE_ERROR_HANDLE(expr, CZ_AST_StructInitNodeType);

    // Reduce chain: expr->struct_declaration.identifier->identifier.name
    const CZ_AST_Node* struct_id_node = expr->struct_declaration.identifier;
    const char* struct_name = struct_id_node->identifier.name;

    // 1. Look up struct and check if it is a valid struct.
    const CZ_Type* struct_lookup = cz_global_type_table_find_type_by_name(sa->gtt, struct_name);
    if (struct_lookup == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
        "Symbol %s is not defined.", struct_name);
        goto error_cleanup;
    }
    if (struct_lookup->kind != CZ_TYPE_KIND_STRUCT) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
        "\"%s\" is not a struct.", struct_name);
        goto error_cleanup;
    }

    // Reduce chain: expr->struct_declaration.member_count and struct_lookup->structure.layout->field_count
    const unsigned int member_count = expr->struct_declaration.member_count;
    const unsigned int field_count = struct_lookup->structure.layout->field_count;

    if (member_count > field_count) {
        cz_error_list_push_error(
            sa->error_list, sa->filename, expr->line, expr->col,
            "There are only %d members in struct %s, but %d initializer "
            "members given",
            field_count, struct_name,
            member_count);
        goto error_cleanup;
    }


    // 2. Check that all const and reference initializers are actually given.
    // 2.1 All parameters are optional except for reference and const.
    for (unsigned int i = 0; i < field_count; i++) {
        const CZ_StructField* field = &struct_lookup->structure.layout->fields[i];
        const CZ_Type* field_type = field->type;
        if ((cz_type_is_const(field_type) || field_type->kind == CZ_TYPE_KIND_REFERENCE) &&
             field->default_initializer == NULL) {
            // Currently field is either const or reference, but default initializer is not given.
            // Check if it is part of the initializers.
            const char* field_name = field->name;
            bool found = false;
            for (unsigned int j = 0; j < member_count; j++) {
                // Reduce chain: expr->struct_declaration.members[j]->struct_init_member.identifier->identifier.name
                const CZ_AST_Node* member_node = expr->struct_declaration.members[j];
                const CZ_AST_Node* member_id_node = member_node->struct_init_member.identifier;
                const char* member_name = member_id_node->identifier.name;
                if (member_name == field_name || strcmp(member_name, field_name) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                "Initializer %s for struct %s is required.", field_name, struct_name);
                goto error_cleanup;
            }
        }

    }

    bool initializer_is_constexpr = true;
    // 3. For each initializer check with the struct definition
    for (unsigned int i = 0; i < member_count; i++) {
        // 3.1 Check duplicate member in initializer
        // Reduce chain: expr->struct_declaration.members[i]->struct_init_member.identifier->identifier.name
        const CZ_AST_Node* member_node_i = expr->struct_declaration.members[i];
        const CZ_AST_Node* member_id_node_i = member_node_i->struct_init_member.identifier;
        const char* member_name = member_id_node_i->identifier.name;
        for (unsigned int j = 0; j < i; j++) {
            // Reduce chain: expr->struct_declaration.members[j]->struct_init_member.identifier->identifier.name
            const CZ_AST_Node* member_node_j = expr->struct_declaration.members[j];
            const CZ_AST_Node* member_id_node_j = member_node_j->struct_init_member.identifier;
            const char* prev_member_name = member_id_node_j->identifier.name;
            if (member_name == prev_member_name || strcmp(member_name, prev_member_name) == 0) {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                "Initializer member %s is given duplicate.", struct_name);
                goto error_cleanup;
            }
        }
        // 3.2 Correct member names.
        int member_field_idx = -1;
        for (unsigned int j = 0; j < field_count; j++) {
            if (struct_lookup->structure.layout->fields[j].name == member_name ||
                strcmp(struct_lookup->structure.layout->fields[j].name, member_name) == 0) {
                member_field_idx = j;
                break;
            }
        }
        if (member_field_idx < 0) {
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Member name %s is not valid.", member_name);
            goto error_cleanup;
        }

        // 3.3. Check and match type with the struct member definitions
        // Reduce chain: expr->struct_declaration.members[i]->struct_init_member.expression
        CZ_AST_Node* member_expr = expr->struct_declaration.members[i]->struct_init_member.expression;
        if (cz_semantic_analyzer_check_expression(sa, env, member_expr) != 1) {
            goto error_cleanup;
        }
        // Reduce chain: expr->struct_declaration.members[i]->struct_init_member.expression->decoration
        const CZ_AST_Decoration* member_init_decor = member_expr->decoration;
        NULL_POINTER_ERROR_HANDLE(member_init_decor);
        // 3.3.1 Fields should match in decay types.
        const CZ_Type* decay_member_init_type = cz_type_decay_type(member_init_decor->resolved_type);
        // Reduce chain: struct_lookup->structure.layout->fields[member_field_idx].type
        const CZ_Type* field_type = struct_lookup->structure.layout->fields[member_field_idx].type;
        const CZ_Type* decay_field_type = cz_type_decay_type(field_type);
        if (!cz_type_equals(decay_member_init_type, decay_field_type)) {
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Type for initializer of member \"%s\" does not match the declared type.", member_name);
            goto error_cleanup;
        }
        // Reduce chain: struct_lookup->structure.layout->fields[member_field_idx].type->kind
        if (field_type->kind == CZ_TYPE_KIND_REFERENCE) {
            // 3.3.2 If field type is reference, initializer must be an l-value
            if (member_init_decor->value_category != CZ_VALUE_CATEGORY_LVALUE) {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                "Initializer of member \"%s\" is declared as reference, and l-value initializer is required", member_name);
                goto error_cleanup;
            }

            // 3.3.3 If field type is not const, then initializer must not be const.
            // Reduce chain: struct_lookup->structure.layout->fields[member_field_idx].type
            // Reduce chain: member_init_decor->resolved_type
            if (!cz_type_is_const(field_type) &&
                cz_type_is_const(member_init_decor->resolved_type)) {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                "Initializer of member \"%s\" is declared as reference, and initializer is const", member_name);
                goto error_cleanup;

            }
        }
        if (!member_init_decor->is_constexpr) {
            initializer_is_constexpr = false;
        }
    }

    decor = cz_ast_decoration_create(struct_lookup,
        CZ_VALUE_CATEGORY_RVALUE,
        initializer_is_constexpr,
        false,
        env->scope_level);
    if (decor == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Allocating AST decorator failure.");
        goto error_cleanup;
    }

    expr->decoration = decor;
    decor = NULL;

    return 1;

error_cleanup:
    cz_ast_decoration_free(decor);
    return 0;
}

static int cz_semantic_analyzer_cast_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    CZ_AST_Decoration* decor = NULL;
    NULL_POINTER_ERROR_HANDLE(sa);
    NULL_POINTER_ERROR_HANDLE(env);
    NULL_POINTER_ERROR_HANDLE(expr);
    INVALID_NODE_TYPE_ERROR_HANDLE(expr, CZ_AST_CastExpressionNodeType);

    // 1. Evaluate expression and type check
    if (cz_semantic_analyzer_check_expression(sa, env, expr->cast_expression.expression) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
        "Expression type could not be deduced for casting.");
        goto error_cleanup;
    }

    const CZ_Type* target_type = cz_type_from_type_node(expr->cast_expression.type, sa->gtt);
    if (target_type == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
        "Cast expression target type could not be deduced.");
        goto error_cleanup;
    }
    if (target_type->kind == CZ_TYPE_KIND_REFERENCE) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
        "Casting to reference type is not allowed.");
        goto error_cleanup;
    }

    // 2. Unwrap and check
    const CZ_Type* decayed_expr_type = cz_type_decay_type(expr->cast_expression.expression->decoration->resolved_type);
    const CZ_Type* decayed_target_type = cz_type_decay_type(target_type);
    NULL_POINTER_ERROR_HANDLE(decayed_expr_type);
    NULL_POINTER_ERROR_HANDLE(decayed_target_type);
    if ((decayed_expr_type->kind != CZ_TYPE_KIND_PRIMITIVE && decayed_expr_type->kind != CZ_TYPE_KIND_NEWTYPE) ||
    (decayed_target_type->kind != CZ_TYPE_KIND_PRIMITIVE && decayed_target_type->kind != CZ_TYPE_KIND_NEWTYPE)) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Casting is only supported between primitives and newtypes.");
        goto error_cleanup;
    }
    const CZ_Type* primitive_expr_type = decayed_expr_type->kind == CZ_TYPE_KIND_NEWTYPE ? decayed_expr_type->newtype.underlying : decayed_expr_type;
    const CZ_Type* primitive_target_type = decayed_target_type->kind == CZ_TYPE_KIND_NEWTYPE ? decayed_target_type->newtype.underlying : decayed_target_type;
    if (primitive_expr_type->kind != CZ_TYPE_KIND_PRIMITIVE || primitive_target_type->kind != CZ_TYPE_KIND_PRIMITIVE) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
        "Underlying type is not a primitive, which is not supported for casting.");
        goto error_cleanup;
    }

    if (primitive_expr_type->primitive == primitive_target_type->primitive) {
        // 2.1 Casting to same "machine type" is supported.
        // This is good.
    } else if (cz_primitive_type_is_numerical(primitive_expr_type->primitive) &&
        // 2.2 Numerical values can be casted to each other.
        cz_primitive_type_is_numerical(primitive_target_type->primitive)) {
        // This is good.
    } else {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
        "Casting not possible between these two.");
        goto error_cleanup;
    }
    
    decor = cz_ast_decoration_create(target_type,
        CZ_VALUE_CATEGORY_RVALUE,
        expr->cast_expression.expression->decoration->is_constexpr,
        false,
        env->scope_level);
    expr->decoration = decor;
    decor = NULL;

    return 1;
error_cleanup:
    cz_ast_decoration_free(decor);
    return 0;
}
