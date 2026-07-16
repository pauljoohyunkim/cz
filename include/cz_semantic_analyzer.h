#ifndef CZ_SEMANTIC_ANALYZER_H
#define CZ_SEMANTIC_ANALYZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cz_symbol_table.h"
#include "cz_parser.h"
#include "cz_error.h"

typedef struct {
    const char* filename;
    char* code;
    size_t code_length;
    CZ_Token* tokens;
    size_t n_tokens;
    CZ_AST_Node* program;
    CZ_StringPool* sp;

    CZ_Environment* global_env;
    CZ_GlobalTypeTable* gtt;

    CZ_Type* current_function_return;
    //bool is_inside_loop;

    CZ_ErrorList* error_list;
} CZ_SemanticAnalyzer;

/**
 * @brief Return read-only CZ_Type from Type Expression Node. Will add to GTT if it does not exist.
 * 
 * @param type_node Type expression node
 * @param gtt Global Type Table
 * @return CZ_Type* Read-only pointer to CZ_Type on success, NULL on failure
 */
const CZ_Type* cz_type_from_type_node(const CZ_AST_Node* type_node, CZ_GlobalTypeTable* gtt);

/**
 * @brief Creates semantic analyzer from parser
 * 
 * @param parser Pointer to CZ_Parser struct.
 * @return CZ_SemanticAnalyzer* Pointer to allocated semantic analyzer, or NULL if failure.
 * 
 * Note that dynamically allocated members of the parser are transferred to semantic analyzer, so parser can safely be freed with its designated free function.
 */
CZ_SemanticAnalyzer* cz_semantic_analyzer_create(CZ_Parser* parser);

/**
 * @brief Frees semantic analyzer and its components
 * 
 * @param sa Pointer to CZ_SemanticAnalyzer
 */
void cz_semantic_analyzer_free(CZ_SemanticAnalyzer* sa);

/**
 * @brief Invoke semantic analyzer to analyze the AST.
 * 
 * @param sa Pointer to CZ_SemanticAnalyzer
 * @return int 1 on success, 0 on failure.
 * 
 * This decorates the AST and populates symbol table and global type table.
 * Note that just because 1 is returned does not mean the code is error free.
 * Always check the error list afterwards.
 */
int cz_semantic_analyzer_analyze(CZ_SemanticAnalyzer* sa);

#ifdef __cplusplus
}
#endif
#endif  /* CZ_SEMANTIC_ANALYZER_H */

