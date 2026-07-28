#ifndef CZ_ERR_H
#define CZ_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct {
    char* message;
    const char* filename;
    int line;
    int column;
} CZ_Error;

typedef struct {
    CZ_Error* errors;
    size_t n_errors;
    size_t error_capacity;
} CZ_ErrorList;

/**
 * @brief Create error list.
 * 
 * @return CZ_ErrorList* 
 */
CZ_ErrorList* cz_error_list_create(void);

/**
 * @brief Pushes the error to error list, expanding storage as needed.
 * 
 * @param error_list Pointer to CZ_ErrorList struct.
 * @param error CZ_Error struct
 * @return int 1 if successful. 0 if unsuccessful.
 */
int cz_error_list_push_error(CZ_ErrorList* error_list, const char* filename, int line, int column, const char* format, ...);

/**
 * @brief Frees error list and its components.
 * 
 * @param error_list Pointer to CZ_ErrorList struct.
 */
void cz_error_list_free(CZ_ErrorList* error_list);

#ifdef __cplusplus
}
#endif

#endif  /* CZ_ERR_H */
