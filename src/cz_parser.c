#include <stdlib.h>
#include <stdbool.h>
#include "cz_parser.h"

#define NULL_POINTER_TO_GOTO(ptr, label) do { if ((ptr) == NULL) goto label; } while (0)

typedef enum {
    CZ_PRECEDENCE_NONE = 0,
    CZ_PRECEDENCE_EQUALITY,
    CZ_PRECEDENCE_RELATIONAL,
    CZ_PRECEDENCE_BITWISE_OR,
    CZ_PRECEDENCE_BITWISE_XOR,
    CZ_PRECEDENCE_BITWISE_AND,
    CZ_PRECEDENCE_ADDITIVE,
    CZ_PRECEDENCE_MULTIPLICATIVE,
    CZ_PRECEDENCE_CAST,
    CZ_PRECEDENCE_UNARY,
    CZ_PRECEDENCE_CALL_ACCESS,
    CZ_PRECEDENCE_PRIMARY
} CZ_Precedence;

static inline unsigned int cz_parser_get_line(const CZ_Parser* parser);
static inline unsigned int cz_parser_get_col(const CZ_Parser* parser);
static CZ_TokenType cz_parser_peek_token_type(const CZ_Parser* parser, unsigned int offset);
static CZ_Precedence cz_parser_get_binary_operation_precedence(CZ_TokenType token_type);
static inline bool cz_parser_is_operation_right_associative(CZ_TokenType token_type);
static int cz_parser_push_global_declaration(CZ_AST_Node* subnode, CZ_AST_Node* program_node);
static int cz_parser_push_function_param(CZ_AST_Node* parameter_list, CZ_AST_Node* param);
static int cz_parser_push_statement_to_block(CZ_AST_Node* block, CZ_AST_Node* statement);
static int cz_parser_push_struct_member_to_members(CZ_AST_Node* members, CZ_AST_Node* member);
static int cz_parser_push_argument_to_function_call_argument_list(CZ_AST_Node* members, CZ_AST_Node* member);
static CZ_AST_Node* cz_parser_create_program_node(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_struct_decl(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_struct_init(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_struct_init_member(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_variable_decl(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_type_decl(CZ_Parser* parser, bool is_strong);
static CZ_AST_Node* cz_parser_create_assignment(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_function_decl(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_param_list(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_param_decl(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_block(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_statement(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_return_statement(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_if_statement(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_for_statement(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_while_statement(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_type(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_base_type(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_literal(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_identifier(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_expression(CZ_Parser* parser, CZ_Precedence min_binding_power);
static CZ_AST_Node* cz_parser_create_unary(CZ_Parser* parser);
static CZ_AST_Node* cz_parser_create_primary(CZ_Parser* parser);
static CZ_Token* cz_parser_consume_token(CZ_Parser* parser, CZ_TokenType expected_type);
static inline bool cz_parser_is_sync_token(CZ_TokenType token_type);
static int cz_parser_sync(CZ_Parser* parser);

CZ_Parser* cz_parser_create(CZ_Lexer* lexer) {
    if (lexer == NULL) {
        return NULL;
    }

    CZ_Parser* parser = (CZ_Parser*) calloc(1, sizeof(CZ_Parser));
    if (parser == NULL) {
        return NULL;
    }

    parser->error_list = cz_error_list_create();
    if (parser->error_list == NULL) {
        cz_parser_free(parser);
        return NULL;
    }

    // Transfer code string ownership
    parser->code = lexer->code;
    lexer->code = NULL;

    parser->code_length = lexer->code_length;

    // Transfer tokens array ownership
    parser->tokens = lexer->tokens;
    lexer->tokens = NULL;
    parser->n_tokens = lexer->n_tokens;

    parser->filename = lexer->filename;

    parser->sp = lexer->sp;
    lexer->sp = NULL;

    return parser;
}

int cz_parser_parse(CZ_Parser* parser) {
    if (parser == NULL) return 0;

    //parser->program = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
    parser->program = cz_parser_create_program_node(parser);
    if (parser->program == NULL) {
        return 0;
    }

    return 1;
}

void cz_parser_free(CZ_Parser* parser) {
    if (parser != NULL) {
        free(parser->code);
        parser->code = NULL;
        free(parser->tokens);
        parser->tokens = NULL;
        cz_error_list_free(parser->error_list);
        parser->error_list = NULL;
        cz_ast_root_free(parser->program);
        parser->program = NULL;
        cz_string_pool_free(parser->sp);
        parser->sp = NULL;
    }
    free(parser);
}

static inline unsigned int cz_parser_get_line(const CZ_Parser* parser) {
    if (parser == NULL) return 0;

    return parser->tokens[parser->idx].line;
}

static inline unsigned int cz_parser_get_col(const CZ_Parser* parser) {
    if (parser == NULL) return 0;

    return parser->tokens[parser->idx].column;
}

static CZ_TokenType cz_parser_peek_token_type(const CZ_Parser* parser, unsigned int offset) {
    if (parser == NULL) return CZ_TT_UNKNOWN;

    if (parser->idx + offset >= parser->n_tokens) return CZ_TT_EOF;

    return parser->tokens[parser->idx + offset].token_type;
}

static CZ_Precedence cz_parser_get_binary_operation_precedence(CZ_TokenType token_type) {
    switch (token_type) {
        case CZ_TT_EQUAL_EQUAL:
        case CZ_TT_EXCLAMATION_EQUAL:
            return CZ_PRECEDENCE_EQUALITY;
        case CZ_TT_LESS:
        case CZ_TT_GREATER:
        case CZ_TT_LESS_EQUAL:
        case CZ_TT_GREATER_EQUAL:
            return CZ_PRECEDENCE_RELATIONAL;
        case CZ_TT_BAR:
            return CZ_PRECEDENCE_BITWISE_OR;
        case CZ_TT_CAROT:
            return CZ_PRECEDENCE_BITWISE_XOR;
        case CZ_TT_AMPERSAND:
            return CZ_PRECEDENCE_BITWISE_AND;
        case CZ_TT_PLUS:
        case CZ_TT_MINUS:
            return CZ_PRECEDENCE_ADDITIVE;
        case CZ_TT_AS:
            return CZ_PRECEDENCE_CAST;
        case CZ_TT_STAR:
        case CZ_TT_SLASH:
        case CZ_TT_PERCENT:
            return CZ_PRECEDENCE_MULTIPLICATIVE;
        case CZ_TT_PERIOD:
        case CZ_TT_LEFT_PARENTHESIS:
            return CZ_PRECEDENCE_CALL_ACCESS;
        default:
            return CZ_PRECEDENCE_NONE;
    }
}

// TODO: For later ->
static inline bool cz_parser_is_operation_right_associative(CZ_TokenType token_type) {
    return false;
}

/**
 * @brief Push one of global declarations to the program node.
 *
 * @param subnode
 * @param program_node
 * @return int 1 if successful, 0 if failure.
 */
static int cz_parser_push_global_declaration(CZ_AST_Node* subnode, CZ_AST_Node* program_node) {
    if (subnode == NULL || program_node == NULL) goto error_fail_return;
    if (program_node->node_type != CZ_AST_ProgramNodeType) goto error_fail_return;

    if (program_node->program.declaration_count > program_node->program.capacity) goto error_fail_return;

    // List full. Double the capacity.
    if (program_node->program.declaration_count == program_node->program.capacity) {
        CZ_AST_Node** new_list = (CZ_AST_Node**) realloc(program_node->program.global_declaration_list, sizeof(CZ_AST_Node*) * 2 * program_node->program.capacity);
        NULL_POINTER_TO_GOTO(new_list, error_fail_return);

        program_node->program.global_declaration_list = new_list;
        program_node->program.capacity *= 2;
    }

    program_node->program.global_declaration_list[program_node->program.declaration_count] = subnode;
    program_node->program.declaration_count++;

    return 1;

error_fail_return:
    return 0;
}

/**
 * @brief Pushes parameter to parameter_list.
 *
 * @param parameter_list Parameter List AST node
 * @param param AST Node to parameter.
 * @return int 1 if successful. 0 if failure.
 */
static int cz_parser_push_function_param(CZ_AST_Node* parameter_list, CZ_AST_Node* param) {
    if (parameter_list == NULL || param == NULL) goto error_fail_return;
    if (parameter_list->node_type != CZ_AST_ParameterListNodeType) goto error_fail_return;

    // Increment capacity.
    CZ_AST_Node** new_list = (CZ_AST_Node**) realloc(parameter_list->parameter_list.params, sizeof(CZ_AST_Node*) * (parameter_list->parameter_list.param_count+1));
    NULL_POINTER_TO_GOTO(new_list, error_fail_return);

    parameter_list->parameter_list.params = new_list;
    new_list = NULL;
    parameter_list->parameter_list.params[parameter_list->parameter_list.param_count] = param;
    parameter_list->parameter_list.param_count++;

    return 1;

error_fail_return:
    return 0;
}

static int cz_parser_push_statement_to_block(CZ_AST_Node* block, CZ_AST_Node* statement) {
    if (block == NULL || statement == NULL) goto error_fail_return;
    if (block->node_type != CZ_AST_BlockStatementNodeType) goto error_fail_return;

    // Increment capacity.
    CZ_AST_Node** new_list = (CZ_AST_Node**) realloc(block->statement_list.statements, sizeof(CZ_AST_Node*) * (block->statement_list.statement_count+1));
    NULL_POINTER_TO_GOTO(new_list, error_fail_return);

    block->statement_list.statements = new_list;
    block->statement_list.statements[block->statement_list.statement_count] = statement;
    block->statement_list.statement_count++;

    return 1;

error_fail_return:
    return 0;
}

static int cz_parser_push_struct_member_to_members(CZ_AST_Node* struct_decl, CZ_AST_Node* member) {
    if (struct_decl == NULL || member == NULL) goto error_fail_return;
    if (!(struct_decl->node_type == CZ_AST_StructDeclarationNodeType && member->node_type == CZ_AST_VariableDeclarationNodeType) &&
        !(struct_decl->node_type == CZ_AST_StructInitNodeType && member->node_type == CZ_AST_StructInitMemberNodeType)) return 0;

    // Increment capacity.
    CZ_AST_Node** new_list = (CZ_AST_Node**) realloc(struct_decl->struct_declaration.members, sizeof(CZ_AST_Node*) * (struct_decl->struct_declaration.member_count+1));
    NULL_POINTER_TO_GOTO(new_list, error_fail_return);

    struct_decl->struct_declaration.members = new_list;
    struct_decl->struct_declaration.members[struct_decl->struct_declaration.member_count] = member;
    struct_decl->struct_declaration.member_count++;

    return 1;

error_fail_return:
    return 0;
}

static int cz_parser_push_argument_to_function_call_argument_list(CZ_AST_Node* function_call, CZ_AST_Node* argument) {
    if (function_call == NULL || argument == NULL) goto error_fail_return;
    if (function_call->node_type != CZ_AST_FunctionCallNodeType) goto error_fail_return;

    // Increment capacity.
    CZ_AST_Node** new_list = (CZ_AST_Node**) realloc(function_call->function_call.arguments, sizeof(CZ_AST_Node*) * (function_call->function_call.arg_count+1));
    NULL_POINTER_TO_GOTO(new_list, error_fail_return);

    function_call->function_call.arguments = new_list;
    function_call->function_call.arguments[function_call->function_call.arg_count] = argument;
    function_call->function_call.arg_count++;

    return 1;

error_fail_return:
    return 0;
}

static CZ_AST_Node* cz_parser_create_program_node(CZ_Parser* parser) {
    if (parser == NULL) return NULL;

    CZ_AST_Node* node = cz_ast_node_create(CZ_AST_ProgramNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    if (node == NULL) return NULL;

    node->program.capacity = 8;
    node->program.global_declaration_list = (CZ_AST_Node**) calloc(node->program.capacity, sizeof(CZ_AST_Node*));
    if (node->program.global_declaration_list == NULL) {
        cz_ast_root_free(node);
        return NULL;
    }
    node->program.declaration_count = 0;

    CZ_AST_Node* subnode = NULL;
    // Check if
    // 1. Function declaration
    // 2. Struct declaration
    // 3. Variable declaration (Global)
    while (true) {
error_sync_target:
        if (cz_parser_peek_token_type(parser, 0) == CZ_TT_EOF) break;

        const CZ_TokenType cur_token_type = cz_parser_peek_token_type(parser, 0);
        subnode = NULL;

        switch (cur_token_type) {
            case CZ_TT_FUNC:
                subnode = cz_parser_create_function_decl(parser);
                if (subnode == NULL) {
                    cz_parser_sync(parser);
                    goto error_sync_target;
                }

                break;
            case CZ_TT_STRUCT:
                subnode = cz_parser_create_struct_decl(parser);
                if (subnode == NULL) {
                    cz_parser_sync(parser);
                    goto error_sync_target;
                }
                break;
            case CZ_TT_CONST:
            case CZ_TT_IDENTIFIER:
                subnode = cz_parser_create_variable_decl(parser);
                if (subnode == NULL) {
                    cz_parser_sync(parser);
                    goto error_sync_target;
                }

                {
                    CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
                    if (semicolon == NULL) {
                        cz_ast_root_free(subnode);
                        subnode = NULL;
                        cz_parser_sync(parser);
                        goto error_sync_target;
                    }
                }

                break;
            case CZ_TT_TYPEDEF:
                subnode = cz_parser_create_type_decl(parser, false);
                if (subnode == NULL) {
                    cz_parser_sync(parser);
                    goto error_sync_target;
                }

                {
                    CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
                    if (semicolon == NULL) {
                        cz_ast_root_free(subnode);
                        subnode = NULL;
                        cz_parser_sync(parser);
                        goto error_sync_target;
                    }
                }
                break;
            case CZ_TT_NEWTYPE:
                subnode = cz_parser_create_type_decl(parser, true);
                if (subnode == NULL) {
                    cz_parser_sync(parser);
                    goto error_sync_target;
                }

                {
                    CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
                    if (semicolon == NULL) {
                        cz_ast_root_free(subnode);
                        subnode = NULL;
                        cz_parser_sync(parser);
                        goto error_sync_target;
                    }
                }
                break;
            default:
                cz_parser_sync(parser);
                goto error_sync_target;
                break;
        }

        if (cz_parser_push_global_declaration(subnode, node) != 1) {
            cz_parser_sync(parser);
            cz_ast_root_free(subnode);
            subnode = NULL;
            cz_error_list_push_error(parser->error_list, parser->filename, parser->tokens[parser->idx].line, parser->tokens[parser->idx].column, "[-] Fatal Error: Cannot add statement to global scope");
            goto error_sync_target;
        } else {
            subnode = NULL;
        }

    }

    return node;
}

static CZ_AST_Node* cz_parser_create_struct_decl(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* identifier = NULL;
    CZ_AST_Node* member = NULL;

    // struct
    {
        CZ_Token* struct_token = cz_parser_consume_token(parser, CZ_TT_STRUCT);
        NULL_POINTER_TO_GOTO(struct_token, error_free_node);
    }

    // identifier
    identifier = cz_parser_create_identifier(parser);
    NULL_POINTER_TO_GOTO(identifier, error_free_node);

    // {
    {
        CZ_Token* left_curly = cz_parser_consume_token(parser, CZ_TT_LEFT_CURLY_BRACKET);
        NULL_POINTER_TO_GOTO(left_curly, error_free_node);
    }

    node = cz_ast_node_create(CZ_AST_StructDeclarationNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    // struct member list
    // 1. Check if empty.
    // 2. Parse member.
    // 3. Peek -> if right curly, break. Otherwise consume

    if (cz_parser_peek_token_type(parser, 0) != CZ_TT_RIGHT_CURLY_BRACKET) {
        do {
            member = cz_parser_create_variable_decl(parser);
            NULL_POINTER_TO_GOTO(member, error_free_node);

            if (cz_parser_push_struct_member_to_members(node, member) != 1) {
                goto error_free_node;
            }
            member = NULL;

            {
                CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
                NULL_POINTER_TO_GOTO(semicolon, error_free_node);
            }

        } while (cz_parser_peek_token_type(parser, 0) != CZ_TT_RIGHT_CURLY_BRACKET);
    }

    // }
    {
        CZ_Token* right_curly = cz_parser_consume_token(parser, CZ_TT_RIGHT_CURLY_BRACKET);
        NULL_POINTER_TO_GOTO(right_curly, error_free_node);
    }

    node->struct_declaration.identifier = identifier;
    identifier = NULL;

    return node;

error_free_node:
    cz_ast_root_free(node);
    cz_ast_root_free(identifier);
    cz_ast_root_free(member);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_struct_init(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* identifier = NULL;
    CZ_AST_Node* member = NULL;

    node = cz_ast_node_create(CZ_AST_StructInitNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    identifier = cz_parser_create_identifier(parser);
    NULL_POINTER_TO_GOTO(identifier, error_free_node);

    // {
    {
        CZ_Token* left_curly_token = cz_parser_consume_token(parser, CZ_TT_LEFT_CURLY_BRACKET);
        NULL_POINTER_TO_GOTO(left_curly_token, error_free_node);
    }

    // --- Struct Init list ---
    // Check empty case.
    if (cz_parser_peek_token_type(parser, 0) != CZ_TT_RIGHT_CURLY_BRACKET) {
        // Not empty!
        // Do one entry first.
        member = cz_parser_create_struct_init_member(parser);
        NULL_POINTER_TO_GOTO(member, error_free_node);
        if (cz_parser_push_struct_member_to_members(node, member) != 1) {
            goto error_free_node;
        }
        member = NULL;

        // See if next entry exists.
        while (cz_parser_peek_token_type(parser, 0) == CZ_TT_COMMA) {
            {
                CZ_Token* comma = cz_parser_consume_token(parser, CZ_TT_COMMA);
                NULL_POINTER_TO_GOTO(comma, error_free_node);
            }

            member = cz_parser_create_struct_init_member(parser);
            NULL_POINTER_TO_GOTO(member, error_free_node);
            if (cz_parser_push_struct_member_to_members(node, member) != 1) {
                goto error_free_node;
            }
            member = NULL;
        }
    }

    // }
    {
        CZ_Token* right_curly_token = cz_parser_consume_token(parser, CZ_TT_RIGHT_CURLY_BRACKET);
        NULL_POINTER_TO_GOTO(right_curly_token, error_free_node);
    }
    
    node->struct_declaration.identifier = identifier;
    identifier = NULL;
    return node;

error_free_node:
    cz_ast_root_free(node);
    cz_ast_root_free(identifier);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_struct_init_member(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* identifier = NULL;
    CZ_AST_Node* expression = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    // .
    {
        CZ_Token* period_token = cz_parser_consume_token(parser, CZ_TT_PERIOD);
        NULL_POINTER_TO_GOTO(period_token, error_free_node);
    }

    identifier = cz_parser_create_identifier(parser);
    NULL_POINTER_TO_GOTO(identifier, error_free_node);

    // =
    {
        CZ_Token* equal_token = cz_parser_consume_token(parser, CZ_TT_EQUAL);
        NULL_POINTER_TO_GOTO(equal_token, error_free_node);
    }

    expression = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
    NULL_POINTER_TO_GOTO(expression, error_free_node);

    node = cz_ast_node_create(CZ_AST_StructInitMemberNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    node->struct_init_member.identifier = identifier;
    identifier = NULL;
    node->struct_init_member.expression = expression;
    expression = NULL;

    return node;

error_free_node:
    cz_ast_root_free(node);
    cz_ast_root_free(identifier);
    cz_ast_root_free(expression);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_variable_decl(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* expression = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    node = cz_parser_create_param_decl(parser);
    NULL_POINTER_TO_GOTO(node, error_free_node);

    // See if there is assignment at declaration.
    if (cz_parser_peek_token_type(parser, 0) == CZ_TT_EQUAL) {
        CZ_Token* equal_token = cz_parser_consume_token(parser, CZ_TT_EQUAL);
        NULL_POINTER_TO_GOTO(equal_token, error_free_node);

        expression = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
        NULL_POINTER_TO_GOTO(expression, error_free_node);

        node->variable_declaration.expression = expression;
        // We have transferred ownership of expression to the node, so set expression to NULL to avoid freeing in error label.
        expression = NULL;
    }

    return node;

error_free_node:
    cz_ast_root_free(expression);
    cz_ast_root_free(node);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_type_decl(CZ_Parser* parser, bool is_strong) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* new_type = NULL;
    CZ_AST_Node* type = NULL;

    if (parser == NULL) return NULL;

    // typedef / newtype
    {
        CZ_Token* type_token = cz_parser_consume_token(parser, is_strong ? CZ_TT_NEWTYPE : CZ_TT_TYPEDEF);
        NULL_POINTER_TO_GOTO(type_token, error_free_node);
    }

    type = cz_parser_create_base_type(parser);
    NULL_POINTER_TO_GOTO(type, error_free_node);

    new_type = cz_parser_create_identifier(parser);
    NULL_POINTER_TO_GOTO(new_type, error_free_node);

    node = cz_ast_node_create(is_strong ? CZ_AST_NewtypeDeclarationNodeType : CZ_AST_TypedefDeclarationNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    node->typedef_declaration.type = type;
    type = NULL;
    node->typedef_declaration.new_type = new_type;
    new_type = NULL;

    return node;

error_free_node:
    cz_ast_root_free(node);
    cz_ast_root_free(type);
    cz_ast_root_free(new_type);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_assignment(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* l_expression = NULL;
    CZ_AST_Node* r_expression = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    l_expression = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
    NULL_POINTER_TO_GOTO(l_expression, error_free_node);


    CZ_TokenType op_token_type = cz_parser_peek_token_type(parser, 0);
    if (cz_token_type_is_assignment(op_token_type)) {
        CZ_Token* op_token = cz_parser_consume_token(parser, op_token_type);
        NULL_POINTER_TO_GOTO(op_token, error_free_node);
    } else {
        goto error_free_node;
    }
        
    r_expression = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
    NULL_POINTER_TO_GOTO(r_expression, error_free_node);

    node = cz_ast_node_create(CZ_AST_AssignmentStatementNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    node->binary_expression.op = op_token_type;
    node->binary_expression.left = l_expression;
    l_expression = NULL;
    node->binary_expression.right = r_expression;
    r_expression = NULL;

    return node;

error_free_node:
    cz_ast_root_free(node);
    cz_ast_root_free(l_expression);
    cz_ast_root_free(r_expression);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_function_decl(CZ_Parser* parser) {
    CZ_AST_Node* param_list = NULL;
    CZ_AST_Node* function_identifier = NULL;
    CZ_AST_Node* type = NULL;
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* body = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    // func
    {
        CZ_Token* func_token = cz_parser_consume_token(parser, CZ_TT_FUNC);
        NULL_POINTER_TO_GOTO(func_token, error_free_node);
    }

    // identifier
    function_identifier = cz_parser_create_identifier(parser);
    NULL_POINTER_TO_GOTO(function_identifier, error_free_node);

    // :: (
    {
        CZ_Token* type_separator = cz_parser_consume_token(parser, CZ_TT_COLON_COLON);
        NULL_POINTER_TO_GOTO(type_separator, error_free_node);
        CZ_Token* left_paren = cz_parser_consume_token(parser, CZ_TT_LEFT_PARENTHESIS);
        NULL_POINTER_TO_GOTO(left_paren, error_free_node);
    }

    // param_list
    param_list = cz_parser_create_param_list(parser);
    NULL_POINTER_TO_GOTO(param_list, error_free_node);

    // ) ->
    {
        CZ_Token* right_paren = cz_parser_consume_token(parser, CZ_TT_RIGHT_PARENTHESIS);
        NULL_POINTER_TO_GOTO(right_paren, error_free_node);
    }

    if (cz_parser_peek_token_type(parser, 0) == CZ_TT_RIGHT_ARROW) {
        {
            CZ_Token* arrow = cz_parser_consume_token(parser, CZ_TT_RIGHT_ARROW);
            NULL_POINTER_TO_GOTO(arrow, error_free_node);
        }

        // type
        type = cz_parser_create_type(parser);
        NULL_POINTER_TO_GOTO(type, error_free_node);
    } else {
        type = cz_ast_node_create(CZ_AST_TypeNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
        NULL_POINTER_TO_GOTO(type, error_free_node);
        type->type_expression.is_const = false;
        type->type_expression.is_reference = false;
        type->type_expression.is_function_type = false;
        type->type_expression.primitive.kind = CZ_AST_TYPE_KIND_VOID;
    }

    
    // body
    body = cz_parser_create_block(parser);
    NULL_POINTER_TO_GOTO(body, error_free_node);

    node = cz_ast_node_create(CZ_AST_FunctionDeclarationNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    node->function_declaration.function_identifier = function_identifier;
    function_identifier = NULL;
    node->function_declaration.function.parameter_list = param_list;
    param_list = NULL;
    node->function_declaration.function.return_type = type;
    type = NULL;
    node->function_declaration.body = body;
    body = NULL;

    return node;

error_free_node:
    cz_ast_root_free(function_identifier);
    cz_ast_root_free(type);
    cz_ast_root_free(param_list);
    cz_ast_root_free(node);
    cz_ast_root_free(body);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_param_list(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* param = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    node = cz_ast_node_create(CZ_AST_ParameterListNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    // No need to allocate as realloc in param_push will handle.
    node->parameter_list.params = NULL;
    node->parameter_list.param_count = 0;

    // Check empty case.
    {
        CZ_TokenType peeked = cz_parser_peek_token_type(parser, 0);
        if (peeked != CZ_TT_IDENTIFIER) {
            return node;
        }
    }

    // Single
    {
        param = cz_parser_create_param_decl(parser);
        NULL_POINTER_TO_GOTO(param, error_free_node);

        if (cz_parser_push_function_param(node, param) != 1) {
            cz_ast_root_free(param);
            param = NULL;
            goto error_free_node;
        }
        param = NULL;
    }

    while (cz_parser_peek_token_type(parser, 0) == CZ_TT_COMMA) {
        CZ_Token* comma = cz_parser_consume_token(parser, CZ_TT_COMMA);
        NULL_POINTER_TO_GOTO(comma, error_free_node);

        param = cz_parser_create_param_decl(parser);
        NULL_POINTER_TO_GOTO(param, error_free_node);

        if (cz_parser_push_function_param(node, param) != 1) {
            cz_ast_root_free(param);
            param = NULL;
            goto error_free_node;
        }
        param = NULL;
    }

    return node;

error_free_node:
    cz_ast_root_free(param);
    cz_ast_root_free(node);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_param_decl(CZ_Parser* parser) {
    CZ_AST_Node* identifier = NULL;
    CZ_AST_Node* type = NULL;
    CZ_AST_Node* node = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    // Identifier
    identifier = cz_parser_create_identifier(parser);
    NULL_POINTER_TO_GOTO(identifier, error_free_node);

    // ::
    {
        CZ_Token* token = cz_parser_consume_token(parser, CZ_TT_COLON_COLON);
        NULL_POINTER_TO_GOTO(token, error_free_node);
    }

    type = cz_parser_create_type(parser);
    NULL_POINTER_TO_GOTO(type, error_free_node);

    node = cz_ast_node_create(CZ_AST_VariableDeclarationNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    node->variable_declaration.identifier = identifier;
    node->variable_declaration.type = type;
    node->variable_declaration.expression = NULL;

    // Now we have transferred ownership of identifier and type to the node, so set to NULL to avoid freeing in error label.
    identifier = NULL;
    type = NULL;

    return node;

error_free_node:
    cz_ast_root_free(type);
    cz_ast_root_free(identifier);
    cz_ast_root_free(node);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_block(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* statement = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    // {
    {
        CZ_Token* left_curly = cz_parser_consume_token(parser, CZ_TT_LEFT_CURLY_BRACKET);
        NULL_POINTER_TO_GOTO(left_curly, error_free_node);
    }

    node = cz_ast_node_create(CZ_AST_BlockStatementNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    while (cz_parser_peek_token_type(parser, 0) != CZ_TT_RIGHT_CURLY_BRACKET) {
        statement = cz_parser_create_statement(parser);
        NULL_POINTER_TO_GOTO(statement, error_free_node);

        if (cz_parser_push_statement_to_block(node, statement) != 1) goto error_free_node;
        statement = NULL;
    }

    // }
    {
        CZ_Token* right_curly = cz_parser_consume_token(parser, CZ_TT_RIGHT_CURLY_BRACKET);
        NULL_POINTER_TO_GOTO(right_curly, error_free_node);
    }

    return node;

error_free_node:
    cz_ast_root_free(node);
    cz_ast_root_free(statement);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_statement(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* lhs = NULL;
    CZ_AST_Node* rhs = NULL;
    NULL_POINTER_TO_GOTO(parser, error_free_node);

    const CZ_TokenType peeked = cz_parser_peek_token_type(parser, 0);

    switch (peeked) {
        case CZ_TT_RETURN:
            node = cz_parser_create_return_statement(parser);
            {
                CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
                NULL_POINTER_TO_GOTO(semicolon, error_free_node);
            }
            break;
        case CZ_TT_IF:
            node = cz_parser_create_if_statement(parser);
            break;
        case CZ_TT_FOR:
            node = cz_parser_create_for_statement(parser);
            break;
        case CZ_TT_WHILE:
            node = cz_parser_create_while_statement(parser);
            break;
        case CZ_TT_LEFT_CURLY_BRACKET:
            node = cz_parser_create_block(parser);
            break;
        //case CZ_TT_CONST:
        //    // Explicitly only variable declaration.
        //    node = cz_parser_create_variable_decl(parser);
        //    {
        //        CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
        //        NULL_POINTER_TO_GOTO(semicolon, error_free_node);
        //    }
        //    break;
        case CZ_TT_IDENTIFIER:
            // Three cases:
            // 1. Variable declaration.
            // 2. Assignment.
            // 3. Expression.
            if (cz_parser_peek_token_type(parser, 1) == CZ_TT_COLON_COLON) {
                // :: means this must be variable declaration.
                node = cz_parser_create_variable_decl(parser);
            } else {
                lhs = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
                NULL_POINTER_TO_GOTO(lhs, error_free_node);

                if (cz_token_type_is_assignment(cz_parser_peek_token_type(parser, 0))) {
                    // Assignment

                    // Consume assignment token
                    const CZ_TokenType assignment_operator_token_type = cz_parser_peek_token_type(parser, 0);
                    CZ_Token* assignment_operator_token = cz_parser_consume_token(parser, assignment_operator_token_type);
                    NULL_POINTER_TO_GOTO(assignment_operator_token, error_free_node);

                    // Consume RHS expression
                    rhs = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
                    NULL_POINTER_TO_GOTO(rhs, error_free_node);

                    node = cz_ast_node_create(CZ_AST_AssignmentStatementNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
                    NULL_POINTER_TO_GOTO(node, error_free_node);

                    node->binary_expression.op = assignment_operator_token_type;
                    node->binary_expression.left = lhs;
                    lhs = NULL;
                    node->binary_expression.right = rhs;
                    rhs = NULL;

                } else {
                    // Expression
                    node = lhs;
                    lhs = NULL;
                }
            }

            // All three cases require semicolon at the end.
            {
                CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
                NULL_POINTER_TO_GOTO(semicolon, error_free_node);
            }
            break;
        default:
            node = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
            {
                CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
                NULL_POINTER_TO_GOTO(semicolon, error_free_node);
            }
            break;
    }
    NULL_POINTER_TO_GOTO(node, error_free_node);

    return node;

error_free_node:
    cz_ast_root_free(node);
    cz_ast_root_free(lhs);
    cz_ast_root_free(rhs);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_return_statement(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* expression = NULL;

    if (parser == NULL) return NULL;

    {
        CZ_Token* return_token = cz_parser_consume_token(parser, CZ_TT_RETURN);
        NULL_POINTER_TO_GOTO(return_token, error_free_node);
    }

    if (cz_parser_peek_token_type(parser, 0) != CZ_TT_SEMICOLON) {
        expression = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
        NULL_POINTER_TO_GOTO(expression, error_free_node);
    }

    
    // Wiring up.
    node = cz_ast_node_create(CZ_AST_ReturnStatementNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    node->return_statement.expression = expression;
    expression = NULL;

    return node;

error_free_node:
    cz_ast_root_free(node);
    cz_ast_root_free(expression);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_if_statement(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* condition = NULL;
    CZ_AST_Node* if_branch = NULL;
    CZ_AST_Node* else_branch = NULL;

    if (parser == NULL) return NULL;

    // if (
    {
        CZ_Token* if_token = cz_parser_consume_token(parser, CZ_TT_IF);
        NULL_POINTER_TO_GOTO(if_token, error_free_node);

        CZ_Token* left_paren = cz_parser_consume_token(parser, CZ_TT_LEFT_PARENTHESIS);
        NULL_POINTER_TO_GOTO(left_paren, error_free_node);
    }

    // Condition
    condition = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
    NULL_POINTER_TO_GOTO(condition, error_free_node);

    // )
    {
        CZ_Token* right_paren = cz_parser_consume_token(parser, CZ_TT_RIGHT_PARENTHESIS);
        NULL_POINTER_TO_GOTO(right_paren, error_free_node);
    }

    // Block
    if_branch = cz_parser_create_block(parser);
    NULL_POINTER_TO_GOTO(if_branch, error_free_node);

    // else branch
    if (cz_parser_peek_token_type(parser, 0) == CZ_TT_ELSE) {
        // else
        {
            CZ_Token* else_token = cz_parser_consume_token(parser, CZ_TT_ELSE);
            NULL_POINTER_TO_GOTO(else_token, error_free_node);
        }

        CZ_TokenType else_peeked = cz_parser_peek_token_type(parser, 0);
        switch (else_peeked) {
            case CZ_TT_LEFT_CURLY_BRACKET:
                else_branch = cz_parser_create_block(parser);
                break;
            case CZ_TT_IF:
                else_branch = cz_parser_create_if_statement(parser);
                break;
            default:
                break;
        }
        NULL_POINTER_TO_GOTO(else_branch, error_free_node);
    }

    node = cz_ast_node_create(CZ_AST_IfStatementNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_free_node);

    // Wire up
    node->if_statement.condition = condition;
    condition = NULL;
    node->if_statement.if_branch = if_branch;
    if_branch = NULL;
    node->if_statement.else_branch = else_branch;
    else_branch = NULL;
    return node;

error_free_node:
    cz_ast_root_free(node);
    cz_ast_root_free(condition);
    cz_ast_root_free(if_branch);
    cz_ast_root_free(else_branch);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_for_statement(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* initialization = NULL;
    CZ_AST_Node* condition = NULL;
    CZ_AST_Node* iteration_step = NULL;
    CZ_AST_Node* lhs = NULL;
    CZ_AST_Node* rhs = NULL;
    CZ_AST_Node* body = NULL;

    NULL_POINTER_TO_GOTO(parser, error_node_free);

    // for (
    {
        CZ_Token* for_token = cz_parser_consume_token(parser, CZ_TT_FOR);
        NULL_POINTER_TO_GOTO(for_token, error_node_free);

        CZ_Token* left_paren = cz_parser_consume_token(parser, CZ_TT_LEFT_PARENTHESIS);
        NULL_POINTER_TO_GOTO(left_paren, error_node_free);
    }

    // Initialization:
    // Lookahead: idx=CZ_TT_IDENTIFIER, idx+1=CZ_TT_COLON_COLON -> variable declaration
    // Lookahead: idx=CZ_TT_IDENTIFIER, idx+1="assignment" -> assignment
    if (cz_parser_peek_token_type(parser, 0) == CZ_TT_IDENTIFIER && cz_parser_peek_token_type(parser, 1) == CZ_TT_COLON_COLON) {
        initialization = cz_parser_create_variable_decl(parser);
        NULL_POINTER_TO_GOTO(initialization, error_node_free);
    } else if (cz_parser_peek_token_type(parser, 0) == CZ_TT_IDENTIFIER && cz_token_type_is_assignment(cz_parser_peek_token_type(parser, 1))) {
        initialization = cz_parser_create_assignment(parser);
        NULL_POINTER_TO_GOTO(initialization, error_node_free);
    } else {
        initialization = NULL;
    }

    // ;
    {
        CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
        NULL_POINTER_TO_GOTO(semicolon, error_node_free);
    }

    // condition
    if (cz_parser_peek_token_type(parser, 0) != CZ_TT_SEMICOLON) {
        condition = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
        NULL_POINTER_TO_GOTO(condition, error_node_free);
    }

    // ;
    {
        CZ_Token* semicolon = cz_parser_consume_token(parser, CZ_TT_SEMICOLON);
        NULL_POINTER_TO_GOTO(semicolon, error_node_free);
    }

    // iteration_step
    // try creating expression
    // then check for assignment token.
    // if so, consume, and create another expression then create assignment.
    if (cz_parser_peek_token_type(parser, 0) != CZ_TT_RIGHT_PARENTHESIS) {
        lhs = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
        NULL_POINTER_TO_GOTO(lhs, error_node_free);

        CZ_TokenType peeked = cz_parser_peek_token_type(parser, 0);
        if (cz_token_type_is_assignment(peeked)) {
            // Assignment
            const CZ_Token* assignment_token = cz_parser_consume_token(parser, peeked);
            NULL_POINTER_TO_GOTO(assignment_token, error_node_free);

            rhs = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
            NULL_POINTER_TO_GOTO(rhs, error_node_free);

            iteration_step = cz_ast_node_create(CZ_AST_AssignmentStatementNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
            NULL_POINTER_TO_GOTO(iteration_step, error_node_free);

            iteration_step->binary_expression.op = peeked;
            iteration_step->binary_expression.left = lhs;
            lhs = NULL;
            iteration_step->binary_expression.right = rhs;
            rhs = NULL;
        } else {
            iteration_step = lhs;
            lhs = NULL;
        }
    }

    // )
    {
        CZ_Token* right_paren = cz_parser_consume_token(parser, CZ_TT_RIGHT_PARENTHESIS);
        NULL_POINTER_TO_GOTO(right_paren, error_node_free);
    }

    body = cz_parser_create_block(parser);
    NULL_POINTER_TO_GOTO(body, error_node_free);

    node = cz_ast_node_create(CZ_AST_ForStatementNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_node_free);

    node->for_statement.initialization = initialization;
    initialization = NULL;
    node->for_statement.condition = condition;
    condition = NULL;
    node->for_statement.iteration_step = iteration_step;
    iteration_step = NULL;
    node->for_statement.body = body;
    body = NULL;

    return node;

error_node_free:
    cz_ast_root_free(node);
    cz_ast_root_free(initialization);
    cz_ast_root_free(condition);
    cz_ast_root_free(iteration_step);
    cz_ast_root_free(body);
    cz_ast_root_free(lhs);
    cz_ast_root_free(rhs);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_while_statement(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* condition = NULL;
    CZ_AST_Node* body = NULL;

    NULL_POINTER_TO_GOTO(parser, error_node_free);

    {
        CZ_Token* while_token = cz_parser_consume_token(parser, CZ_TT_WHILE);
        NULL_POINTER_TO_GOTO(while_token, error_node_free);
    }

    condition = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
    NULL_POINTER_TO_GOTO(condition, error_node_free);

    body = cz_parser_create_block(parser);
    NULL_POINTER_TO_GOTO(body, error_node_free);

    node = cz_ast_node_create(CZ_AST_WhileStatementNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    NULL_POINTER_TO_GOTO(node, error_node_free);

    node->while_statement.condition = condition;
    condition = NULL;
    node->while_statement.body = body;
    body = NULL;

    return node;

error_node_free:
    cz_ast_root_free(node);
    cz_ast_root_free(condition);
    cz_ast_root_free(body);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_type(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    bool is_const = false;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    if (cz_parser_peek_token_type(parser, 0) == CZ_TT_CONST) {
        const CZ_Token* const_token = cz_parser_consume_token(parser, CZ_TT_CONST);
        NULL_POINTER_TO_GOTO(const_token, error_free_node);
        is_const = true;
    }

    node = cz_parser_create_base_type(parser);
    NULL_POINTER_TO_GOTO(node, error_free_node);

    if (cz_parser_peek_token_type(parser, 0) == CZ_TT_AMPERSAND) {
        CZ_Token* token = cz_parser_consume_token(parser, CZ_TT_AMPERSAND);
        NULL_POINTER_TO_GOTO(token, error_free_node);

        node->type_expression.is_reference = true;
    }
    node->type_expression.is_const = is_const;

    return node;

error_free_node:
    cz_ast_root_free(node);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_base_type(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    CZ_TokenType peeked = cz_parser_peek_token_type(parser, 0);

    switch (peeked) {
        // TODO: Add more types as more are added.
        case CZ_TT_BOOL:
        case CZ_TT_INT32:
        case CZ_TT_FLOAT:
        case CZ_TT_IDENTIFIER:
            {
                CZ_Token* token = cz_parser_consume_token(parser, peeked);
                NULL_POINTER_TO_GOTO(token, error_free_node);

                node = cz_ast_node_create(CZ_AST_TypeNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
                NULL_POINTER_TO_GOTO(node, error_free_node);

                node->type_expression.is_reference = false;
                node->type_expression.is_function_type = false;
                node->type_expression.primitive.name = (const char*) token->lexeme;
                switch (peeked) {
                    case CZ_TT_BOOL:
                        node->type_expression.primitive.kind = CZ_AST_TYPE_KIND_BOOL;
                        break;
                    case CZ_TT_INT32:
                        node->type_expression.primitive.kind = CZ_AST_TYPE_KIND_INT32;
                        break;
                    case CZ_TT_FLOAT:
                        node->type_expression.primitive.kind = CZ_AST_TYPE_KIND_FLOAT;
                        break;
                    case CZ_TT_IDENTIFIER:
                        node->type_expression.primitive.kind = CZ_AST_TYPE_KIND_IDENTIFIER;
                    default:
                        break;
                }
            }
            break;
        default:
            cz_error_list_push_error(parser->error_list, parser->filename, cz_parser_get_line(parser), cz_parser_get_col(parser), "Unrecognized base type");
            goto error_free_node;
    }

    return node;

error_free_node:
    cz_ast_root_free(node);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_literal(CZ_Parser* parser) {
    CZ_Token* token = NULL;
    CZ_AST_Node* node = NULL;

    if (parser == NULL) return NULL;

    // We expect the current token to be a literal (numerical or string)
    CZ_TokenType peeked = cz_parser_peek_token_type(parser, 0);
    switch (peeked) {
        case CZ_TT_NUMERICAL_LITERAL:
        case CZ_TT_STRING_LITERAL:
        case CZ_TT_TRUE:
        case CZ_TT_FALSE:
            token = cz_parser_consume_token(parser, peeked);
            if (token == NULL) return NULL;

            node = cz_ast_node_create(CZ_AST_LiteralNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
            if (node == NULL) return NULL;

            node->literal.literal_type = token->token_type;
            node->literal.lexeme = (const char*) token->lexeme;
            break;
        default:
            return NULL;
    }


    return node;
}

static CZ_AST_Node* cz_parser_create_identifier(CZ_Parser* parser) {
    CZ_Token* token = NULL;
    CZ_AST_Node* node = NULL;

    if (parser == NULL) return NULL;

    token = cz_parser_consume_token(parser, CZ_TT_IDENTIFIER);
    if (token == NULL) return NULL;

    node = cz_ast_node_create(CZ_AST_IdentifierNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
    if (node == NULL) return NULL;

    node->identifier.name = (const char*) token->lexeme;

    return node;
}

static CZ_AST_Node* cz_parser_create_expression(CZ_Parser* parser, CZ_Precedence min_binding_power) {
    CZ_AST_Node* lhs = NULL;
    CZ_AST_Node* rhs = NULL;
    CZ_AST_Node* parent = NULL;
    CZ_AST_Node* argument = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    lhs = cz_parser_create_unary(parser);
    NULL_POINTER_TO_GOTO(lhs, error_free_node);

    // Loop while next operator binds tighter than current threshold
    while (true) {
        CZ_TokenType next_op = cz_parser_peek_token_type(parser, 0);
        CZ_Precedence op_power = cz_parser_get_binary_operation_precedence(next_op);

        // Not binary operator or less tight bond.
        if (op_power <= min_binding_power) break;

        CZ_Token* token = cz_parser_consume_token(parser, next_op);
        NULL_POINTER_TO_GOTO(token, error_free_node);

        if (next_op == CZ_TT_AS) {
            parent = cz_ast_node_create(CZ_AST_CastExpressionNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
            NULL_POINTER_TO_GOTO(parent, error_free_node);
            
            parent->cast_expression.expression = lhs;
            lhs = NULL;
            parent->cast_expression.type = cz_parser_create_type(parser); // Custom target parsing!
            NULL_POINTER_TO_GOTO(parent->cast_expression.type, error_free_node);
            
            lhs = parent;
            parent = NULL;
            continue;
        } else if (next_op == CZ_TT_PERIOD) {
            parent = cz_ast_node_create(CZ_AST_StructMemberAccessNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
            NULL_POINTER_TO_GOTO(parent, error_free_node);

            parent->struct_member_access.object = lhs; // Left-hand expression moves to object slot
            lhs = NULL;
            
            // The right hand side MUST be a direct identifier name token
            CZ_AST_Node* struct_access_member = cz_parser_create_identifier(parser);
            NULL_POINTER_TO_GOTO(struct_access_member, error_free_node);
            parent->struct_member_access.member = struct_access_member;
            struct_access_member = NULL;

            lhs = parent;
            parent = NULL;
            continue;
        } else if (next_op == CZ_TT_LEFT_PARENTHESIS) {
            parent = cz_ast_node_create(CZ_AST_FunctionCallNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
            NULL_POINTER_TO_GOTO(parent, error_free_node);

            // The expression built so far on the left is the thing being called!
            parent->function_call.callee = lhs; 
            lhs =  NULL;

            if (cz_parser_peek_token_type(parser, 0) != CZ_TT_RIGHT_PARENTHESIS) {
                // There is at least one argument to parse.
                while (true) {
                    argument = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
                    NULL_POINTER_TO_GOTO(argument, error_free_node);

                    if (cz_parser_push_argument_to_function_call_argument_list(parent, argument) != 1) {
                        goto error_free_node;
                    }
                    argument = NULL;

                    if (cz_parser_peek_token_type(parser, 0) == CZ_TT_RIGHT_PARENTHESIS) {
                        break;
                    }

                    // ,
                    {
                        CZ_Token* comma_token = cz_parser_consume_token(parser, CZ_TT_COMMA);
                        NULL_POINTER_TO_GOTO(comma_token, error_free_node);
                    }
                }
            }

            // )
            {
                CZ_Token* right_paren = cz_parser_consume_token(parser, CZ_TT_RIGHT_PARENTHESIS);
                NULL_POINTER_TO_GOTO(right_paren, error_free_node);
            }


            lhs = parent;
            parent = NULL;
            continue;
        }


        CZ_Precedence next_min_power = op_power;
        if (cz_parser_is_operation_right_associative(next_op)) {
            next_min_power = op_power - 1;
        }

        rhs = cz_parser_create_expression(parser, next_min_power);
        NULL_POINTER_TO_GOTO(rhs, error_free_node);

        // Allocate parent binary node and wire them up
        parent = cz_ast_node_create(CZ_AST_BinaryExpressionNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
        NULL_POINTER_TO_GOTO(parent, error_free_node);
        parent->binary_expression.op = next_op;
        parent->binary_expression.left = lhs;
        parent->binary_expression.right = rhs;

        // Now we have transferred ownership of lhs and rhs to the parent, so set the source pointers to NULL.
        lhs = NULL;
        rhs = NULL;
        // Now we want to use the parent as the current lhs for the next iteration.
        lhs = parent;
        parent = NULL;
    }

    return lhs;

error_free_node:
    cz_ast_root_free(rhs);
    cz_ast_root_free(parent);
    cz_ast_root_free(lhs);
    cz_ast_root_free(argument);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_unary(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;
    CZ_AST_Node* subnode = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    const CZ_TokenType cur_token_type = cz_parser_peek_token_type(parser, 0);

    switch (cur_token_type) {
        case CZ_TT_MINUS:
        case CZ_TT_EXCLAMATION:
            {
                CZ_Token* token = cz_parser_consume_token(parser, cur_token_type);
                NULL_POINTER_TO_GOTO(token, error_free_node);

                node = cz_ast_node_create(CZ_AST_UnaryExpressionNodeType, cz_parser_get_line(parser), cz_parser_get_col(parser));
                NULL_POINTER_TO_GOTO(node, error_free_node);

                subnode = cz_parser_create_primary(parser);
                NULL_POINTER_TO_GOTO(subnode, error_free_node);

                node->unary_expression.op = token->token_type;
                node->unary_expression.operand = subnode;

                // We have transferred ownership of subnode to the node, so set subnode to NULL to avoid freeing in error label.
                subnode = NULL;
            }
            break;
        default:
            node = cz_parser_create_primary(parser);
            NULL_POINTER_TO_GOTO(node, error_free_node);
            return node;
    }

    return node;

error_free_node:
    cz_ast_root_free(subnode);
    cz_ast_root_free(node);
    return NULL;
}

static CZ_AST_Node* cz_parser_create_primary(CZ_Parser* parser) {
    CZ_AST_Node* node = NULL;

    NULL_POINTER_TO_GOTO(parser, error_free_node);

    const CZ_TokenType cur_token_type = cz_parser_peek_token_type(parser, 0);

    // TODO: Expression
    CZ_Token* token = NULL;
    switch (cur_token_type) {
        case CZ_TT_NUMERICAL_LITERAL:
        case CZ_TT_STRING_LITERAL:
        case CZ_TT_TRUE:
        case CZ_TT_FALSE:
            node = cz_parser_create_literal(parser);
            NULL_POINTER_TO_GOTO(node, error_free_node);
            break;
        case CZ_TT_IDENTIFIER:
            if (cz_parser_peek_token_type(parser, 1) == CZ_TT_LEFT_CURLY_BRACKET) {
                node = cz_parser_create_struct_init(parser);
                NULL_POINTER_TO_GOTO(node, error_free_node);
            } else {
            node = cz_parser_create_identifier(parser);
            NULL_POINTER_TO_GOTO(node, error_free_node);
            }

            //if (cz_parser_peek_token_type(parser, 0) == CZ_TT_LEFT_PARENTHESIS) {
            //    // TODO: Function Call
            //    goto error_free_node;
            //}
            break;
        case CZ_TT_LEFT_PARENTHESIS:
            token = cz_parser_consume_token(parser, CZ_TT_LEFT_PARENTHESIS);
            NULL_POINTER_TO_GOTO(token, error_free_node);

            node = cz_parser_create_expression(parser, CZ_PRECEDENCE_NONE);
            NULL_POINTER_TO_GOTO(node, error_free_node);

            token = cz_parser_consume_token(parser, CZ_TT_RIGHT_PARENTHESIS);
            if (token == NULL) {
                cz_ast_root_free(node);
                node = NULL;
                goto error_free_node;
            }
            break;
        default:
            cz_error_list_push_error(parser->error_list, parser->filename, parser->tokens[parser->idx].line, parser->tokens[parser->idx].column, "Unexpected token for primary value.");
            goto error_free_node;
    }
    return node;

error_free_node:
    cz_ast_root_free(node);
    return NULL;
}

static CZ_Token* cz_parser_consume_token(CZ_Parser* parser, CZ_TokenType expected_type) {
    if (parser == NULL) return NULL;
    if (parser->idx >= parser->n_tokens) {
        cz_error_list_push_error(parser->error_list, parser->filename, 0, 0, "Out of tokens.");
        return NULL;
    }

    if (cz_parser_peek_token_type(parser, 0) == expected_type) {
        CZ_Token* token = &parser->tokens[parser->idx];
        parser->idx++;
        return token;
    } else {
        cz_error_list_push_error(parser->error_list, parser->filename, parser->tokens[parser->idx].line, parser->tokens[parser->idx].column, "Expected token %s", cz_token_to_human_name(expected_type));
        parser->idx++;
        return NULL;
    }
}



static inline bool cz_parser_is_sync_token(CZ_TokenType token_type) {
    switch (token_type) {
        case CZ_TT_SEMICOLON:
        case CZ_TT_RIGHT_CURLY_BRACKET:
        case CZ_TT_IF:
        case CZ_TT_FUNC:
        case CZ_TT_RETURN:
        case CZ_TT_FOR:
        case CZ_TT_WHILE:
            return true;
        default:
            return false;
    }
}

static int cz_parser_sync(CZ_Parser* parser) {
    if (parser == NULL) return 0;

    if (cz_parser_peek_token_type(parser, 0) == CZ_TT_EOF) {
        return 1;
    }

    if (cz_parser_is_sync_token(cz_parser_peek_token_type(parser, 0))) {
        parser->idx++;
    }

    while (cz_parser_peek_token_type(parser, 0) != CZ_TT_EOF) {
        CZ_TokenType peeked = cz_parser_peek_token_type(parser, 0);
        if (cz_parser_is_sync_token(peeked)) {
            if (peeked == CZ_TT_SEMICOLON || peeked == CZ_TT_RIGHT_CURLY_BRACKET) {
                // For semicolon and curly bracket, one must advance once more.
                parser->idx++;
            }
            return 1;     // Found sync token; exit the function
        }
        parser->idx++;
    }

    return 1;
}
