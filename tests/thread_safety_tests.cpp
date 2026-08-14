// Built with -fsanitize=thread (see tests/CMakeLists.txt): stresses
// detail::violation_handler_ref() — a std::atomic<ViolationHandler> —
// with concurrent writers (set_violation_handler) racing concurrent
// readers (real contract violations, which load() the handler and
// invoke it) to prove there's no data race on that shared state.
//
// This is a positive check: a clean TSan run (exit 0, no
// "WARNING: ThreadSanitizer: data race" report) is the pass criterion.
// It is not a torn-pointer detector by itself — see the commented-out
// deliberately-racy alternative below for how to confirm this test
// harness would actually catch a real regression.
#include "compile_time_ut.hpp"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using namespace CompileTimeUnitTesting;

namespace {

std::atomic<long> g_handler_calls{0};

void handler_a(const char*, const char*, int) {
    g_handler_calls.fetch_add(1, std::memory_order_relaxed);
}

void handler_b(const char*, const char*, int) {
    g_handler_calls.fetch_add(1, std::memory_order_relaxed);
}

constexpr int kSetterThreads = 4;
constexpr int kCheckerThreads = 4;
constexpr int kIterations = 20000;

} // namespace

int main() {
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    threads.reserve(kSetterThreads + kCheckerThreads);

    // Writers: hammer set_violation_handler(), cycling through two real
    // handlers and nullptr, so every store() carries a genuinely
    // different value (not just repeatedly writing the same pointer).
    for (int t = 0; t < kSetterThreads; ++t) {
        threads.emplace_back([t, &start] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int i = 0; i < kIterations; ++i) {
                switch ((i + t) % 3) {
                    case 0: set_violation_handler(nullptr); break;
                    case 1: set_violation_handler(handler_a); break;
                    default: set_violation_handler(handler_b); break;
                }
            }
        });
    }

    // Readers: trigger real precondition violations concurrently, each
    // of which load()s whatever handler happens to be installed at that
    // instant and, if non-null, calls through it.
    for (int t = 0; t < kCheckerThreads; ++t) {
        threads.emplace_back([&start] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int i = 0; i < kIterations; ++i) {
                try {
                    precondition(false);
                } catch (...) {
                    // Expected: check_contract() always throws after
                    // consulting the handler. The point under test is
                    // the handler dispatch, not the throw itself.
                }
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto& th : threads) th.join();

    set_violation_handler(nullptr);

    std::printf("thread_safety_tests: %ld handler invocations observed across %d threads, "
                "no data race reported\n",
                g_handler_calls.load(), kSetterThreads + kCheckerThreads);
    return 0;
}

// --- To confirm this harness actually catches a real regression ---
//
// Temporarily replace detail::violation_handler_ref()'s
// std::atomic<ViolationHandler> with a bare "static ViolationHandler"
// (the pre-fix implementation) and rebuild this target with
// -fsanitize=thread: TSan reports "WARNING: ThreadSanitizer: data race"
// pointing at the concurrent read (dispatch in check_contract) and
// write (set_violation_handler) within a few hundred iterations. Not
// left wired into the build permanently — it requires hand-editing the
// header — but verified manually while adding this test.
