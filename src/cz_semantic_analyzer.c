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
        case CZ_AST_TYPE_KIND_INT32: {
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
        case CZ_AST_TYPE_KIND_BOOL: {
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
        case CZ_AST_TYPE_KIND_FLOAT: {
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
        case CZ_AST_TYPE_KIND_IDENTIFIER: {
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

    // Populate with primitive types.
    // cz_global_type_table_push_type(gtt, "int32", cz_type_create(CZ_PRIMITIVE_INT32));
    // cz_global_type_table_push_type(gtt, "bool", cz_type_create(CZ_PRIMITIVE_BOOL));
    // cz_global_type_table_push_type(gtt, "float", cz_type_create(CZ_PRIMITIVE_FLOAT));

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
    cz_semantic_analyzer_free(sa);
    cz_error_list_free(error_list);
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
static int cz_semantic_analyzer_register_function_decl(CZ_Environment* env, const CZ_AST_Node* decl);
static int cz_semantic_analyzer_register_struct_decl(CZ_Environment* env, const CZ_AST_Node* decl);
static int cz_semantic_analyzer_register_variable_decl(CZ_Environment* env, const CZ_AST_Node* decl);
static int cz_semantic_analyzer_register_typedef(CZ_Environment* env, const CZ_AST_Node* decl);
static int cz_semantic_analyzer_register_newtypedef(CZ_Environment* env, const CZ_AST_Node* decl);

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
                //cz_semantic_analyzer_register_function_decl(sa->global_env, statement);
                break;
            case CZ_AST_StructDeclarationNodeType:
                //cz_semantic_analyzer_register_struct_decl(sa->global_env, statement);
                break;
            case CZ_AST_VariableDeclarationNodeType:
                //cz_semantic_analyzer_register_variable_decl(sa->global_env, statement);
                break;
            case CZ_AST_TypedefDeclarationNodeType:
                cz_semantic_analyzer_register_typedef(sa->global_env, statement);
                break;
            case CZ_AST_NewtypeDeclarationNodeType:
                //cz_semantic_analyzer_register_newtypedef(sa->global_env, statement);
                break;
            default:
                cz_error_list_push_error(sa->error_list, sa->filename, statement->line, statement->col, "Unrecognized global statement.");
                break;
        }
    }

error_cleanup:
    return 0;
}

static int cz_semantic_analyzer_register_typedef(CZ_Environment* env, const CZ_AST_Node* decl) {
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(decl, error_cleanup);
    INVALID_NODE_TYPE_TO_GOTO(decl, CZ_AST_TypedefDeclarationNodeType, error_cleanup);



    return 1;

error_cleanup:
    return 0;
}
