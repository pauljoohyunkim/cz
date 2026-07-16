#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "cz_lexer.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)

static inline char* create_null_terminated_string(const char* text, size_t length) {
    char* nt_str = NULL;
    if (text == NULL || length == 0) goto error_cleanup;

    nt_str = (char*) malloc(sizeof(char) * (length + 1));
    NULL_POINTER_TO_GOTO(nt_str, error_cleanup);

    memcpy(nt_str, text, length);
    nt_str[length] = '\0';

    return nt_str;

error_cleanup:
    return NULL;
}


CZ_StringPool* cz_string_pool_create(void) {
    const char** strings = NULL;
    CZ_StringPool* sp = NULL;

    sp = (CZ_StringPool*) calloc(1, sizeof(CZ_StringPool));
    NULL_POINTER_TO_GOTO(sp, error_cleanup);

    sp->capacity = 8;
    strings = (const char**) calloc(sp->capacity, sizeof(const char*));
    NULL_POINTER_TO_GOTO(strings, error_cleanup);

    sp->strings = strings;
    strings = NULL;

    return sp;
error_cleanup:
    free(strings);
    cz_string_pool_free(sp);
    return NULL;
}

void cz_string_pool_free(CZ_StringPool* sp) {
    if (sp != NULL) {
        for (unsigned int i = 0; i < sp->count; i++) {
            free((void*)sp->strings[i]);
            sp->strings[i] = NULL;
        }
    }
    free(sp);
}

const char* cz_string_pool_push(CZ_StringPool* sp, const char* text, size_t length) {
    const char* nt_str = NULL;
    const char** new_strings = NULL;
    NULL_POINTER_TO_GOTO(sp, error_cleanup);
    NULL_POINTER_TO_GOTO(text, error_cleanup);
    if (length == 0) goto error_cleanup;

    // Search pool
    for (unsigned int i = 0; i < sp->count; i++) {
        if (length == strlen(sp->strings[i]) && strncmp(sp->strings[i], text, length) == 0) {
            nt_str = sp->strings[i];
            break;
        }
    }

    if (nt_str == NULL) {
        // Create and push.
        if (sp->capacity == sp->count) {
            // Increase capacity.
            new_strings = (const char**) realloc(sp->strings, sizeof(const char*) * sp->capacity * 2);
            NULL_POINTER_TO_GOTO(new_strings, error_cleanup);

            // Move ownership of newly allocated strings.
            sp->strings = new_strings;
            new_strings = NULL;
            sp->capacity *= 2;
        }

        // Create string
        nt_str = create_null_terminated_string(text, length);
        NULL_POINTER_TO_GOTO(nt_str, error_cleanup);

        // Transfer string (but keep the shallow copy)
        sp->strings[sp->count] = nt_str;

        sp->count++;
    }

    // Return the pointer to inside string pool
    return nt_str;
error_cleanup:
    free((void*)nt_str);
    free(new_strings);
    return NULL;
}

CZ_Lexer* cz_lexer_create(const char* code, const char* filename) {
    if (code == NULL) {
        return NULL;
    }

    const size_t code_length = strlen(code);

    CZ_Lexer* lexer = (CZ_Lexer*) calloc(1, sizeof(CZ_Lexer));
    // Token array allocation
    lexer->n_tokens_capacity = 8;
    lexer->n_tokens = 0;
    lexer->tokens = (CZ_Token*) calloc(lexer->n_tokens_capacity, sizeof(CZ_Token));
    if (lexer->tokens == NULL) {
        cz_lexer_free(lexer);
        return NULL;
    }

    // Code allocation
    lexer->code = (char*) calloc(code_length + 1, sizeof(char));
    if (lexer->code == NULL) {
        cz_lexer_free(lexer);
        return NULL;
    }
    // Copy code
    strncpy(lexer->code, code, code_length);

    // Add error list
    lexer->error_list = cz_error_list_create();
    if (lexer->error_list == NULL) {
        cz_lexer_free(lexer);
        return NULL;
    }

    lexer->sp = cz_string_pool_create();
    if (lexer->sp == NULL) {
        cz_lexer_free(lexer);
        return NULL;
    }

    lexer->filename = filename;
    lexer->row = 1;
    lexer->col = 1;
    lexer->code_length = code_length;

    return lexer;
}

