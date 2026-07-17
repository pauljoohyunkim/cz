#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "cz_lexer.h"

TEST(Lexer, Analysis_Length_1) {
    auto lexer { cz_lexer_create("( { ( [ (  \n )]))}", nullptr) };
    std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> lexer_ptr { lexer, cz_lexer_free };     // In case of failure, C++ smart pointer will call the free function.

    ASSERT_EQ(cz_lexer_analyze(lexer), 1);

    ASSERT_EQ(lexer->n_tokens, 11);

    ASSERT_EQ(lexer->tokens[0].token_type, CZ_TT_LEFT_PARENTHESIS);
    ASSERT_EQ(lexer->tokens[1].token_type, CZ_TT_LEFT_CURLY_BRACKET);
    ASSERT_EQ(lexer->tokens[2].token_type, CZ_TT_LEFT_PARENTHESIS);
    ASSERT_EQ(lexer->tokens[3].token_type, CZ_TT_LEFT_SQUARE_BRACKET);
    ASSERT_EQ(lexer->tokens[4].token_type, CZ_TT_LEFT_PARENTHESIS);
    ASSERT_EQ(lexer->tokens[5].token_type, CZ_TT_RIGHT_PARENTHESIS);
    ASSERT_EQ(lexer->tokens[6].token_type, CZ_TT_RIGHT_SQUARE_BRACKET);
    ASSERT_EQ(lexer->tokens[7].token_type, CZ_TT_RIGHT_PARENTHESIS);
    ASSERT_EQ(lexer->tokens[8].token_type, CZ_TT_RIGHT_PARENTHESIS);
    ASSERT_EQ(lexer->tokens[9].token_type, CZ_TT_RIGHT_CURLY_BRACKET);
    ASSERT_EQ(lexer->tokens[10].token_type, CZ_TT_EOF);

    for (size_t i = 0; i < 10; i++) {
        ASSERT_EQ(strlen(lexer->tokens[i].lexeme), 1);
    }
    for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ(lexer->tokens[i].line, 1);
    }
    for (size_t i = 5; i < lexer->n_tokens; i++) {
        ASSERT_EQ(lexer->tokens[i].line, 2);
    }
    ASSERT_EQ(lexer->tokens[0].column, 1);
    ASSERT_EQ(lexer->tokens[1].column, 3);
    ASSERT_EQ(lexer->tokens[2].column, 5);
    ASSERT_EQ(lexer->tokens[3].column, 7);
    ASSERT_EQ(lexer->tokens[4].column, 9);
    ASSERT_EQ(lexer->tokens[5].column, 2);
    ASSERT_EQ(lexer->tokens[6].column, 3);
    ASSERT_EQ(lexer->tokens[7].column, 4);
    ASSERT_EQ(lexer->tokens[8].column, 5);
    ASSERT_EQ(lexer->tokens[9].column, 6);

    ASSERT_TRUE(lexer->n_tokens <= lexer->n_tokens_capacity);
}

TEST(Lexer, Analysis_Comment) {
    auto lexer { cz_lexer_create("- == // %@13 +- == \n//\n->", nullptr) };
    std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> lexer_ptr { lexer, cz_lexer_free };     // In case of failure, C++ smart pointer will call the free function.

    ASSERT_EQ(cz_lexer_analyze(lexer), 1);

    ASSERT_EQ(lexer->n_tokens, 4);
    ASSERT_EQ(lexer->tokens[0].token_type, CZ_TT_MINUS);
    ASSERT_EQ(lexer->tokens[1].token_type, CZ_TT_EQUAL_EQUAL);
    ASSERT_EQ(lexer->tokens[2].token_type, CZ_TT_RIGHT_ARROW);
    ASSERT_EQ(lexer->tokens[3].token_type, CZ_TT_EOF);

    ASSERT_TRUE(lexer->n_tokens <= lexer->n_tokens_capacity);
}

TEST(Lexer, Analysis_Equal) {
    auto lexer { cz_lexer_create("= ==", nullptr) };
    std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> lexer_ptr { lexer, cz_lexer_free };     // In case of failure, C++ smart pointer will call the free function.

    ASSERT_EQ(cz_lexer_analyze(lexer), 1);

    ASSERT_EQ(lexer->n_tokens, 3);
    ASSERT_EQ(lexer->tokens[0].token_type, CZ_TT_EQUAL);
    ASSERT_EQ(lexer->tokens[1].token_type, CZ_TT_EQUAL_EQUAL);
    ASSERT_EQ(lexer->tokens[2].token_type, CZ_TT_EOF);

    ASSERT_TRUE(lexer->n_tokens <= lexer->n_tokens_capacity);
}

