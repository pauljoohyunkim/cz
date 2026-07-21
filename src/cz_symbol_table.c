#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cz_symbol_table.h"
#include "cz_type.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)

CZ_Symbol* cz_symbol_create(CZ_SymbolKind kind, const char* name, unsigned int scope_level) {
    CZ_Symbol* symbol = NULL;
    NULL_POINTER_TO_GOTO(name, error_cleanup);

    symbol = (CZ_Symbol*) calloc(1, sizeof(CZ_Symbol));
    NULL_POINTER_TO_GOTO(symbol, error_cleanup);
    
    symbol->kind = kind;
    symbol->name = name;
    symbol->scope_level = scope_level;

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

const CZ_Symbol* cz_environment_lookup(const CZ_Environment* env, const char* name, bool cascade) {
    if (env == NULL || name == NULL) return NULL;

    for (unsigned i = 0; i < env->symbol_count; i++) {
        if (env->symbols[i]->name == name || strcmp(env->symbols[i]->name, name) == 0) {
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

void cz_symbol_print(const CZ_Symbol* symbol, unsigned int depth) {
    if (symbol == NULL) {
        for (unsigned int i = 0; i < depth; i++) {
            printf("\t");
        }
        printf("NULL symbol\n");
        return;
    }

    for (unsigned int i = 0; i < depth; i++) {
        printf("\t");
    }

    printf("Symbol: %s", symbol->name);

    // Print the kind
    printf(" (%s)", symbol->kind == CZ_SYMBOL_KIND_VALUE ? "Value" : "Type");

    // Now, if the symbol has a type, print it
    const CZ_Type* type = NULL;
    if (symbol->kind == CZ_SYMBOL_KIND_VALUE) {
        type = symbol->data.value.type;
    } else if (symbol->kind == CZ_SYMBOL_KIND_TYPE) {
        type = symbol->data.type_decl.type;
    }

    if (type != NULL) {
        printf(" : ");
        // Now print the type
        switch (type->kind) {
            case CZ_TYPE_KIND_PRIMITIVE:
                switch (type->primitive) {
                    case CZ_PRIMITIVE_VOID: printf("void"); break;
                    case CZ_PRIMITIVE_INT32: printf("i32"); break;
                    case CZ_PRIMITIVE_BOOL: printf("bool"); break;
                    case CZ_PRIMITIVE_FLOAT: printf("float"); break;
                }
                break;
            case CZ_TYPE_KIND_STRUCT:
                printf("struct %s", type->structure.name);
                break;
            case CZ_TYPE_KIND_CONST:
                printf("const ");
                break;
            case CZ_TYPE_KIND_REFERENCE:
                printf("ref");
                break;
            case CZ_TYPE_KIND_NEWTYPE:
                printf("newtype %s", type->newtype.name);
                break;
            case CZ_TYPE_KIND_FUNCTION:
                printf("function");
                break;
            default:
                printf("unknown");
                break;
        }
    }
    printf("\n");
}

void cz_environment_print(const CZ_Environment* env, unsigned int depth, bool cascade) {
    if (env == NULL) {
        for (unsigned int i = 0; i < depth; i++) {
            printf("\t");
        }
        printf("NULL environment\n");
        return;
    }

    for (unsigned int i = 0; i < depth; i++) {
        printf("\t");
    }
    printf("Environment (scope_level=%u):\n", env->scope_level);

    if (cascade && env->parent != NULL) {
        for (unsigned int i = 0; i < depth + 1; i++) {
            printf("\t");
        }
        printf("Parent: %p (scope_level=%u)\n", (void*)env->parent, env->parent->scope_level);
    }

    for (unsigned int i = 0; i < depth + 1; i++) {
        printf("\t");
    }
    printf("Symbols (%u/%u):\n", env->symbol_count, env->symbol_capacity);

    for (unsigned int i = 0; i < env->symbol_count; i++) {
        cz_symbol_print(env->symbols[i], depth + 2);
    }
}
