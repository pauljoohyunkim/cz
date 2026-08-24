#include <stdio.h>
#include <stdlib.h>
#include "cz_ast.h"

CZ_AST_Decoration* cz_ast_decoration_create(const CZ_Type* type, CZ_ValueCategory val_category, bool is_constexpr, bool is_reference_source, unsigned int scope_level) {
    CZ_AST_Decoration* decor = NULL;
    if (type == NULL) return NULL;

    decor = (CZ_AST_Decoration*) calloc(1, sizeof(CZ_AST_Decoration));
    if (decor == NULL) return NULL;

    decor->resolved_type = type;
    decor->value_category = val_category;
    decor->is_constexpr = is_constexpr;
    decor->is_reference_source = is_reference_source;
    decor->scope_level = scope_level;

    return decor;
}

void cz_ast_decoration_free(CZ_AST_Decoration* decor) {
    // Note that internal CZ_Type* should not be freed.
    free(decor);
}

CZ_AST_Node* cz_ast_node_create(CZ_AST_NodeType node_type, unsigned int line, unsigned int col) {
    CZ_AST_Node* node = (CZ_AST_Node*) calloc(1, sizeof(CZ_AST_Node));
    if (node == NULL) return NULL;

    node->node_type = node_type;
    node->line = line;
    node->col = col;

    return node;
}

void cz_ast_root_free(CZ_AST_Node* node) {
    if (node != NULL) {
        switch (node->node_type) {
            case CZ_AST_ProgramNodeType:
                for (unsigned int i = 0; i < node->program.declaration_count; i++) {
                    cz_ast_root_free(node->program.global_declaration_list[i]);
                }
                free(node->program.global_declaration_list);
                break;
            case CZ_AST_StructDeclarationNodeType:
            case CZ_AST_StructInitNodeType:
                cz_ast_root_free(node->struct_declaration.identifier);
                for (unsigned int i = 0; i < node->struct_declaration.member_count; i++) {
                    cz_ast_root_free(node->struct_declaration.members[i]);
                }
                free(node->struct_declaration.members);
                break;
            case CZ_AST_StructInitMemberNodeType:
                cz_ast_root_free(node->struct_init_member.identifier);
                cz_ast_root_free(node->struct_init_member.expression);
                break;
            case CZ_AST_ArrayInitNodeType:
                for (unsigned int i = 0; i < node->array_init.element_count; i++) {
                    cz_ast_root_free(node->array_init.elements[i]);
                }
                free(node->array_init.elements);
                break;
            case CZ_AST_FunctionDeclarationNodeType:
                cz_ast_root_free(node->function_declaration.function_identifier);
                cz_ast_root_free(node->function_declaration.function.parameter_list);
                cz_ast_root_free(node->function_declaration.function.return_type);
                cz_ast_root_free(node->function_declaration.body);
                break;
            case CZ_AST_ParameterListNodeType:
                for (unsigned int i = 0; i < node->parameter_list.param_count; i++) {
                    cz_ast_root_free(node->parameter_list.params[i]);
                }
                free(node->parameter_list.params);
                cz_environment_free(node->parameter_list.scope);
                break;
            case CZ_AST_BlockStatementNodeType:
                for (unsigned int i = 0; i < node->statement_list.statement_count; i++) {
                    cz_ast_root_free(node->statement_list.statements[i]);
                }
                free(node->statement_list.statements);
                cz_environment_free(node->statement_list.scope);
                break;
            case CZ_AST_VariableDeclarationNodeType:
                cz_ast_root_free(node->variable_declaration.identifier);
                cz_ast_root_free(node->variable_declaration.type);
                cz_ast_root_free(node->variable_declaration.expression);
                break;
            case CZ_AST_TypedefDeclarationNodeType:
            case CZ_AST_NewtypeDeclarationNodeType:
                cz_ast_root_free(node->typedef_declaration.type);
                cz_ast_root_free(node->typedef_declaration.new_type);
                break;
            case CZ_AST_ReturnStatementNodeType:
                cz_ast_root_free(node->return_statement.expression);
                break;
            case CZ_AST_IfStatementNodeType:
                cz_ast_root_free(node->if_statement.condition);
                cz_ast_root_free(node->if_statement.if_branch);
                cz_ast_root_free(node->if_statement.else_branch);
                break;
            case CZ_AST_ForStatementNodeType:
                cz_ast_root_free(node->for_statement.initialization);
                cz_ast_root_free(node->for_statement.condition);
                cz_ast_root_free(node->for_statement.iteration_step);
                cz_ast_root_free(node->for_statement.body);
                cz_environment_free(node->for_statement.scope);
                break;
            case CZ_AST_WhileStatementNodeType:
                cz_ast_root_free(node->while_statement.condition);
                cz_ast_root_free(node->while_statement.body);
                break;
            case CZ_AST_TypeNodeType:
                if (node->type_expression.is_function_type == true) {
                    // Function Signature
                    cz_ast_root_free(node->type_expression.function_signature.parameter_list);
                    cz_ast_root_free(node->type_expression.function_signature.return_type);
                }
                if (node->type_expression.is_array == true || node->type_expression.is_list == true) {
                    // Element types and size expression
                    cz_ast_root_free(node->type_expression.array.element_type);
                    cz_ast_root_free(node->type_expression.array.size_expr);
                }
                break;
            case CZ_AST_BinaryExpressionNodeType:
            case CZ_AST_AssignmentStatementNodeType:
                cz_ast_root_free(node->binary_expression.left);
                cz_ast_root_free(node->binary_expression.right);
                break;
            case CZ_AST_UnaryExpressionNodeType:
                cz_ast_root_free(node->unary_expression.operand);
                break;
            case CZ_AST_FunctionCallNodeType:
                cz_ast_root_free(node->function_call.callee);
                for (unsigned int i = 0; i < node->function_call.arg_count; i++) {
                    cz_ast_root_free(node->function_call.arguments[i]);
                }
                free(node->function_call.arguments);
                break;
            case CZ_AST_LiteralNodeType:
                break;
            case CZ_AST_IdentifierNodeType:
                break;
            case CZ_AST_CastExpressionNodeType:
                cz_ast_root_free(node->cast_expression.expression);
                cz_ast_root_free(node->cast_expression.type);
                break;
            case CZ_AST_StructMemberAccessNodeType:
            case CZ_AST_ArrayListElementAccessNodeType:
                cz_ast_root_free(node->member_access.object);
                cz_ast_root_free(node->member_access.member);
                break;
        }
        cz_ast_decoration_free(node->decoration);
    }
    free(node);
}

