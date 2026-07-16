#ifndef CZ_PARSER_H
#define CZ_PARSER_H

#include <stddef.h>
#include "cz_tokens.h"
#include "cz_lexer.h"
#include "cz_error.h"
#include "cz_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* filename;
    char* code;
    size_t code_length;
    CZ_Token* tokens;
    size_t n_tokens;
    size_t idx;
    CZ_AST_Node* program;
    CZ_ErrorList* error_list;
} CZ_Parser;


/**
 * @brief Creates CZ_Parser.
 * 
 * @param lexer Pointer to CZ_Lexer struct.
 * @return CZ_Parser* Pointer to CZ_Parser struct upon sucess. NULL if failure.
 * 
 * Note that ownership of code string and token array are moved to Parser.
 * Lexer can be safely freed.
 */
CZ_Parser* cz_parser_create(CZ_Lexer* lexer);

/**
 * @brief Parse tokens to AST.
 * 
 * @param parser Pointer to CZ_Parser struct.
 * @return int 1 if successful. 0 otherwise.
 * 
 * The root node of AST is stored at parser->program.
 */
int cz_parser_parse(CZ_Parser* parser);

/**
 * @brief Frees the CZ_Parser.
 * 
 * @param parser Pointer to CZ_Parser struct.
 * 
 * Note that internal components are also freed.
 */
void cz_parser_free(CZ_Parser* parser);

#ifdef __cplusplus
}
#endif

#endif  /* CZ_PARSER_H */
