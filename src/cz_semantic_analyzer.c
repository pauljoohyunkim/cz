#include <string.h>
#include "cz_parser.h"
#include "cz_type.h"
#include "cz_semantic_analyzer.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)
#define INVALID_NODE_TYPE_TO_GOTO(node, node_type_enum, label) do { if ((node)->node_type != (node_type_enum)) goto label; } while (0)
#define MAX(x,y) ((x) > (y) ? (x) : (y))

const CZ_Type* cz_type_from_type_node(const CZ_AST_Node* type_node, CZ_GlobalTypeTable* gtt) {
    const CZ_Type* base_type = NULL;
    const char* query_name = NULL;

    NULL_POINTER_TO_GOTO(type_node, error_cleanup);
    NULL_POINTER_TO_GOTO(gtt, error_cleanup);
    if (type_node->node_type != CZ_AST_TypeNodeType) goto error_cleanup;

    if (type_node->type_expression.is_function_type) {
        printf("Currently function type not supported.\n");
        goto error_cleanup;
    }

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
                NULL_POINTER_TO_GOTO(new_type, error_cleanup);
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
                NULL_POINTER_TO_GOTO(new_type, error_cleanup);
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
                NULL_POINTER_TO_GOTO(new_type, error_cleanup);
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
                NULL_POINTER_TO_GOTO(new_type, error_cleanup);
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
            NULL_POINTER_TO_GOTO(query_name, error_cleanup);

            // Lookup the named type (which is inherently non-const in the registry)
            base_type = cz_global_type_table_find_type_by_name(gtt, query_name);
            NULL_POINTER_TO_GOTO(base_type, error_cleanup);
            break;
        }
        default:
            goto error_cleanup;
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
            NULL_POINTER_TO_GOTO(new_const_type, error_cleanup);
            new_const_type->const_of = current_type;

            if (cz_global_type_table_push_type(gtt, NULL, new_const_type) != 1) {
                cz_type_free(new_const_type);
                goto error_cleanup;
            }
            current_type = new_const_type; // Update tracking pointer
        }
    }

    // --- PHASE 3: Apply the reference wrapper if requested by the AST ---
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
            NULL_POINTER_TO_GOTO(new_ref_type, error_cleanup);
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

