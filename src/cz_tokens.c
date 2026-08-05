#include <stdlib.h>
#include <stdbool.h>
#include "cz_tokens.h"

bool cz_token_type_is_assignment(CZ_TokenType token_type) {
    switch (token_type) {
        case CZ_TT_EQUAL:
        case CZ_TT_PLUS_EQUAL:
        case CZ_TT_MINUS_EQUAL:
        case CZ_TT_STAR_EQUAL:
        case CZ_TT_SLASH_EQUAL:
        case CZ_TT_PERCENT_EQUAL:
        case CZ_TT_AMPERSAND_EQUAL:
        case CZ_TT_BAR_EQUAL:
        case CZ_TT_CARET_EQUAL:
            return true;
        default:
            return false;
    }
}

const char* cz_token_to_human_name(CZ_TokenType token_type) {
    switch (token_type) {

        case CZ_TT_LEFT_CURLY_BRACKET:
            return "{";
        case CZ_TT_RIGHT_CURLY_BRACKET:
            return "}";
        case CZ_TT_LEFT_PARENTHESIS:
            return "(";
        case CZ_TT_RIGHT_PARENTHESIS:
            return ")";
        case CZ_TT_LEFT_SQUARE_BRACKET:
            return "[";
        case CZ_TT_RIGHT_SQUARE_BRACKET:
            return "]";
        case CZ_TT_EQUAL:
            return "=";
        case CZ_TT_PLUS:
            return "+";
        case CZ_TT_MINUS:
            return "-";
        case CZ_TT_STAR:
            return "*";
        case CZ_TT_SLASH:
            return "/";
        case CZ_TT_PERCENT:
            return "%%";
        case CZ_TT_AMPERSAND:
            return "&";
        case CZ_TT_BAR:
            return "|";
        case CZ_TT_CARET:
            return "^";
        case CZ_TT_SEMICOLON:
            return ";";
        case CZ_TT_PERIOD:
            return ".";
        case CZ_TT_COMMA:
            return ",";
        case CZ_TT_EXCLAMATION:
            return "!";
        case CZ_TT_GREATER:
            return ">";
        case CZ_TT_LESS:
            return "<";

        // Double-letter
        case CZ_TT_PLUS_EQUAL:
            return "+=";
        case CZ_TT_MINUS_EQUAL:
            return "-=";
        case CZ_TT_STAR_EQUAL:
            return "*=";
        case CZ_TT_SLASH_EQUAL:
            return "/=";
        case CZ_TT_PERCENT_EQUAL:
            return "%%=";
        case CZ_TT_AMPERSAND_EQUAL:
            return "&=";
        case CZ_TT_BAR_EQUAL:
            return "|=";
        case CZ_TT_CARET_EQUAL:
            return "^=";
        case CZ_TT_EQUAL_EQUAL:
            return "==";
        case CZ_TT_EXCLAMATION_EQUAL:
            return "!=";
        case CZ_TT_GREATER_EQUAL:
            return ">=";
        case CZ_TT_LESS_EQUAL:
            return "<=";
        case CZ_TT_RIGHT_ARROW:
            return "->";
        case CZ_TT_COLON_COLON:
            return "::";
        case CZ_TT_AMPERSAND_AMPERSAND:
            return "&&";
        case CZ_TT_BAR_BAR:
            return "||";

        // Others
        //case CZ_TT_IN:
        //    break;
        case CZ_TT_FOR:
            return "for";
        case CZ_TT_WHILE:
            return "while";
        case CZ_TT_IF:
            return "if";
        case CZ_TT_ELSE:
            return "else";
        case CZ_TT_RETURN:
            return "return";
        case CZ_TT_FUNC:
            return "func";
        case CZ_TT_CONST:
            return "const";
        case CZ_TT_BOOL:
            return "bool";
        case CZ_TT_INT32:
            return "int32";
        case CZ_TT_UINT32:
            return "uint32";
        //case CZ_TT_UINT:
        //    break;
        case CZ_TT_FLOAT:
            break;
        //case CZ_TT_STRING:
        //    break;
        //case CZ_TT_MODULE:
        //    break;
        case CZ_TT_STRUCT:
            return "struct";
        case CZ_TT_TYPEDEF:
            return "typedef";
        case CZ_TT_NEWTYPE:
            return "newtype";
        case CZ_TT_AS:
            return "as";
        case CZ_TT_TRUE:
            return "true";
        case CZ_TT_FALSE:
            return "false";
        case CZ_TT_NUMERICAL_LITERAL:
            return "numeral";
        case CZ_TT_STRING_LITERAL:
            return "string";
        case CZ_TT_IDENTIFIER:
            return "identifier";
        case CZ_TT_UNKNOWN:
            return "unknown";
        case CZ_TT_EOF:
            return "EOF";
        default:
            return NULL;
    }
    return NULL;
}
