#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "cz_error.h"

CZ_ErrorList* cz_error_list_create(void) {
    CZ_ErrorList* error_list = (CZ_ErrorList*) calloc(1, sizeof(CZ_ErrorList));
    if (error_list == NULL) {
        return NULL;
    }

    error_list->error_capacity = 8;
    error_list->errors = (CZ_Error*) calloc(error_list->error_capacity, sizeof(CZ_Error));
    if (error_list->errors == NULL) {
        cz_error_list_free(error_list);
        return NULL;
    }
    error_list->n_errors = 0;

    return error_list;
}

int cz_error_list_push_error(CZ_ErrorList* error_list, const char* filename, int line, int column, const char* format, ...) {
    if (error_list == NULL) {
        return 0;
    }

    // Already overflowing.
    if (error_list->n_errors > error_list->error_capacity) {
        return 0;
    }

    va_list args;
    va_start(args, format);

    char buffer[BUFSIZ] = { 0 };
    int msg_len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (msg_len < 0) {
        fprintf(stderr, "[-] Internal error: logging failure (vsnprintf failure)\n");
        return 0;
    }
    char* msg = (char*) calloc(msg_len+1, sizeof(char));
    if (msg == NULL) {
        fprintf(stderr, "[-] Internal error: logging failure (msg allocation failure)\n");
        return 0;
    }
    memcpy(msg, buffer, msg_len+1);

    // Increase capacity if needed.
    if (error_list->n_errors == error_list->error_capacity) {
        // Realloc
        CZ_Error* new_errors = (CZ_Error*) realloc(error_list->errors, sizeof(CZ_Error) * 2 * error_list->error_capacity);
        if (new_errors == NULL) {
            return 0;
        }
        error_list->errors = new_errors;
        error_list->error_capacity *= 2;
    }

    CZ_Error error = {
        .column = column,
        .line = line,
        .filename = filename,
        .message = msg
    };
    msg = NULL;

    error_list->errors[error_list->n_errors] = error;
    error_list->n_errors++;

    return 1;
}

void cz_error_list_free(CZ_ErrorList* error_list) {
    if (error_list != NULL) {
        for (unsigned int i = 0; i < error_list->n_errors; i++) {
            free(error_list->errors[i].message);
        }
        free(error_list->errors);
    }
    free(error_list);
}
