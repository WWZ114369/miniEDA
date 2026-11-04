#pragma once

#include <exception>
#include <iostream>
#include <string>
#include <utility>

namespace test
{
inline int failures = 0;

inline void expect(bool condition, const char* expression, const char* file, int line)
{
    if (!condition)
    {
        ++failures;
        std::cerr << file << ':' << line
                  << ": check failed: " << expression << '\n';
    }
}

template <typename Function>
void run(const char* name, Function&& function)
{
    const int failures_before = failures;
    try
    {
        std::forward<Function>(function)();
    }
    catch (const std::exception& error)
    {
        ++failures;
        std::cerr << name << ": unexpected exception: " << error.what() << '\n';
    }
    catch (...)
    {
        ++failures;
        std::cerr << name << ": unexpected non-standard exception\n";
    }

    if (failures == failures_before)
    {
        std::cout << "[PASS] " << name << '\n';
    }
}
} // namespace test

#define TEST_EXPECT(expression) \
    ::test::expect(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