CZ_SemanticAnalyzer* cz_semantic_analyzer_create(CZ_Parser* parser) {
    CZ_SemanticAnalyzer* sa = NULL;
    CZ_Environment* global_env = NULL;
    CZ_GlobalTypeTable* gtt = NULL;
    CZ_ErrorList* error_list = NULL;
    if (parser == NULL) return NULL;

    sa = (CZ_SemanticAnalyzer*) calloc(1, sizeof(CZ_SemanticAnalyzer));
    NULL_POINTER_TO_GOTO(sa, error_cleanup);

    global_env = cz_environment_create();
    NULL_POINTER_TO_GOTO(global_env, error_cleanup);

    gtt = cz_global_type_table_create();
    NULL_POINTER_TO_GOTO(gtt, error_cleanup);

    error_list = cz_error_list_create();
    NULL_POINTER_TO_GOTO(error_list, error_cleanup);

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
        free(sa->code);
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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);

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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(sa->program, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(sa->program, CZ_AST_ProgramNodeType, error_cleanup);
    NULL_POINTER_TO_GOTO(sa->program->program.global_declaration_list, error_cleanup);

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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(decl, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(decl, CZ_AST_FunctionDeclarationNodeType, error_cleanup);

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
    NULL_POINTER_TO_GOTO(func_param_types, error_cleanup);

    // 2.2.2 Populate the parameter types list.
    for (unsigned int i = 0; i < func_param_count; i++) {
        const CZ_Type* func_param_type = cz_type_from_type_node(func_param_list->parameter_list.params[i]->variable_declaration.type, sa->gtt);
        NULL_POINTER_TO_GOTO(func_param_type, error_cleanup);
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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(decl, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(decl, CZ_AST_StructDeclarationNodeType, error_cleanup);

    //struct Vector {
    //    x :: int32 = 1,
    //    y :: int32 = 0,
    //    z :: int32
    //};

    const char* struct_name = decl->struct_declaration.identifier->identifier.name;
    NULL_POINTER_TO_GOTO(struct_name, error_cleanup);

    // 1. Check if struct name exists in the global type table. If so, bad.
    const CZ_Type* struct_type_lookup = cz_global_type_table_find_type_by_name(sa->gtt, struct_name);
    if (struct_type_lookup != NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                                 "Type \"%s\" already defined previously", struct_name);
        goto error_cleanup;
    }

    // 2. Create struct type with NULL layout (to be populated in pass 2)
    struct_type = cz_type_create(CZ_TYPE_KIND_STRUCT);
    NULL_POINTER_TO_GOTO(struct_type, error_cleanup);

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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(decl, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(decl, CZ_AST_VariableDeclarationNodeType, error_cleanup);

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

    // 3. Create symbol and add it to symbol table.
    variable_symbol = cz_symbol_create(CZ_SYMBOL_KIND_VALUE, variable_node->identifier.name, 0);
    if (variable_symbol == NULL) {
        cz_error_list_push_error(sa->error_list, sa->filename, type_node->line, type_node->col,
                                 "Symbol \"%s\" could not be created", variable_node->identifier.name);
        goto error_cleanup;
    }
    variable_symbol->data.value.type = variable_type;
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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(decl, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(decl, CZ_AST_TypedefDeclarationNodeType, error_cleanup);

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
    NULL_POINTER_TO_GOTO(type, error_cleanup);
    NULL_POINTER_TO_GOTO(name, error_cleanup);

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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(decl, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(decl, CZ_AST_NewtypeDeclarationNodeType, error_cleanup);

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

/**
 * @brief Simple helper for stripping const and reference.
 * 
 * @param type Pointer to CZ_Type
 * @return const CZ_Type* Pointer to unwrapped CZ_Type.
 */
static const CZ_Type* cz_semantic_analyzer_decay_operand_type(const CZ_Type* type) {
    if (type->kind == CZ_TYPE_KIND_REFERENCE) {
        type = type->reference_to;
    }
    if (type->kind == CZ_TYPE_KIND_CONST) {
        type = type->const_of;
    }
    return type;
}
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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(type, error_cleanup);
    NULL_POINTER_TO_GOTO(states, error_cleanup);

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

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_full_analyze(CZ_SemanticAnalyzer* sa) {
    CZ_StructRecursiveCycleState* states = NULL;
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(sa->program, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(sa->program, CZ_AST_ProgramNodeType, error_cleanup);
    NULL_POINTER_TO_GOTO(sa->program->program.global_declaration_list, error_cleanup);

    for (unsigned int i = 0; i < sa->program->program.declaration_count; i++) {
        const CZ_AST_Node* statement = sa->program->program.global_declaration_list[i];
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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(decl, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(decl, CZ_AST_FunctionDeclarationNodeType, error_cleanup);

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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(decl, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(decl, CZ_AST_StructDeclarationNodeType, error_cleanup);

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
            const CZ_AST_Decoration* initializer_decor = member_node->decoration;
            if (!initializer_decor->is_constexpr) {
                cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col, "Initializer for member idx %d is not constexpr.", i+1);
                goto error_cleanup;
            }

            // 3.2.2 Decay type match
            const CZ_Type* decay_member_type = cz_semantic_analyzer_decay_operand_type(field_type);
            const CZ_Type* decay_expr_type = cz_semantic_analyzer_decay_operand_type(initializer_decor->resolved_type);
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

        }
    }


    return 1;

error_cleanup:
    cz_struct_layout_free(struct_layout);
    return 0;
}

static int cz_semantic_analyzer_check_global_var_init(CZ_SemanticAnalyzer* sa, CZ_AST_Node* decl) {
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(decl, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(decl, CZ_AST_VariableDeclarationNodeType, error_cleanup);

    // x :: int32 = 1 + 2;
    // y :: int32& = x;

    // 1. Look up symbol for LHS.
    const char* var_name = decl->variable_declaration.identifier->identifier.name;
    CZ_Symbol* var_symbol = (CZ_Symbol*) cz_environment_lookup(sa->global_env, var_name, false);
    NULL_POINTER_TO_GOTO(var_symbol, error_cleanup);
    const CZ_Type* var_decl_type = var_symbol->data.value.type;

    // 2. Check if there is an initializer.
    // 2.1 If no initializer, check if variable is either const or reference. (If either is true, error)
    if (decl->variable_declaration.expression == NULL) {
        if (var_decl_type->kind == CZ_TYPE_KIND_CONST || var_decl_type->kind == CZ_TYPE_KIND_REFERENCE) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                "\"%s\" is declared without initializer, but is const or reference.", var_name);
            goto error_cleanup;
        }

        // Nothing more to check since RHS does not exist.
        return 1;
    }
    
    // 3. Check expression RHS.
    if (cz_semantic_analyzer_check_expression(sa, sa->global_env, decl->variable_declaration.expression) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
            "Could not determine the initializer for %s.", var_name);
        goto error_cleanup;
    }

    const CZ_AST_Decoration* rhs_decoration = decl->variable_declaration.expression->decoration;
    NULL_POINTER_TO_GOTO(rhs_decoration, error_cleanup);

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
    
        // Check if the underlying referenced type is non-const, but the RHS is const
        if (referenced_type->kind != CZ_TYPE_KIND_CONST && rhs_decoration->resolved_type->kind == CZ_TYPE_KIND_CONST) {
            cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
                "Cannot bind non-const reference \"%s\" to a const value.", var_name);
            goto error_cleanup;
        }

    }

    // 5. If explicit type, do types match after decaying.
    const CZ_Type* decayed_lhs_type = cz_semantic_analyzer_decay_operand_type(var_decl_type);
    const CZ_Type* decayed_rhs_type = cz_semantic_analyzer_decay_operand_type(rhs_decoration->resolved_type);
    if (!cz_type_equals(decayed_lhs_type, decayed_rhs_type)) {
        cz_error_list_push_error(sa->error_list, sa->filename, decl->line, decl->col,
            "Variable  \"%s\" type does not match the type of RHS.", var_name);
        goto error_cleanup;
    }
    
    // 6. Update symbol.
    var_symbol->data.value.is_constexpr = rhs_decoration->is_constexpr;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_statement_list(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* block) {
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(block, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(block, CZ_AST_BlockStatementNodeType, error_cleanup);

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

//static int cz_semantic_analyzer_check_variable_declaration_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt);
static int cz_semantic_analyzer_check_return_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt);

static int cz_semantic_analyzer_check_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt) {
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(stmt, error_cleanup);

    switch (stmt->node_type) {
        case CZ_AST_VariableDeclarationNodeType:
            //if (cz_semantic_analyzer_check_variable_declaration_statement(sa, env, stmt) != 1) {
            //    goto error_cleanup;
            //}
            break;
        case CZ_AST_AssignmentStatementNodeType:
            break;
        case CZ_AST_ReturnStatementNodeType:
            if (cz_semantic_analyzer_check_return_statement(sa, env, stmt) != 1) {
                goto error_cleanup;
            }
            break;
        case CZ_AST_IfStatementNodeType:
            break;
        case CZ_AST_ForStatementNodeType:
            break;
        case CZ_AST_WhileStatementNodeType:
            break;
        case CZ_AST_BlockStatementNodeType:
            break;
        default:
            // Expression
            break;
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_return_statement(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* stmt) {
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(stmt, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(stmt, CZ_AST_ReturnStatementNodeType, error_cleanup);
    NULL_POINTER_TO_GOTO(sa->current_function_return, error_cleanup);

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
    const CZ_Type* decayed_return_type = cz_semantic_analyzer_decay_operand_type(sa->current_function_return);
    NULL_POINTER_TO_GOTO(decayed_return_type, error_cleanup);
    const CZ_Type* decayed_expr_type = cz_semantic_analyzer_decay_operand_type(stmt->return_statement.expression->decoration->resolved_type);
    NULL_POINTER_TO_GOTO(decayed_expr_type, error_cleanup);
    if (!cz_type_equals(decayed_return_type, decayed_expr_type)) {
        cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
            "Return type of function does not match the expression type of return statement.");
        goto error_cleanup;
    }

    // 3. Reference handling
    if (sa->current_function_return->kind == CZ_TYPE_KIND_REFERENCE) {
        // 3.1 Cannot return local scope.
        if (stmt->return_statement.expression->decoration->scope_level >= 2) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Return type of function is reference, so cannot return local scope expressions.");
            goto error_cleanup;
        }
        // 3.2 Parameter can be returned only if it is reference.
        if (stmt->return_statement.expression->node_type == CZ_AST_IdentifierNodeType) {
            const CZ_Symbol* identifier = cz_environment_lookup(env, stmt->return_statement.expression->identifier.name, true);
            // Check if it is actually parameter.
            if (identifier->scope_level == 1 && identifier->data.value.type->kind != CZ_TYPE_KIND_REFERENCE) {
                cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                    "Return type of function is reference and you are trying to return a non-reference parameter.");
                goto error_cleanup;
            }
        }
        // 3.3 Returning expression must be L-value
        if (stmt->return_statement.expression->decoration->value_category != CZ_VALUE_CATEGORY_LVALUE) {
            cz_error_list_push_error(sa->error_list, sa->filename, stmt->line, stmt->col,
                "Return type of reference returning function is should be l-value.");
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

static int cz_semantic_analyzer_check_binary_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_check_unary_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_check_literal_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);
static int cz_semantic_analyzer_check_identifier_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr);

static int cz_semantic_analyzer_check_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(expr, error_cleanup);

    switch (expr->node_type) {
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
        default:
            cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Not yet supported.");
            break;
            
    }

    return 1;

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_check_binary_expression(CZ_SemanticAnalyzer* sa, CZ_Environment* env, CZ_AST_Node* expr) {
    CZ_AST_Decoration* decor = NULL;
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(expr, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(expr, CZ_AST_BinaryExpressionNodeType, error_cleanup);

    if (cz_semantic_analyzer_check_expression(sa, env, expr->binary_expression.left) != 1) {
        goto error_cleanup;
    }
    if (cz_semantic_analyzer_check_expression(sa, env, expr->binary_expression.right) != 1) {
        goto error_cleanup;
    }

    // Pre-fetch basic types
    const CZ_Type* int32_type = cz_global_type_table_find_type_by_name(sa->gtt, "int32");
    const CZ_Type* float_type = cz_global_type_table_find_type_by_name(sa->gtt, "float");
    const CZ_Type* bool_type = cz_global_type_table_find_type_by_name(sa->gtt, "bool");
    NULL_POINTER_TO_GOTO(int32_type, error_cleanup);
    NULL_POINTER_TO_GOTO(float_type, error_cleanup);
    NULL_POINTER_TO_GOTO(bool_type, error_cleanup);

    const CZ_AST_Node* lhs_node = expr->binary_expression.left;
    const CZ_Type* lhs_type = lhs_node->decoration->resolved_type;
    const CZ_AST_Node* rhs_node = expr->binary_expression.right;
    const CZ_Type* rhs_type = rhs_node->decoration->resolved_type;

    // Strip away reference and const.
    lhs_type = cz_semantic_analyzer_decay_operand_type(lhs_type);
    rhs_type = cz_semantic_analyzer_decay_operand_type(rhs_type);

    if (lhs_type->kind != CZ_TYPE_KIND_PRIMITIVE || rhs_type->kind != CZ_TYPE_KIND_PRIMITIVE) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
            "Invalid type for either the LHS or RHS of binary operation. (Even after stripping reference and const, not a primitive)");
        goto error_cleanup;
    }

    const CZ_Type* result_type = NULL;
    switch (expr->binary_expression.op) {
        case CZ_TT_PLUS:
            // int32 + int32 -> int32
            // float + float -> float
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = float_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "+ only defined for int32 or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_MINUS:
            // int32 - int32 -> int32
            // float - float -> float
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = float_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "- only defined for int32 or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_STAR:
            // int32 * int32 -> int32
            // float * float -> float
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = float_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "* only defined for int32 or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_SLASH:
            // int32 / int32 -> int32
            // float / float -> float
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = float_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "/ only defined for int32 or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_PERCENT:
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "/ only defined for int32");
                goto error_cleanup;
            }
            break;
        case CZ_TT_AMPERSAND:
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "& only defined for int32");
                goto error_cleanup;
            }
            break;
        case CZ_TT_BAR:
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "| only defined for int32");
                goto error_cleanup;
            }
            break;
        case CZ_TT_CAROT:
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = int32_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "^ only defined for int32");
                goto error_cleanup;
            }
            break;
        case CZ_TT_GREATER:
        case CZ_TT_LESS:
        case CZ_TT_GREATER_EQUAL:
        case CZ_TT_LESS_EQUAL:
            // int32 >= int32 -> bool
            // float >= float -> bool
            // bool >= bool -> bool
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = bool_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = bool_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "Unordered comparison only defined for int32 or float.");
                goto error_cleanup;
            }
            break;
        case CZ_TT_EQUAL_EQUAL:
        case CZ_TT_EXCLAMATION_EQUAL:
            if (lhs_type->primitive == CZ_PRIMITIVE_INT32 && rhs_type->primitive == CZ_PRIMITIVE_INT32) {
                result_type = bool_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_FLOAT && rhs_type->primitive == CZ_PRIMITIVE_FLOAT) {
                result_type = bool_type;
            } else if (lhs_type->primitive == CZ_PRIMITIVE_BOOL && rhs_type->primitive == CZ_PRIMITIVE_BOOL) {
                result_type = bool_type;
            } else {
                cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col,
                    "Unordered comparison only defined for int32 or float.");
                goto error_cleanup;
            }
            break;
        default:
            break;
    }

    decor = cz_ast_decoration_create(result_type,
        CZ_VALUE_CATEGORY_RVALUE,
        lhs_node->decoration->is_constexpr && rhs_node->decoration->is_constexpr,
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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(expr, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(expr, CZ_AST_UnaryExpressionNodeType, error_cleanup);

    if (cz_semantic_analyzer_check_expression(sa, env, expr->unary_expression.operand) != 1) {
        cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "Could not check operand for unary operation.");
        goto error_cleanup;
    }

    // Pre-fetch basic types
    const CZ_Type* int32_type = cz_global_type_table_find_type_by_name(sa->gtt, "int32");
    const CZ_Type* float_type = cz_global_type_table_find_type_by_name(sa->gtt, "float");
    const CZ_Type* bool_type = cz_global_type_table_find_type_by_name(sa->gtt, "bool");
    NULL_POINTER_TO_GOTO(int32_type, error_cleanup);
    NULL_POINTER_TO_GOTO(float_type, error_cleanup);
    NULL_POINTER_TO_GOTO(bool_type, error_cleanup);

    const CZ_AST_Node* operand_node = expr->unary_expression.operand;
    const CZ_Type* operand_type = operand_node->decoration->resolved_type;

    // Decay and strip const
    operand_type = cz_semantic_analyzer_decay_operand_type(operand_type);

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
                    cz_error_list_push_error(sa->error_list, sa->filename, expr->line, expr->col, "- unary operator only operates on numerical data");
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
    decor = cz_ast_decoration_create(result_type, CZ_VALUE_CATEGORY_RVALUE, operand_node->decoration->is_constexpr, expr->unary_expression.operand->decoration->scope_level);
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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(expr, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(expr, CZ_AST_LiteralNodeType, error_cleanup);

    const CZ_Type* type = NULL;

    switch (expr->literal.literal_type) {
        case CZ_TT_NUMERICAL_LITERAL:
            {
                const char* literal = expr->literal.lexeme;
                const char* decimal_point = strchr(literal, '.');
                if (decimal_point == NULL) {
                    // Integer
                    type = cz_global_type_table_find_type_by_name(sa->gtt, "int32");
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

    NULL_POINTER_TO_GOTO(type, error_cleanup);

    // Create decoration
    decor = cz_ast_decoration_create(type, CZ_VALUE_CATEGORY_RVALUE, true, 0);
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
    NULL_POINTER_TO_GOTO(sa, error_cleanup);
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(expr, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(expr, CZ_AST_IdentifierNodeType, error_cleanup);

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

    // A function symbol or a compile-time constant cannot be an L-value (assignable)
    if (symbol->data.value.type->kind == CZ_TYPE_KIND_FUNCTION || symbol->data.value.is_constexpr) {
        value_cat = CZ_VALUE_CATEGORY_RVALUE;
    }

    decor = cz_ast_decoration_create(symbol->data.value.type, value_cat, symbol->data.value.is_constexpr, symbol->scope_level);
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