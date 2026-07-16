#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cz_ast.h"
#include "cz_symbol_table.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)

CZ_Symbol* cz_symbol_create(CZ_SymbolKind kind, const char* name) {
    CZ_Symbol* symbol = NULL;
    const char* name_cpy = NULL;
    NULL_POINTER_TO_GOTO(name, error_cleanup);

    symbol = (CZ_Symbol*) calloc(1, sizeof(CZ_Symbol));
    NULL_POINTER_TO_GOTO(symbol, error_cleanup);
    
    name_cpy = strdup(name);
    NULL_POINTER_TO_GOTO(name_cpy, error_cleanup);

    symbol->kind = kind;
    symbol->name = name_cpy;

    return symbol;

error_cleanup:
    free(symbol);
    free((void*)name_cpy);
    return NULL;
}

void cz_symbol_free(CZ_Symbol* symbol) {
    if (symbol != NULL) {
        free((void*)symbol->name);

        switch (symbol->kind) {
            case CZ_SYMBOL_KIND_VARIABLE:
                // Do nothing
                break;
            case CZ_SYMBOL_KIND_FUNCTION:
                if (symbol->data.function.params != NULL) {
                    for (unsigned int i = 0; i < symbol->data.function.param_count; i++) {
                        free((void*)symbol->data.function.params[i].name);
                        symbol->data.function.params[i].name = NULL;
                    }
                    free((void*)symbol->data.function.params);
                    symbol->data.function.params = NULL;
                }
                break;
            case CZ_SYMBOL_KIND_TYPE:
                // Do nothing
                break;
        }
    }
    free(symbol);
}