void cz_ast_root_print(const CZ_AST_Node* node, unsigned int depth) {
    if (node == NULL) return;

    // Indent depth number of times.
    for (unsigned int i = 0; i < depth; i++) {
        printf("\t");
    }

    switch (node->node_type) {
        case CZ_AST_ProgramNodeType:
            printf("Program\n");
            for (unsigned int i = 0; i < node->program.declaration_count; i++) {
                cz_ast_root_print(node->program.global_declaration_list[i], depth+1);
            }
            break;
        case CZ_AST_StructDeclarationNodeType:
            printf("StructDeclaration\n");
            cz_ast_root_print(node->struct_declaration.identifier, depth+1);
            for (unsigned int i = 0; i < node->struct_declaration.member_count; i++) {
                cz_ast_root_print(node->struct_declaration.members[i], depth+1);
            }
            break;
        case CZ_AST_StructInitNodeType:
            printf("StructInit\n");
            cz_ast_root_print(node->struct_declaration.identifier, depth+1);
            for (unsigned int i = 0; i < node->struct_declaration.member_count; i++) {
                cz_ast_root_print(node->struct_declaration.members[i], depth+1);
            }
            break;
        case CZ_AST_StructInitMemberNodeType:
            printf("StructInit\n");
            cz_ast_root_print(node->struct_init_member.identifier, depth+1);
            cz_ast_root_print(node->struct_init_member.expression, depth+1);
            break;
        case CZ_AST_ArrayInitNodeType:
            printf("ArrayInit\n");
            for (unsigned int i = 0; i < node->array_init.element_count; i++) {
                cz_ast_root_print(node->array_init.elements[i], depth+1);
            }
            break;
        case CZ_AST_FunctionDeclarationNodeType:
            printf("FunctionDeclaration\n");
            cz_ast_root_print(node->function_declaration.function_identifier, depth+1);
            cz_ast_root_print(node->function_declaration.function.parameter_list, depth+1);
            cz_ast_root_print(node->function_declaration.function.return_type, depth+1);
            cz_ast_root_print(node->function_declaration.body, depth+1);
            break;
        case CZ_AST_ParameterListNodeType:
            printf("ParameterList\n");
            for (unsigned int i = 0; i < node->parameter_list.param_count; i++) {
                cz_ast_root_print(node->parameter_list.params[i], depth+1);
            }
            break;
        case CZ_AST_BlockStatementNodeType:
            printf("BlockStatement\n");
            for (unsigned int i = 0; i < node->statement_list.statement_count; i++) {
                cz_ast_root_print(node->statement_list.statements[i], depth+1);
            }
            break;
        case CZ_AST_VariableDeclarationNodeType:
            //printf("VariableDeclaration%s\n", node->variable_declaration.is_const ? " (const)" : "");
            printf("VariableDeclaration\n");
            cz_ast_root_print(node->variable_declaration.identifier, depth+1);
            cz_ast_root_print(node->variable_declaration.type, depth+1);
            cz_ast_root_print(node->variable_declaration.expression, depth+1);
            break;
        case CZ_AST_AssignmentStatementNodeType:
            printf("AssignmentStatement: ");
            switch (node->binary_expression.op) {
                case CZ_TT_GREATER:
                    printf(">\n");
                    break;
                case CZ_TT_LESS:
                    printf("<\n");
                    break;
                case CZ_TT_GREATER_EQUAL:
                    printf(">=\n");
                    break;
                case CZ_TT_LESS_EQUAL:
                    printf("<=\n");
                    break;
                case CZ_TT_AMPERSAND:
                    printf("&\n");
                    break;
                case CZ_TT_BAR:
                    printf("|\n");
                    break;
                case CZ_TT_PLUS:
                    printf("+\n");
                    break;
                case CZ_TT_MINUS:
                    printf("-\n");
                    break;
                case CZ_TT_STAR:
                    printf("*\n");
                    break;
                case CZ_TT_SLASH:
                    printf("/\n");
                    break;
                case CZ_TT_PERCENT:
                    printf("%%\n");
                    break;
                case CZ_TT_EQUAL:
                    printf("=\n");
                    break;
                case CZ_TT_PLUS_EQUAL:
                    printf("+=\n");
                    break;
                case CZ_TT_MINUS_EQUAL:
                    printf("-=\n");
                    break;
                case CZ_TT_STAR_EQUAL:
                    printf("*=\n");
                    break;
                case CZ_TT_SLASH_EQUAL:
                    printf("/=\n");
                    break;
                case CZ_TT_PERCENT_EQUAL:
                    printf("%%=\n");
                    break;
                case CZ_TT_AMPERSAND_EQUAL:
                    printf("&=\n");
                    break;
                /* case CZ_TT_CARET_EQ: */ /* not defined in cz_tokens.h */
                /*     printf("^=\n"); */
                /*     break; */
                case CZ_TT_BAR_EQUAL:
                    printf("|=\n");
                    break;
                /* case CZ_TT_LSHIFT_EQ: */ /* not defined in cz_tokens.h */
                /*     printf("<<=\n"); */
                /*     break; */
                /* case CZ_TT_RSHIFT_EQ: */ /* not defined in cz_tokens.h */
                /*     printf(">>=\n"); */
                /*     break; */
                default:
                    printf("?\n");
                    break;
            }
            cz_ast_root_print(node->binary_expression.left, depth+1);
            cz_ast_root_print(node->binary_expression.right, depth+1);
            break;
        case CZ_AST_ReturnStatementNodeType:
            printf("ReturnStatement\n");
            cz_ast_root_print(node->return_statement.expression, depth+1);
            break;
        case CZ_AST_IfStatementNodeType:
            printf("IfStatement\n");
            cz_ast_root_print(node->if_statement.condition, depth+1);
            cz_ast_root_print(node->if_statement.if_branch, depth+1);
            if (node->if_statement.else_branch != NULL) {
                cz_ast_root_print(node->if_statement.else_branch, depth+1);
            }
            break;
        case CZ_AST_ForStatementNodeType:
            printf("ForStatement\n");
            if (node->for_statement.initialization != NULL) {
                cz_ast_root_print(node->for_statement.initialization, depth+1);
            }
            if (node->for_statement.condition != NULL) {
                cz_ast_root_print(node->for_statement.condition, depth+1);
            }
            if (node->for_statement.iteration_step != NULL) {
                cz_ast_root_print(node->for_statement.iteration_step, depth+1);
            }
            cz_ast_root_print(node->for_statement.body, depth+1);
            break;
        case CZ_AST_WhileStatementNodeType:
            printf("WhileStatement\n");
            cz_ast_root_print(node->while_statement.condition, depth+1);
            cz_ast_root_print(node->while_statement.body, depth+1);
            break;
        case CZ_AST_TypedefDeclarationNodeType:
            printf("TypedefDeclaration\n");
            cz_ast_root_print(node->typedef_declaration.type, depth+1);
            cz_ast_root_print(node->typedef_declaration.new_type, depth+1);
            break;
        case CZ_AST_NewtypeDeclarationNodeType:
            printf("NewtypeDeclaration\n");
            cz_ast_root_print(node->typedef_declaration.type, depth+1);
            cz_ast_root_print(node->typedef_declaration.new_type, depth+1);
            break;
        case CZ_AST_TypeNodeType:
            if (node->type_expression.is_function_type) {
                printf("TypeNode (function)\n");
                cz_ast_root_print(node->type_expression.function_signature.parameter_list, depth+1);
                cz_ast_root_print(node->type_expression.function_signature.return_type, depth+1);
            } else if (node->type_expression.is_array) {
                printf("TypeNode (array)\n");
                cz_ast_root_print(node->type_expression.array.element_type, depth+1);
                cz_ast_root_print(node->type_expression.array.size_expr, depth+1);
            } else if (node->type_expression.is_list) {
                printf("TypeNode (list)\n");
                cz_ast_root_print(node->type_expression.array.element_type, depth+1);
            } else {
                printf("TypeNode: %s %s\n", node->type_expression.is_const ? "(const)" : "", node->type_expression.primitive.name);
            }
            break;
        case CZ_AST_BinaryExpressionNodeType:
            printf("BinaryExpression: ");
            switch (node->binary_expression.op) {
                case CZ_TT_GREATER:
                    printf(">\n");
                    break;
                case CZ_TT_LESS:
                    printf("<\n");
                    break;
                case CZ_TT_GREATER_EQUAL:
                    printf(">=\n");
                    break;
                case CZ_TT_LESS_EQUAL:
                    printf("<=\n");
                    break;
                case CZ_TT_AMPERSAND:
                    printf("&\n");
                    break;
                case CZ_TT_BAR:
                    printf("|\n");
                    break;
                case CZ_TT_PLUS:
                    printf("+\n");
                    break;
                case CZ_TT_MINUS:
                    printf("-\n");
                    break;
                case CZ_TT_STAR:
                    printf("*\n");
                    break;
                case CZ_TT_SLASH:
                    printf("/\n");
                    break;
                case CZ_TT_PERCENT:
                    printf("%%\n");
                    break;
                case CZ_TT_EQUAL_EQUAL:
                    printf("==\n");
                    break;
                case CZ_TT_EXCLAMATION_EQUAL:
                    printf("!=\n");
                    break;
                case CZ_TT_AMPERSAND_AMPERSAND:
                    printf("&&\n");
                    break;
                case CZ_TT_BAR_BAR:
                    printf("||\n");
                    break;
                default:
                    return;
            }
            cz_ast_root_print(node->binary_expression.left, depth+1);
            cz_ast_root_print(node->binary_expression.right, depth+1);
            break;
        case CZ_AST_UnaryExpressionNodeType:
            printf("UnaryExpression: ");
            if (node->unary_expression.op == CZ_TT_MINUS) {
                printf("-\n");
            } else if (node->unary_expression.op == CZ_TT_EXCLAMATION) {
                printf("!\n");
            } else if (node->unary_expression.op == CZ_TT_AMPERSAND) {
                printf("&\n");
            } else {
                return;
            }
            cz_ast_root_print(node->unary_expression.operand, depth+1);
            break;
        case CZ_AST_FunctionCallNodeType:
            printf("FunctionCall\n");
            cz_ast_root_print(node->function_call.callee, depth+1);
            for (unsigned int i = 0; i < node->function_call.arg_count; i++) {
                cz_ast_root_print(node->function_call.arguments[i], depth+1);
            }
            break;
        case CZ_AST_LiteralNodeType:
            printf("Literal: %s\n", node->literal.lexeme);
            break;
        case CZ_AST_IdentifierNodeType:
            printf("Identifier: %s\n", node->identifier.name);
            break;
        case CZ_AST_CastExpressionNodeType:
            printf("CastExpression\n");
            cz_ast_root_print(node->cast_expression.expression, depth+1);
            cz_ast_root_print(node->cast_expression.type, depth+1);
            break;
        case CZ_AST_StructMemberAccessNodeType:
        case CZ_AST_ArrayListElementAccessNodeType:
            printf(node->node_type == CZ_AST_StructMemberAccessNodeType ? "StructMemberAccess\n" : "ArrayListElementAccess\n");
            cz_ast_root_print(node->member_access.object, depth+1);
            cz_ast_root_print(node->member_access.member, depth+1);
            break;
    }
}
