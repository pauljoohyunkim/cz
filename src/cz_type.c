#include <stdlib.h>
#include "cz_type.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)

CZ_Type* cz_type_create(CZ_PrimitiveType primitive, bool is_const) {
    CZ_Type* type = NULL;
    
    type = (CZ_Type*) calloc(1, sizeof(CZ_Type));
    NULL_POINTER_TO_GOTO(type, error_cleanup);

    type->primitive = primitive;
    type->is_const = is_const;

    return type;

error_cleanup:
    free(type);
    return NULL;
}

static void cz_struct_layout_free(CZ_StructLayout* layout) {
    if (layout != NULL) {
        for (unsigned int i = 0; i < layout->field_count; i++) {
            free((void*)layout->fields[i].name);
            layout->fields[i].name = NULL;

            // Type is owned by type table.
            layout->fields[i].type = NULL;
        }
        free(layout->fields);
        layout->fields = NULL;
    }
    free(layout);
}

void cz_type_free(CZ_Type* type) {
    if (type != NULL) {
        switch (type->kind) {
            case CZ_TYPE_KIND_PRIMITIVE:
                // Nothing to do
                break;
            case CZ_TYPE_KIND_STRUCT:
                free((void*)type->structure.name);
                type->structure.name = NULL;
                cz_struct_layout_free((void*)type->structure.layout);
                type->structure.layout = NULL;
                break;
            case CZ_TYPE_KIND_REFERENCE:
                // Nothing to do
                break;
            case CZ_TYPE_KIND_NEWTYPE:
                free((void*)type->newtype.name);
                type->newtype.name = NULL;
                break;
            case CZ_TYPE_KIND_FUNCTION:
                free(type->function.param_types);
                type->function.param_types = NULL;
                break;
        }
    }
    free(type);
}
