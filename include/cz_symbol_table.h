#ifndef CZ_SYMBOL_TABLE_H
#define CZ_SYMBOL_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdlib.h>
#include "cz_type.h"

typedef struct CZ_Environment CZ_Environment;

typedef enum {
    CZ_SYMBOL_KIND_VARIABLE,
    CZ_SYMBOL_KIND_FUNCTION,
    CZ_SYMBOL_KIND_TYPE             // Struct, Typedef, Newtype
} CZ_SymbolKind;

typedef struct {
    const CZ_Type* type;            // Parameter type
    const char* name;               // Parameter name
} CZ_ParamSymbol;

typedef struct {
    CZ_SymbolKind kind;
    const char* name;

    union {
        struct {
            const CZ_Type* type;
        } variable;

        struct {
            const CZ_Type* return_type;
            CZ_ParamSymbol* params;
            unsigned int param_count;
        } function;

        struct {
            const CZ_Type* type;
        } type_decl;
    } data;
} CZ_Symbol;

struct CZ_Environment {
    CZ_Environment* parent;         // NULL for global.

    CZ_Symbol** symbols;
    unsigned int symbol_count;

    unsigned int scope_level;
};

#ifdef __cplusplus
}
#endif

#endif  /* CZ_SYMBOL_TABLE_H */
