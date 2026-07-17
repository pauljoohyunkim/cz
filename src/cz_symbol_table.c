#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cz_ast.h"
#include "cz_symbol_table.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)

CZ_Symbol* cz_symbol_create(CZ_SymbolKind kind, const char* name) {
    CZ_Symbol* symbol = NULL;
    NULL_POINTER_TO_GOTO(name, error_cleanup);

    symbol = (CZ_Symbol*) calloc(1, sizeof(CZ_Symbol));
    NULL_POINTER_TO_GOTO(symbol, error_cleanup);
    
    symbol->kind = kind;
    symbol->name = name;

    return symbol;

error_cleanup:
    free(symbol);
    return NULL;
}

void cz_symbol_free(CZ_Symbol* symbol) {
    if (symbol != NULL) {
        //free((void*)symbol->name);

        switch (symbol->kind) {
            case CZ_SYMBOL_KIND_VALUE:
                // Do nothing
                break;
            case CZ_SYMBOL_KIND_TYPE:
                // Do nothing
                break;
        }
    }
    free(symbol);
}

CZ_Environment* cz_environment_create(void) {
    CZ_Environment* env = NULL;
    CZ_Symbol** symbols = NULL;

    env = (CZ_Environment*) calloc(1, sizeof(CZ_Environment));
    NULL_POINTER_TO_GOTO(env, error_cleanup);

    env->symbol_capacity = 8;
    symbols = (CZ_Symbol**) calloc(env->symbol_capacity, sizeof(CZ_Symbol*));
    NULL_POINTER_TO_GOTO(symbols, error_cleanup);

    env->symbols = symbols;
    symbols = NULL;

    return env;

error_cleanup:
    free(env);
    free(symbols);
    return NULL;
}

void cz_environment_free(CZ_Environment* env) {
    if (env != NULL) {
        if (env->symbols != NULL) {
            for (unsigned int i = 0; i < env->symbol_count; i++) {
                cz_symbol_free(env->symbols[i]);
                env->symbols[i] = NULL;
            }
        }
        free(env->symbols);
    }
    free(env);
}

const CZ_Symbol* cz_environment_lookup(CZ_Environment* env, const char* name, bool cascade) {
    if (env == NULL || name == NULL) return NULL;

    for (unsigned i = 0; i < env->symbol_count; i++) {
        if (env->symbols[i] == name && strcmp(env->symbols[i]->name, name) == 0) {
            return env->symbols[i];
        }
    }

    // Since could not find in current environment, look up parent.
    if (env->parent != NULL && cascade) {
        return cz_environment_lookup(env->parent, name, cascade);
    }

    // Global scope could not find symbol.
    return NULL;
}

int cz_environment_push_symbol(CZ_Environment* env, const CZ_Symbol* symbol) {
    CZ_Symbol** new_symbols = NULL;
    NULL_POINTER_TO_GOTO(env, error_cleanup);
    NULL_POINTER_TO_GOTO(symbol, error_cleanup);

    if (env->symbol_capacity == env->symbol_count) {
        new_symbols = (CZ_Symbol**) realloc(env->symbols, sizeof(CZ_Symbol*) * env->symbol_capacity * 2);
        NULL_POINTER_TO_GOTO(new_symbols, error_cleanup);

        env->symbols = new_symbols;
        new_symbols = NULL;
        env->symbol_capacity *= 2;
    }
    env->symbols[env->symbol_count] = (CZ_Symbol*) symbol;
    env->symbol_count++;

    return 1;

error_cleanup:
    free(new_symbols);
    return 0;
}
