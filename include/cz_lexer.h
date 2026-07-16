#ifndef LEXER_H
#define LEXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stddef.h>
#include "cz_tokens.h"
#include "cz_error.h"

typedef struct {
    const char* filename;
    char* code;
    size_t code_length;
    unsigned int idx;
    unsigned int row;
    unsigned int col;
    CZ_Token* tokens;
    size_t n_tokens_capacity;
    size_t n_tokens;
    CZ_ErrorList* error_list;
} CZ_Lexer;

/**
 * @brief Creates CZ_Lexer.
 * 
 * @param code Pointer to raw code. Note that the created lexer will allocate and copy code from this.
 * @param filename Filename. Ownership of filename string must be global.
 * @return CZ_Lexer* Pointer to CZ_Lexer struct upon success. NULL if failure.
 */
CZ_Lexer* cz_lexer_create(const char* code, const char* filename);

/**
 * @brief Analyzes the code and fills token array.
 * 
 * @param lexer Pointer to CZ_Lexer struct.
 * @return int 1 on success, 0 on failure.
 * 
 * After success, tokens should be stored in lexer->tokens. The number of tokens is stored in lexer->n_tokens.
 * Note that this may return 1 even if there are unrecognized tokens. Must check each token if it is of type CZ_TT_UNKNOWN for error.
 */
int cz_lexer_analyze(CZ_Lexer* lexer);

/**
 * @brief Frees CZ_Lexer and its components.
 * 
 * @param lexer Pointer to CZ_Lexer struct.
 */
void cz_lexer_free(CZ_Lexer* lexer);

#ifdef __cplusplus
}
#endif

#endif  /* LEXER_H */
