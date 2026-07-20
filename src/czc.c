#include <stdio.h>
#include <stdlib.h>
#include "cz_lexer.h"
#include "cz_parser.h"
#include "cz_semantic_analyzer.h"
#include "cz_code_generator.h"

static long get_file_size(const char *filename) {
    // Open in binary mode ("rb") to get an exact byte count
    FILE *fp = fopen(filename, "rb"); 
    if (fp == NULL) {
        return -1; 
    }

    // Move the file pointer to the end of the file
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    // Get the current position of the file pointer (file size in bytes)
    long size = ftell(fp); 

    fclose(fp);
    return size;
}

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        printf("Usage: czc src.c output.o\n");
        return 1;
    }

    const char* source_filename = argv[1];
    const size_t filesize = get_file_size(source_filename);
    CZ_Lexer* lexer = NULL;
    CZ_Parser* parser = NULL;
    CZ_SemanticAnalyzer* sa = NULL;
    CZ_CodeGenerator* cg = NULL;
    bool generate_code = true;

    FILE* fp = fopen(source_filename, "r");
    if (fp == NULL) {
        printf("Could not open %s\n", source_filename);
        return 1;
    }

    char* code = (char*)calloc(filesize + 1, sizeof(char));
    if (code == NULL) {
        printf("Could not allocate memory for code.\n");
        fclose(fp);
    }

    fread(code, sizeof(char), filesize, fp);
    fclose(fp);

    printf("%s\n", code);
    lexer = cz_lexer_create(code, source_filename);
    free(code);

    // Lexer
    int ret = cz_lexer_analyze(lexer);
    if (ret != 1) {
        printf("Failure lexing\n");
        goto error_cleanup_exit;
    }

    if (lexer->error_list->n_errors > 0) {
        for (unsigned int i = 0; i < lexer->error_list->n_errors; i++) {
            printf("Lexer: line %d col %d: %s\n",
                lexer->error_list->errors[i].line,
                lexer->error_list->errors[i].column,
                lexer->error_list->errors[i].message);
        }
        generate_code = false;
    }

    // Parser
    parser = cz_parser_create(lexer);
    if (parser == NULL) {
        printf("Parser creation failure\n");
        goto error_cleanup_exit;
    }

    ret = cz_parser_parse(parser);
    if (ret != 1) {
        printf("Failure parsing\n");
        goto error_cleanup_exit;
    }

    if (parser->error_list->n_errors > 0) {
        for (unsigned int i = 0; i < parser->error_list->n_errors; i++) {
            printf("Parser: line %d col %d: %s\n",
                parser->error_list->errors[i].line,
                parser->error_list->errors[i].column,
                parser->error_list->errors[i].message);
        }
        generate_code = false;
    }

    cz_ast_root_print(parser->program, 0);

    // Semantic Analyzer
    sa = cz_semantic_analyzer_create(parser);
    if (sa == NULL) {
        printf("Semantic Analyzer creation failure\n");
        goto error_cleanup_exit;
    }
    ret = cz_semantic_analyzer_analyze(sa);
    if (ret != 1) {
        printf("Failure semantic analysis\n");
        goto error_cleanup_exit;
    }

    printf("--- Global Environment ---\n");
    cz_environment_print(sa->global_env, 0, false);
    // Print function body environments
    if (sa->program && sa->program->program.global_declaration_list) {
        for (unsigned int i = 0; i < sa->program->program.declaration_count; i++) {
            const CZ_AST_Node* decl = sa->program->program.global_declaration_list[i];
            if (decl && decl->node_type == CZ_AST_FunctionDeclarationNodeType) {
                const char* func_name = decl->function_declaration.function_identifier->identifier.name;
                if (decl->function_declaration.body &&
                    decl->function_declaration.body->statement_list.scope) {
                    printf("\n--- Function '%s' Body Environment (scope level %u) ---\n",
                           func_name,
                           decl->function_declaration.body->statement_list.scope->scope_level);
                    cz_environment_print(decl->function_declaration.body->statement_list.scope, 0, false);
                }
            }
        }
    }
    printf("\n--- Global Type Table ---\n");
    cz_global_type_table_print(sa->gtt);

    if (sa->error_list->n_errors > 0) {
        for (unsigned int i = 0; i < sa->error_list->n_errors; i++) {
            printf("Semantic Analyzer: line %d col %d: %s\n",
                sa->error_list->errors[i].line,
                sa->error_list->errors[i].column,
                sa->error_list->errors[i].message);
        }
        generate_code = false;
    }

    if (!generate_code) {
        printf("Skipping code generation\n");
        goto error_cleanup_exit;
    }

    cg = cz_code_generator_create(sa);
    if (cg == NULL) {
        printf("Code generator creation failure\n");
        goto error_cleanup_exit;
    }

    ret = cz_code_generator_generate(cg);
    if (ret != 1) {
        printf("Failure code generator.\n");
        goto error_cleanup_exit;
    }

    if (cg->error_list->n_errors > 0) {
        for (unsigned int i = 0; i < cg->error_list->n_errors; i++) {
            printf("Code Generator: line %d col %d: %s\n",
                cg->error_list->errors[i].line,
                cg->error_list->errors[i].column,
                cg->error_list->errors[i].message);
        }
    }

    LLVMDumpModule(cg->mod);

    if (argc == 3) {
        const char* output_filename = argv[2];
        cz_code_generator_emit_object_file(cg->mod, output_filename);
    }


    cz_code_generator_free(cg);
    cz_semantic_analyzer_free(sa);
    cz_parser_free(parser);
    cz_lexer_free(lexer);

    return 0;

error_cleanup_exit:
    cz_code_generator_free(cg);
    cz_semantic_analyzer_free(sa);
    cz_parser_free(parser);
    cz_lexer_free(lexer);
    return 1;
}