/**
 * @brief Pushes token to CZ_Lexer.
 * 
 * @param lexer Pointer to CZ_Lexer struct.
 * @param token CZ_Token struct to append. (Pass by Value)
 * @return int 1 on success, 0 on failure.
 * 
 * Note that token array is automatically reallocated.
 */
static int cz_lexer_push_token(CZ_Lexer* lexer, CZ_Token token) {
    if (lexer == NULL) {
        return 0;
    }

    if (lexer->n_tokens_capacity < lexer->n_tokens) {
        // Already an invalid token array.
        return 0;
    }

    // Check capacity, increase if needed.
    if (lexer->n_tokens_capacity == lexer->n_tokens) {
        // Realloc
        CZ_Token* new_tokens_array = (CZ_Token*) realloc(lexer->tokens, sizeof(CZ_Token) * lexer->n_tokens_capacity * 2);
        if (new_tokens_array == NULL) {
            // Keep the tokens array intact.
            return 0;
        }
        lexer->tokens = new_tokens_array;
        lexer->n_tokens_capacity *= 2;
    }

    // Push
    lexer->tokens[lexer->n_tokens] = token;
    lexer->n_tokens++;

    return 1;
}

/* Helper Functions */
/**
 * @brief Checks if lexer index points to the end of code.
 * 
 * @param lexer Pointer to CZ_Lexer struct.
 * @return true Is at end.
 * @return false Is not at end.
 */
static inline bool is_at_end(CZ_Lexer* lexer) { return lexer->idx >= lexer->code_length; }
/**
 * @brief Advance lexer index by n, ignoring if going past the end.
 * 
 * @param lexer Pointer to CZ_Lexer struct.
 * @param n Bytes to advance.
 */
static inline void advance(CZ_Lexer* lexer, unsigned int n) {
    if (is_at_end(lexer)) return;

    for (unsigned int i = 0; i < n; i++) {
        const char c = lexer->code[lexer->idx];
        lexer->idx++;
        if (lexer->idx >= lexer->code_length) break;
        if (c == '\n') {
            lexer->row++;
            lexer->col = 1;
        } else {
            lexer->col ++;
        }
    }
}
/**
 * @brief Get the byte n bytes ahead.
 * 
 * @param lexer Pointer to CZ_Lexer struct.
 * @param n Bytes to look ahead.
 * @return char Character n bytes ahead. '\0' if unavailable.
 */
static inline char peek(CZ_Lexer* lexer, unsigned int n) { return lexer->idx + n >= lexer->code_length ? '\0' : lexer->code[lexer->idx+n]; }
/**
 * @brief A wrapper for cz_lexer_push_token where CZ_Token is inserted inline.
 * 
 * @param lexer Pointer to CZ_Lexer struct.
 * @param token_type Type of token to push.
 * @param length Length of the token string.
 * @return int 1 if successful, 0 if failure.
 */
static inline int cz_lexer_push_token_helper(CZ_Lexer* lexer, CZ_TokenType token_type, size_t length) {
    const char* lexeme = cz_string_pool_push(lexer->sp, lexer->code + lexer->idx, length);
    if (lexeme == NULL) return 0;

    return cz_lexer_push_token(lexer, (CZ_Token){
                                        .token_type = token_type,
                                        .lexeme = lexeme,
                                        .line = lexer->row,
                                        .column = lexer->col
                                    });
}
/**
 * @brief Checks if the string at current index is a keyword given.
 * 
 * @param lexer Pointer to CZ_Lexer struct.
 * @param keyword Keyword to match
 * @return true Keyword matches.
 * @return false Keyword does not match.
 */
