#include "cz_parser.h"
#include "cz_semantic_analyzer.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)
#define INVALID_NODE_TYPE_TO_GOTO(node, node_type_enum, label) do { if ((node)->node_type != (node_type_enum)) goto label; } while (0)

CZ_SemanticAnalyzer* cz_semantic_analyzer_create(CZ_Parser* parser) {
    CZ_SemanticAnalyzer* sa = NULL;
    CZ_Environment* global_env = NULL;
    CZ_ErrorList* error_list = NULL;
    if (parser == NULL) return NULL;

    sa = (CZ_SemanticAnalyzer*) calloc(1, sizeof(CZ_SemanticAnalyzer));
    NULL_POINTER_TO_GOTO(sa, error_cleanup);

    global_env = cz_environment_create();
    NULL_POINTER_TO_GOTO(global_env, error_cleanup);

    error_list = cz_error_list_create();
    NULL_POINTER_TO_GOTO(error_list, error_cleanup);

    // Transfer global_env inside the semantic analyzer.
    sa->global_env = global_env;
    global_env = NULL;

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

    return sa;
error_cleanup:
    cz_environment_free(global_env);
    cz_semantic_analyzer_free(sa);
    cz_error_list_free(error_list);
    return NULL;
}

void cz_semantic_analyzer_free(CZ_SemanticAnalyzer* sa) {
    if (sa != NULL) {
        free(sa->code);
        free(sa->tokens);
        cz_ast_root_free(sa->program);
        cz_environment_free(sa->global_env);
        cz_error_list_free(sa->error_list);
    }
    free(sa);
}
