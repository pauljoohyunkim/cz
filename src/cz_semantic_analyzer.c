#include <string.h>
#include "cz_parser.h"
#include "cz_semantic_analyzer.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)
#define INVALID_NODE_TYPE_TO_GOTO(node, node_type_enum, label) do { if ((node)->node_type != (node_type_enum)) goto label; } while (0)

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

    // --- PHASE 2: Apply the const wrapper if requested by the AST ---
    if (type_node->type_expression.is_const) {
        // Build a temporary query for the const wrapper pointing to our base type
        CZ_Type const_query = {
            .kind = CZ_TYPE_KIND_CONST,
            .const_of = (CZ_Type*)base_type
        };

        const CZ_Type* existing_const = cz_global_type_table_find_type(gtt, &const_query);
        if (existing_const != NULL) {
            return existing_const;
        }

        // It doesn't exist, allocate the wrapper type
        CZ_Type* new_const_type = cz_type_create(CZ_TYPE_KIND_CONST);
        NULL_POINTER_TO_GOTO(new_const_type, error_cleanup);
        new_const_type->const_of = base_type;

        if (cz_global_type_table_push_type(gtt, NULL, new_const_type) != 1) {
            cz_type_free(new_const_type);
            goto error_cleanup;
        }
        return new_const_type;
    }

    return base_type;

error_cleanup:
    // Notice that we don't have to clean up `type` here anymore. 
    // Any newly created types were either pushed to the GTT successfully 
    // or freed immediately upon failure. No leaks!
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

// Will be invoking pass 1 and pass 2.
int cz_semantic_analyzer_analyze(CZ_SemanticAnalyzer* sa) {
    NULL_POINTER_TO_GOTO(sa, error_cleanup);

    cz_semantic_analyzer_build_global_table(sa);

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
                //cz_semantic_analyzer_register_struct_decl(sa->global_env, statement);
                break;
            case CZ_AST_VariableDeclarationNodeType:
                //cz_semantic_analyzer_register_variable_decl(sa->global_env, statement);
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
    CZ_Type** func_param_types = NULL;
    CZ_Symbol* func_symbol = NULL;
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
    func_param_types = (CZ_Type**) calloc(func_param_count, sizeof(CZ_Type*));
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
            goto error_cleanup;
        }
    } else {
        // 3.2 GTT already contains the function signature. Use the lookup and free the param_types
        free(func_type_query.function.param_types);
        func_type = func_type_lookup;
    }
    
    // 4. Add symbol.
    func_symbol = cz_symbol_create(CZ_SYMBOL_KIND_VALUE, decl->function_declaration.function_identifier->identifier.name);
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

static int cz_semantic_analyzer_register_typedef(CZ_SemanticAnalyzer* sa, CZ_Environment* env, const CZ_AST_Node* decl) {
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