static bool cz_lexer_exactly_match_keyword(CZ_Lexer* lexer, const char* keyword) {
    const size_t keyword_length = strlen(keyword);
    // Overflow
    if (lexer->idx + keyword_length > lexer->code_length) {
        return false;
    }

    // For each character, check.
    for (size_t i = 0; i < keyword_length; i++) {
        if (lexer->code[lexer->idx + i] != keyword[i]) return false;
    }
    // Check for alphanumeric character. If there is one, this is not a keyword.
    if (lexer->idx + keyword_length < lexer->code_length) {
        if (isalnum(lexer->code[lexer->idx + keyword_length]) || lexer->code[lexer->idx + keyword_length] == '_') {
            return false;
        }
    }

    return true;
}

int cz_lexer_analyze(CZ_Lexer* lexer) {
    if (lexer == NULL) {
        return 0;
    }
    if (lexer->n_tokens >= lexer->n_tokens_capacity) {
        return 0;
    }

    while (!is_at_end(lexer)) {
        const char c = lexer->code[lexer->idx];
        
        // If whitespace, advance.
        if (isspace(c)) {
            advance(lexer, 1);
            continue;
        }

        // Single character
        switch (c) {
            case '{':
                if (cz_lexer_push_token_helper(lexer, CZ_TT_LEFT_CURLY_BRACKET, 1) != 1) {
                    return 0;
                }
                advance(lexer, 1);
                break;
            case '}':
                if (cz_lexer_push_token_helper(lexer, CZ_TT_RIGHT_CURLY_BRACKET, 1) != 1) {
                    return 0;
                }
                advance(lexer, 1);
                break;
            case '(':
                if (cz_lexer_push_token_helper(lexer, CZ_TT_LEFT_PARENTHESIS, 1) != 1) {
                    return 0;
                }
                advance(lexer, 1);
                break;
            case ')':
                if (cz_lexer_push_token_helper(lexer, CZ_TT_RIGHT_PARENTHESIS, 1) != 1) {
                    return 0;
                }
                advance(lexer, 1);
                break;
            case '[':
                if (cz_lexer_push_token_helper(lexer, CZ_TT_LEFT_SQUARE_BRACKET, 1) != 1) {
                    return 0;
                }
                advance(lexer, 1);
                break;
            case ']':
                if (cz_lexer_push_token_helper(lexer, CZ_TT_RIGHT_SQUARE_BRACKET, 1) != 1) {
                    return 0;
                }
                advance(lexer, 1);
                break;
            case '=':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_EQUAL_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_EQUAL, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '+':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_PLUS_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_PLUS, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '-':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_MINUS_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else if (peeked == '>') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_RIGHT_ARROW, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_MINUS, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case ':':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == ':') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_COLON_COLON, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    }
                }
                break;
            case '*':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_STAR_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_STAR, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '/':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_SLASH_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else if (peeked == '/') {
                        // Comment
                        do { advance(lexer, 1); } while (lexer->code[lexer->idx] != '\n' && !is_at_end(lexer));
                        advance(lexer, 1);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_SLASH, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '%':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_PERCENT_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_PERCENT, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '&':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_AMPERSAND_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else if (peeked == '&') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_AMPERSAND_AMPERSAND, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_AMPERSAND, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '|':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_BAR_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else if (peeked == '|') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_BAR_BAR, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_BAR, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '^':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_CAROT_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_CAROT, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case ';':
                if (cz_lexer_push_token_helper(lexer, CZ_TT_SEMICOLON, 1) != 1) {
                    return 0;
                }
                advance(lexer, 1);
                break;
            case '.':
                if (cz_lexer_push_token_helper(lexer, CZ_TT_PERIOD, 1) != 1) {
                    return 0;
                }
                advance(lexer, 1);
                break;
            case ',':
                if (cz_lexer_push_token_helper(lexer, CZ_TT_COMMA, 1) != 1) {
                    return 0;
                }
                advance(lexer, 1);
                break;
            case '!':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_EXCLAMATION_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_EXCLAMATION, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '>':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_GREATER_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_GREATER, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '<':
                {
                    const char peeked = peek(lexer, 1);
                    if (peeked == '=') {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_LESS_EQUAL, 2) != 1) {
                            return 0;
                        }
                        advance(lexer, 2);
                    } else {
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_LESS, 1) != 1) {
                            return 0;
                        }
                        advance(lexer, 1);
                    }
                }
                break;
            case '\"':
                {
                    // TODO: String literal
                    return 0;
                }
                break;
            default:
                // Keywords
                if (cz_lexer_exactly_match_keyword(lexer, "for")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_FOR, strlen("for")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("for"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "while")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_WHILE, strlen("while")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("while"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "if")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_IF, strlen("if")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("if"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "else")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_ELSE, strlen("else")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("else"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "return")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_RETURN, strlen("return")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("return"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "func")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_FUNC, strlen("func")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("func"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "const")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_CONST, strlen("const")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("const"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "bool")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_BOOL, strlen("bool")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("bool"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "int32")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_INT32, strlen("int32")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("int32"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "float")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_FLOAT, strlen("float")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("float"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "true")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_TRUE, strlen("true")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("true"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "struct")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_STRUCT, strlen("struct")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("struct"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "false")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_FALSE, strlen("false")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("false"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "typedef")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_TYPEDEF, strlen("typedef")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("typedef"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "newtype")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_NEWTYPE, strlen("newtype")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("newtype"));
                } else if (cz_lexer_exactly_match_keyword(lexer, "as")) {
                    if (cz_lexer_push_token_helper(lexer, CZ_TT_AS, strlen("as")) != 1) {
                        return 0;
                    }
                    advance(lexer, strlen("as"));
                } else {
                    if (isalpha(c)) {
                        // Keyword.
                        size_t looper = 1;
                        while (isalnum(peek(lexer, looper)) || peek(lexer, looper) == '_') {
                            looper++;
                        }

                        if (cz_lexer_push_token_helper(lexer, CZ_TT_IDENTIFIER, looper) != 1) {
                            return 0;
                        }
                        advance(lexer, looper);
                    } else if (isdigit(c)) {
                        // Numerical Literal
                        size_t looper = 1;
                        while (isdigit(peek(lexer, looper))) {
                            looper++;
                        }
                        if (peek(lexer, looper) == '.') {
                            // Decimal point
                            looper++;
                            while (isdigit(peek(lexer, looper))) {
                                looper++;
                            }
                        }

                        if (cz_lexer_push_token_helper(lexer, CZ_TT_NUMERICAL_LITERAL, looper) != 1) {
                            return 0;
                        }
                        advance(lexer, looper);
                    } else {
                        // Invalid token: Add as unknown, then skip to the end of line.
                        size_t looper = 1;
                        while (lexer->idx + looper < lexer->code_length) {
                            if (peek(lexer, looper) == '\n') {
                                break;
                            }
                            looper++;
                        }
                        if (cz_lexer_push_token_helper(lexer, CZ_TT_UNKNOWN, looper) != 1) {
                            return 0;
                        }
                        if (cz_error_list_push_error(lexer->error_list, lexer->filename, lexer->row, lexer->col, "Unknown token") != 1) {
                            return 0;
                        }
                        advance(lexer, looper);
                    }
                }


                break;
        }
    }

    if (cz_lexer_push_token(lexer, (CZ_Token){
            .token_type = CZ_TT_EOF,
            .lexeme = NULL,
            .line = lexer->row,
            .column = lexer->col
        }) != 1) {
        return 0;
    }

    return 1;
}

void cz_lexer_free(CZ_Lexer* lexer) {
    if (lexer != NULL) {
        free(lexer->tokens);
        free(lexer->code);
        cz_error_list_free(lexer->error_list);
        free(lexer);
    }
}
