#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "cz_lexer.h"
#include "cz_parser.h"
#include "cz_error.h"
#include "cz_semantic_analyzer.h"

// Helper function to create a lexer from source code and analyze it.
static std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> create_lexer(const char* source) {
    CZ_Lexer* lexer = cz_lexer_create(source, nullptr);
    if (!lexer) {
        return std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)>(nullptr, cz_lexer_free);
    }
    if (cz_lexer_analyze(lexer) != 1) {
        cz_lexer_free(lexer);
        return std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)>(nullptr, cz_lexer_free);
    }
    return std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)>(lexer, cz_lexer_free);
}

// Helper function to create a parser from a lexer (parser takes ownership of lexer resources).
static std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)> create_parser(std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)>& lexer_ptr) {
    CZ_Lexer* lexer = lexer_ptr.release();  // Transfer ownership to parser
    CZ_Parser* parser = cz_parser_create(lexer);
    if (!parser) {
        cz_lexer_free(lexer);  // If parser creation fails, we must free lexer
        return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(nullptr, cz_parser_free);
    }
    if (cz_parser_parse(parser) != 1) {
        cz_parser_free(parser);
        return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(nullptr, cz_parser_free);
    }
    // Parser now owns lexer's resources, so we don't free lexer separately
    return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(parser, cz_parser_free);
}

// Helper function to create a semantic analyzer from a parser (semantic analyzer takes ownership of parser resources).
static std::unique_ptr<CZ_SemanticAnalyzer, decltype(&cz_semantic_analyzer_free)> create_semantic_analyzer(std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>& parser_ptr) {
    CZ_Parser* parser = parser_ptr.release();  // Transfer ownership to semantic analyzer
    CZ_SemanticAnalyzer* sa = cz_semantic_analyzer_create(parser);
    if (!sa) {
        cz_parser_free(parser);  // If semantic analyzer creation fails, we must free parser
        return std::unique_ptr<CZ_SemanticAnalyzer, decltype(&cz_semantic_analyzer_free)>(nullptr, cz_semantic_analyzer_free);
    }
    // Semantic analyzer now owns parser's resources, so we don't free parser separately
    return std::unique_ptr<CZ_SemanticAnalyzer, decltype(&cz_semantic_analyzer_free)>(sa, cz_semantic_analyzer_free);
}

// Test Case 28: SemanticAnalyzerTest.GoodCase1_GlobalCounter
TEST(SemanticAnalyzerTest, GoodCase1_GlobalCounter) {
    const char* source =
        "global_counter :: int32 = 42;\n\n"
        "func get_counter :: () -> int32 & {\n"
        "    return global_counter;\n"
        "}";

    auto lexer_ptr = create_lexer(source);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    auto parser_ptr = create_parser(lexer_ptr);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;

    if (error_count > 0) {
        fprintf(stderr, "Semantic analysis failed with %d errors:\n", error_count);
        for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
            CZ_Error err = sa_ptr->error_list->errors[i];
            fprintf(stderr, "  Error %zu: %s at line %d, column %d\n",
                    i, err.message, err.line, err.column);
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}

// Test Case 29: SemanticAnalyzerTest.GoodCase2_ChooseLeft
TEST(SemanticAnalyzerTest, GoodCase2_ChooseLeft) {
    const char* source =
        "func choose_left :: (left :: int32&, right :: int32&) -> int32 & {"
        "    return left;\n"
        "}";

    auto lexer_ptr = create_lexer(source);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    auto parser_ptr = create_parser(lexer_ptr);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;

    if (error_count > 0) {
        fprintf(stderr, "Semantic analysis failed with %d errors:\n", error_count);
        for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
            CZ_Error err = sa_ptr->error_list->errors[i];
            fprintf(stderr, "  Error %zu: %s at line %d, column %d\n",
                    i, err.message, err.line, err.column);
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}

// Test Case 30: SemanticAnalyzerTest.BadCase3_InvalidLocalReferenceReturn
TEST(SemanticAnalyzerTest, BadCase3_InvalidLocalReferenceReturn) {
    const char* source =
        "func bad_leak :: () -> int32 & {\n"
        "    local_val :: int32 = 100;\n"
        "    return local_val;\n"
        "}";

    auto lexer_ptr = create_lexer(source);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    auto parser_ptr = create_parser(lexer_ptr);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;

    if (error_count > 0) {
        fprintf(stderr, "Semantic analysis failed with %d errors:\n", error_count);
        for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
            CZ_Error err = sa_ptr->error_list->errors[i];
            fprintf(stderr, "  Error %zu: %s at line %d, column %d\n",
                    i, err.message, err.line, err.column);
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}

// Test Case 31: SemanticAnalyzerTest.BadCase4_InvalidDeeplyNestedLocalReferenceReturn
TEST(SemanticAnalyzerTest, BadCase4_InvalidDeeplyNestedLocalReferenceReturn) {
    const char* source =
        "func complex_leak :: (condition :: bool) -> int32 & {\n"
        "    if (condition) {\n"
        "        nested_val :: int32 = 5;\n"
        "        return nested_val;\n"
        "    }\n"
        "    global_backup :: int32 = 0;\n"
        "    return global_backup;\n"
        "}";

    auto lexer_ptr = create_lexer(source);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    auto parser_ptr = create_parser(lexer_ptr);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;

    if (error_count > 0) {
        fprintf(stderr, "Semantic analysis failed with %d errors:\n", error_count);
        for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
            CZ_Error err = sa_ptr->error_list->errors[i];
            fprintf(stderr, "  Error %zu: %s at line %d, column %d\n",
                    i, err.message, err.line, err.column);
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}