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
    unsigned int symbol_capacity;

    unsigned int scope_level;
};

/**
 * @brief Create CZ_Symbol
 * 
 * @param kind Symbol kind
 * @param name Name of the symbol. Will internally copy.
 * @return CZ_Symbol* Allocated CZ_Symbol on success, NULL on failure.
 */
CZ_Symbol* cz_symbol_create(CZ_SymbolKind kind, const char* name);

/**
 * @brief Frees allocated CZ_Symbol
 * 
 * @param symbol Pointer to CZ_Symbol
 */
void cz_symbol_free(CZ_Symbol* symbol);

/**
 * @brief Create CZ_Environment
 * 
 * @return CZ_Environment* Pointer to CZ_Environment on success, NULL on failure.
 */
CZ_Environment* cz_environment_create(void);

/**
 * @brief Frees environment
 * 
 * @param env Pointer to CZ_Environment
 */
void cz_environment_free(CZ_Environment* env);

/**
 * @brief Pushes symbol to environment.
 * 
 * @param env Pointer to CZ_Environment
 * @param symbol Pointer to CZ_Symbol. (Do not free, as ownership is being transferred)
 * @return int 1 on success, 0 on failure.
 */
int cz_environment_push_symbol(CZ_Environment* env, const CZ_Symbol* symbol);

#ifdef __cplusplus
}
#endif

#endif  /* CZ_SYMBOL_TABLE_H */
