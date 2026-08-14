#include "compile_time_ut.hpp"

#include <cstdio>
#include <cstring>

using namespace CompileTimeUnitTesting;

// =====================================================================
// Compile-time tests: all levels are always checked at compile time
// =====================================================================

constexpr bool test_preconditions_pass() {
    precondition_safe(true);
    precondition_safe(1 > 0);
    precondition(true);
    precondition(42 > 0);
    precondition_aggressive(true);
    precondition_aggressive(2 + 2 == 4);
    return true;
}

constexpr bool test_postconditions_pass() {
    postcondition_safe(true);
    postcondition_safe(1 > 0);
    postcondition(true);
    postcondition(42 > 0);
    postcondition_aggressive(true);
    postcondition_aggressive(2 + 2 == 4);
    return true;
}

constexpr bool test_invariants_pass() {
    invariant_safe(true);
    invariant_safe(1 > 0);
    invariant(true);
    invariant(42 > 0);
    invariant_aggressive(true);
    invariant_aggressive(2 + 2 == 4);
    return true;
}

static_assert(test_preconditions_pass());
static_assert(test_postconditions_pass());
static_assert(test_invariants_pass());

// Uncomment any of these to verify compile-time failure:
// static_assert((precondition_safe(false), true));
// static_assert((precondition(false), true));
// static_assert((precondition_aggressive(false), true));
// static_assert((postcondition(false), true));
// static_assert((invariant(false), true));

// =====================================================================
// Runtime tests
// =====================================================================

static int test_count = 0;
static int fail_count = 0;

static void check(bool cond, const char* name) {
    ++test_count;
    if (!cond) {
        ++fail_count;
        std::fprintf(stderr, "FAIL: %s\n", name);
    }
}

// State for the violation handler tests.
static const char* last_violation = nullptr;
static const char* last_file = nullptr;
static int last_line = 0;

static void recording_handler(const char* kind, const char* file, int line) {
    last_violation = kind;
    last_file = file;
    last_line = line;
}

int main() {
    // ------------------------------------------------------------------
    // At default CTUT_ASSERTION_LEVEL = 2 (normal):
    //   safe (1)       — active at runtime
    //   normal (2)     — active at runtime
    //   aggressive (3) — compiled away (no-op at runtime)
    // ------------------------------------------------------------------

    // --- Preconditions ---

    check(expect_violation([] { precondition_safe(false); },
                            "precondition_safe violated"),
          "precondition_safe(false) violates");

    check(expect_violation([] { precondition(false); },
                            "precondition violated"),
          "precondition(false) violates");

    check(expect_no_throw([] { precondition_aggressive(false); }),
          "precondition_aggressive(false) is no-op at level 2");

    check(expect_no_throw([] { precondition_safe(true); }),
          "precondition_safe(true) passes");

    check(expect_no_throw([] { precondition(true); }),
          "precondition(true) passes");

    check(expect_no_throw([] { precondition_aggressive(true); }),
          "precondition_aggressive(true) passes");

    // --- Postconditions ---

    check(expect_violation([] { postcondition_safe(false); },
                            "postcondition_safe violated"),
          "postcondition_safe(false) violates");

    check(expect_violation([] { postcondition(false); },
                            "postcondition violated"),
          "postcondition(false) violates");

    check(expect_no_throw([] { postcondition_aggressive(false); }),
          "postcondition_aggressive(false) is no-op at level 2");

    check(expect_no_throw([] { postcondition_safe(true); }),
          "postcondition_safe(true) passes");

    // --- Invariants ---

    check(expect_violation([] { invariant_safe(false); },
                            "invariant_safe violated"),
          "invariant_safe(false) violates");

    check(expect_violation([] { invariant(false); },
                            "invariant violated"),
          "invariant(false) violates");

    check(expect_no_throw([] { invariant_aggressive(false); }),
          "invariant_aggressive(false) is no-op at level 2");

    check(expect_no_throw([] { invariant_safe(true); }),
          "invariant_safe(true) passes");

    // --- expect_no_violation (positive-path helper) ---

    check(expect_no_violation([] { precondition(true); }),
          "expect_no_violation passes through a satisfied precondition");

    // --- Violation handler ---

    set_violation_handler(recording_handler);

    last_violation = nullptr;
    last_file = nullptr;
    last_line = 0;
    try { precondition(false); } catch (...) {}
    check(last_violation != nullptr, "handler called on precondition violation");
    check(last_violation && std::strcmp(last_violation, "precondition violated") == 0,
          "handler receives correct precondition kind");
#ifdef CTUT_HAS_SOURCE_LOCATION
    check(last_file != nullptr && std::strstr(last_file, "precondition_tests.cpp") != nullptr,
          "handler receives the caller's file when source_location is available");
    check(last_line == __LINE__ - 7,
          "handler receives the caller's line when source_location is available");
#endif

    last_violation = nullptr;
    try { postcondition_safe(false); } catch (...) {}
    check(last_violation != nullptr, "handler called on postcondition violation");
    check(last_violation && std::strcmp(last_violation, "postcondition_safe violated") == 0,
          "handler receives correct postcondition kind");

    last_violation = nullptr;
    try { invariant(false); } catch (...) {}
    check(last_violation != nullptr, "handler called on invariant violation");
    check(last_violation && std::strcmp(last_violation, "invariant violated") == 0,
          "handler receives correct invariant kind");

    // Handler is NOT called for levels above configured_level
    last_violation = nullptr;
    try { precondition_aggressive(false); } catch (...) {}
    check(last_violation == nullptr,
          "handler not called for aggressive at level 2");

    // Throw still happens after handler
    set_violation_handler(recording_handler);
    check(expect_violation([] { precondition(false); }, "precondition violated"),
          "throw still happens after handler");

    // Reset handler
    set_violation_handler(nullptr);
    last_violation = nullptr;
    try { precondition(false); } catch (...) {}
    check(last_violation == nullptr,
          "null handler means no callback");

    // --- Summary ---
    std::printf("%d/%d tests passed\n", test_count - fail_count, test_count);
    return fail_count > 0 ? 1 : 0;
}