TEST(Lexer, Keywords) {
    auto lexer { cz_lexer_create("return func int32", nullptr) };
    std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> lexer_ptr { lexer, cz_lexer_free };     // In case of failure, C++ smart pointer will call the free function.

    ASSERT_EQ(cz_lexer_analyze(lexer), 1);

    ASSERT_EQ(lexer->n_tokens, 4);
    ASSERT_EQ(lexer->tokens[0].token_type, CZ_TT_RETURN);
    ASSERT_EQ(lexer->tokens[1].token_type, CZ_TT_FUNC);
    ASSERT_EQ(lexer->tokens[2].token_type, CZ_TT_INT32);
    ASSERT_EQ(lexer->tokens[3].token_type, CZ_TT_EOF);

    ASSERT_TRUE(lexer->n_tokens <= lexer->n_tokens_capacity);
}

TEST(Lexer, Identifiers) {
    auto lexer { cz_lexer_create("return1 inv func30j2", nullptr) };
    std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> lexer_ptr { lexer, cz_lexer_free };     // In case of failure, C++ smart pointer will call the free function.

    ASSERT_EQ(cz_lexer_analyze(lexer), 1);

    ASSERT_EQ(lexer->n_tokens, 4);
    ASSERT_EQ(lexer->tokens[0].token_type, CZ_TT_IDENTIFIER);
    ASSERT_EQ(lexer->tokens[1].token_type, CZ_TT_IDENTIFIER);
    ASSERT_EQ(lexer->tokens[2].token_type, CZ_TT_IDENTIFIER);
    ASSERT_EQ(lexer->tokens[3].token_type, CZ_TT_EOF);

    ASSERT_EQ(std::string(lexer->tokens[0].lexeme), "return1");
    ASSERT_EQ(std::string(lexer->tokens[1].lexeme), "inv");
    ASSERT_EQ(std::string(lexer->tokens[2].lexeme), "func30j2");

    ASSERT_TRUE(lexer->n_tokens <= lexer->n_tokens_capacity);
}

TEST(Lexer, NumericLiterals) {
    auto lexer { cz_lexer_create("0 1.0 3.", nullptr) };
    std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> lexer_ptr { lexer, cz_lexer_free };     // In case of failure, C++ smart pointer will call the free function.

    ASSERT_EQ(cz_lexer_analyze(lexer), 1);

    ASSERT_EQ(lexer->n_tokens, 4);
    ASSERT_EQ(lexer->tokens[0].token_type, CZ_TT_NUMERICAL_LITERAL);
    ASSERT_EQ(lexer->tokens[1].token_type, CZ_TT_NUMERICAL_LITERAL);
    ASSERT_EQ(lexer->tokens[2].token_type, CZ_TT_NUMERICAL_LITERAL);
    ASSERT_EQ(lexer->tokens[3].token_type, CZ_TT_EOF);

    ASSERT_TRUE(lexer->n_tokens <= lexer->n_tokens_capacity);
}

TEST(Lexer, Unknown) {
    auto lexer { cz_lexer_create("a bd ~ `\n+", nullptr) };
    std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> lexer_ptr { lexer, cz_lexer_free };     // In case of failure, C++ smart pointer will call the free function.

    ASSERT_EQ(cz_lexer_analyze(lexer), 1);

    ASSERT_EQ(lexer->n_tokens, 5);
    ASSERT_EQ(lexer->tokens[0].token_type, CZ_TT_IDENTIFIER);
    ASSERT_EQ(lexer->tokens[1].token_type, CZ_TT_IDENTIFIER);
    ASSERT_EQ(lexer->tokens[2].token_type, CZ_TT_UNKNOWN);
    ASSERT_EQ(lexer->tokens[3].token_type, CZ_TT_PLUS);
    ASSERT_EQ(lexer->tokens[4].token_type, CZ_TT_EOF);

    ASSERT_EQ(lexer->error_list->n_errors, 1);

    ASSERT_TRUE(lexer->n_tokens <= lexer->n_tokens_capacity);
}
