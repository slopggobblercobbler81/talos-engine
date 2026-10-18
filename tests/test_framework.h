// Minimal dependency-free test harness (TEST, CHECK, CHECK_NEAR, CHECK_EQ)
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// Minimal dependency-free test harness: register cases with TEST(), assert with CHECK*().
namespace TestFramework {

    struct TestCase {
        const char* name;
        std::function<void()> fn;
    };

    inline std::vector<TestCase>& Registry() {
        static std::vector<TestCase> cases;
        return cases;
    }

    inline int& FailureCount() {
        static int failures = 0;
        return failures;
    }

    struct Registrar {
        Registrar(const char* name, std::function<void()> fn) { Registry().push_back({ name, std::move(fn) }); }
    };

    inline int RunAll() {
        int failedCases = 0;
        for (const auto& c : Registry()) {
            int before = FailureCount();
            c.fn();
            bool ok = (FailureCount() == before);
            std::printf("[%s] %s\n", ok ? " OK " : "FAIL", c.name);
            if (!ok) ++failedCases;
        }
        std::printf("\n%zu test(s), %d failed\n", Registry().size(), failedCases);
        return failedCases == 0 ? 0 : 1;
    }

}

#define TEST(name) \
    static void name(); \
    static TestFramework::Registrar registrar_##name(#name, name); \
    static void name()

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("    %s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++TestFramework::FailureCount(); \
        } \
    } while (0)

#define CHECK_NEAR(a, b, eps) \
    do { \
        double va_ = static_cast<double>(a), vb_ = static_cast<double>(b); \
        if (std::fabs(va_ - vb_) > (eps)) { \
            std::printf("    %s:%d: CHECK_NEAR failed: %s (%g) vs %s (%g)\n", \
                        __FILE__, __LINE__, #a, va_, #b, vb_); \
            ++TestFramework::FailureCount(); \
        } \
    } while (0)

#define CHECK_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            std::printf("    %s:%d: CHECK_EQ failed: %s vs %s\n", __FILE__, __LINE__, #a, #b); \
            ++TestFramework::FailureCount(); \
        } \
    } while (0)

#define TEST_MAIN() \
    int main() { return TestFramework::RunAll(); }
