#pragma once

#include <iostream>
#include <string>

inline int g_test_failures = 0;

inline void report_failure(const std::string& expr, const std::string& msg, const char* file, int line) {
    ++g_test_failures;
    std::cerr << file << ":" << line << ": check failed: " << expr;
    if (!msg.empty())
        std::cerr << " (" << msg << ")";
    std::cerr << std::endl;
}

#define TEST_EXPECT_TRUE(expr)                                                                                                                                        \
    do {                                                                                                                                                              \
        if (!(expr))                                                                                                                                                  \
            report_failure(#expr, "", __FILE__, __LINE__);                                                                                                            \
    } while (0)

#define TEST_EXPECT_EQ(lhs, rhs)                                                                                                                                      \
    do {                                                                                                                                                              \
        auto _lhs = (lhs);                                                                                                                                            \
        auto _rhs = (rhs);                                                                                                                                            \
        if (!(_lhs == _rhs)) {                                                                                                                                        \
            report_failure(#lhs " == " #rhs, std::to_string(_lhs) + " vs " + std::to_string(_rhs), __FILE__, __LINE__);                                               \
        }                                                                                                                                                             \
    } while (0)

#define TEST_EXPECT_STR_EQ(lhs, rhs)                                                                                                                                  \
    do {                                                                                                                                                              \
        auto _lhs = (lhs);                                                                                                                                            \
        auto _rhs = (rhs);                                                                                                                                            \
        if (!(_lhs == _rhs)) {                                                                                                                                        \
            report_failure(#lhs " == " #rhs, std::string(_lhs) + " vs " + std::string(_rhs), __FILE__, __LINE__);                                                     \
        }                                                                                                                                                             \
    } while (0)

#define TEST_EXPECT_FALSE(expr) TEST_EXPECT_TRUE(!(expr))

#define TEST_ASSERT_OK()                                                                                                                                              \
    do {                                                                                                                                                              \
        return g_test_failures;                                                                                                                                       \
    } while (0)
