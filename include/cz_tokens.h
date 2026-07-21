#ifndef CZ_TOKENS_H
#define CZ_TOKENS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef enum {
    // Single-letter
    CZ_TT_LEFT_CURLY_BRACKET,
    CZ_TT_RIGHT_CURLY_BRACKET,
    CZ_TT_LEFT_PARENTHESIS,
    CZ_TT_RIGHT_PARENTHESIS,
    CZ_TT_LEFT_SQUARE_BRACKET,
    CZ_TT_RIGHT_SQUARE_BRACKET,
    CZ_TT_EQUAL,
    CZ_TT_PLUS,
    CZ_TT_MINUS,
    CZ_TT_STAR,
    CZ_TT_SLASH,
    CZ_TT_PERCENT,
    CZ_TT_AMPERSAND,
    CZ_TT_BAR,
    CZ_TT_CARET,
    CZ_TT_SEMICOLON,
    CZ_TT_PERIOD,
    CZ_TT_COMMA,
    CZ_TT_EXCLAMATION,
    CZ_TT_GREATER,
    CZ_TT_LESS,

    // Double-letter
    CZ_TT_PLUS_EQUAL,
    CZ_TT_MINUS_EQUAL,
    CZ_TT_STAR_EQUAL,
    CZ_TT_SLASH_EQUAL,
    CZ_TT_PERCENT_EQUAL,
    CZ_TT_AMPERSAND_EQUAL,
    CZ_TT_BAR_EQUAL,
    CZ_TT_CARET_EQUAL,
    CZ_TT_EQUAL_EQUAL,
    CZ_TT_EXCLAMATION_EQUAL,
    CZ_TT_GREATER_EQUAL,
    CZ_TT_LESS_EQUAL,
    CZ_TT_RIGHT_ARROW,
    CZ_TT_COLON_COLON,
    CZ_TT_AMPERSAND_AMPERSAND,
    CZ_TT_BAR_BAR,

    // Others
    //CZ_TT_IN,
    CZ_TT_FOR,
    CZ_TT_WHILE,
    CZ_TT_IF,
    CZ_TT_ELSE,
    CZ_TT_RETURN,
    CZ_TT_FUNC,
    CZ_TT_CONST,
    CZ_TT_BOOL,
    CZ_TT_INT32,
    //CZ_TT_UINT,
    CZ_TT_FLOAT,
    //CZ_TT_STRING,
    //CZ_TT_MODULE,
    CZ_TT_STRUCT,
    CZ_TT_TYPEDEF,
    CZ_TT_NEWTYPE,
    CZ_TT_AS,
    CZ_TT_TRUE,
    CZ_TT_FALSE,
    CZ_TT_NUMERICAL_LITERAL,
    CZ_TT_STRING_LITERAL,
    CZ_TT_IDENTIFIER,
    CZ_TT_UNKNOWN,

    CZ_TT_EOF
} CZ_TokenType;

typedef struct {
    CZ_TokenType token_type;
    const char* lexeme;
    unsigned int line;
    unsigned int column;
} CZ_Token;

/**
 * @brief Checks if token type is an assignment operator.
 * 
 * @param token_type Token type
 * @return true if assignment operator (=, +=, -=, etc.)
 * @return false otherwise.
 */
bool cz_token_type_is_assignment(CZ_TokenType token_type);

/**
 * @brief Translates token type to human-understandable text.
 * 
 * @param token_type Token type
 * @return const char* Read-only string literal if successful (Do not free.) or NULL if failure.
 */
const char* cz_token_to_human_name(CZ_TokenType token_type);

#ifdef __cplusplus
}
#endif

#endif  /* CZ_TOKENS_H */
