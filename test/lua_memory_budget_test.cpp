// SYS-W2-06: budgetedLuaAllocator() exercised directly against Lua's real
// lua_Alloc contract (see LuaMemoryBudget.hpp's header comment), rather than
// indirectly through real Lua execution -- Lua's incremental GC scheduling
// makes it impractical to deterministically force the exact sequence of
// new-allocation/resize/free calls a regression test needs from script
// source alone. Calling the allocator directly gives an exact, fast,
// non-flaky reproduction of the bug this fixes: a new allocation's `oldSize`
// (a small object-kind tag per Lua's contract, not a real prior size) used
// to be subtracted from the running total as if it were one, silently
// eroding the tracked total below real usage across many small allocations.

#include <MeshCraft/Editor/LuaMemoryBudget.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace MeshCraft::Editor;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

// A tiny stand-in for luaC_newobj()'s call shape: pointer == nullptr, and
// oldSize is an object-kind tag (0-8ish; LUA_TSTRING/LUA_TTABLE/... in real
// Lua), never a real previous size.
static void* newObject(LuaMemoryBudget& budget, std::size_t tag, std::size_t size) {
    return budgetedLuaAllocator(&budget, nullptr, tag, size);
}

static void freeObject(LuaMemoryBudget& budget, void* p, std::size_t realSize) {
    budgetedLuaAllocator(&budget, p, realSize, 0);
}

static void* resizeBlock(LuaMemoryBudget& budget, void* p, std::size_t oldRealSize,
                          std::size_t newSize) {
    return budgetedLuaAllocator(&budget, p, oldRealSize, newSize);
}

int main() {
    // Many small "new object" allocations (the LUA_TTABLE/LUA_TSTRING tag
    // shape), all held (never freed), totalling comfortably under budget:
    // every one must succeed, and the tracked total must equal the real
    // sum of sizes exactly -- no drift from the tag values, which the old
    // code would have subtracted on every single one of these calls.
    {
        LuaMemoryBudget budget;
        std::vector<void*> held;
        constexpr std::size_t kCount = 100000;
        constexpr std::size_t kEach = 64; // e.g. a small table's sizeof()
        bool allSucceeded = true;
        for (std::size_t i = 0; i < kCount; ++i) {
            void* p = newObject(budget, /*tag=*/5, kEach); // LUA_TTABLE-shaped
            if (!p) { allSucceeded = false; break; }
            held.push_back(p);
        }
        check(allSucceeded, "many small held table-shaped allocations under budget all succeed");
        check(budget.allocated == kCount * kEach,
              "tracked total after many small held allocations equals the real sum "
              "exactly (no drift from the new-allocation tag)");
        for (void* p : held) freeObject(budget, p, kEach);
        check(budget.allocated == 0, "freeing every held allocation returns the tracked total to 0");
    }

    // Many short strings (the LUA_TSTRING tag shape), same property.
    {
        LuaMemoryBudget budget;
        std::vector<void*> held;
        constexpr std::size_t kCount = 50000;
        constexpr std::size_t kEach = 24;
        for (std::size_t i = 0; i < kCount; ++i) {
            void* p = newObject(budget, /*tag=*/4, kEach); // LUA_TSTRING-shaped
            held.push_back(p);
        }
        check(budget.allocated == kCount * kEach,
              "tracked total after many short-string-shaped allocations equals the "
              "real sum exactly");
        for (void* p : held) freeObject(budget, p, kEach);
    }

    // Table growth: repeated resize of ONE existing block (real oldSize each
    // time, per Lua's luaM_realloc_ contract) must track the CURRENT size,
    // not accumulate every intermediate size.
    {
        LuaMemoryBudget budget;
        void* p = newObject(budget, /*tag=*/5, 16);
        check(budget.allocated == 16, "initial table allocation tracked");
        for (std::size_t size = 32; size <= 4096; size *= 2) {
            std::size_t prevSize = size / 2;
            void* grown = resizeBlock(budget, p, prevSize, size);
            check(grown != nullptr, "table growth to " + std::to_string(size) + " succeeds");
            p = grown;
            check(budget.allocated == size,
                  "tracked total after growth to " + std::to_string(size) +
                  " equals the current size, not a cumulative sum");
        }
        freeObject(budget, p, 4096);
        check(budget.allocated == 0, "freeing the grown block returns tracked total to 0");
    }

    // Grow/shrink reallocations: shrinking must reduce the tracked total by
    // the real delta, not the other way around.
    {
        LuaMemoryBudget budget;
        void* p = newObject(budget, /*tag=*/5, 4096);
        p = resizeBlock(budget, p, 4096, 1024); // shrink
        check(budget.allocated == 1024, "shrinking a block tracks the smaller current size");
        p = resizeBlock(budget, p, 1024, 8192); // grow past the original size
        check(budget.allocated == 8192, "growing a shrunk block tracks the larger current size");
        freeObject(budget, p, 8192);
        check(budget.allocated == 0, "freeing after grow/shrink returns tracked total to 0");
    }

    // Collection followed by reallocation: after freeing a batch, the
    // budget must have genuinely reclaimed headroom for a fresh allocation
    // that would not otherwise fit.
    {
        LuaMemoryBudget budget;
        std::vector<void*> held;
        constexpr std::size_t kEach = 1024 * 1024; // 1 MiB each
        for (int i = 0; i < 15; ++i) held.push_back(newObject(budget, 5, kEach));
        check(budget.allocated == 15 * kEach, "15 MiB held before collection");

        // A 4 MiB request does not fit in the ~1 MiB of remaining headroom.
        void* tooBig = newObject(budget, 4, 4 * 1024 * 1024);
        check(tooBig == nullptr, "a request exceeding remaining headroom is rejected before collection");

        // "Collect" by freeing 10 of the 15 held blocks.
        for (int i = 0; i < 10; ++i) freeObject(budget, held[static_cast<std::size_t>(i)], kEach);
        check(budget.allocated == 5 * kEach, "tracked total reflects exactly what remains held after collection");

        // The same 4 MiB request now fits in the reclaimed headroom.
        void* fitsNow = newObject(budget, 4, 4 * 1024 * 1024);
        check(fitsNow != nullptr, "the same request succeeds once collection has freed real headroom");

        for (int i = 10; i < 15; ++i) freeObject(budget, held[static_cast<std::size_t>(i)], kEach);
        freeObject(budget, fitsNow, 4 * 1024 * 1024);
        check(budget.allocated == 0, "fully unwound to 0 after freeing everything");
    }

    // Failed allocation rollback: a rejected request must not perturb the
    // tracked total at all, whether it's a new allocation or a failed
    // resize of an existing block.
    {
        LuaMemoryBudget budget;
        void* p = newObject(budget, 5, kLuaMemoryBudgetBytes - 1024);
        const std::size_t before = budget.allocated;
        void* rejected = newObject(budget, 4, 2048); // does not fit in the last 1024 bytes
        check(rejected == nullptr, "a new allocation exceeding remaining headroom is rejected");
        check(budget.allocated == before,
              "a rejected new allocation leaves the tracked total completely unchanged");

        void* rejectedResize = resizeBlock(budget, p, kLuaMemoryBudgetBytes - 1024, kLuaMemoryBudgetBytes + 1024);
        check(rejectedResize == nullptr, "a resize exceeding the budget is rejected");
        check(budget.allocated == before,
              "a rejected resize leaves the tracked total completely unchanged (the "
              "original block, and its accounting, are still intact)");

        freeObject(budget, p, kLuaMemoryBudgetBytes - 1024);
        check(budget.allocated == 0, "unwound to 0 after the rollback-verified block is freed");
    }

    // Successful transaction immediately below the limit, and rejection
    // immediately above it, from an empty budget.
    {
        LuaMemoryBudget budget;
        void* atLimit = newObject(budget, 5, kLuaMemoryBudgetBytes);
        check(atLimit != nullptr, "an allocation of exactly the budget succeeds from empty");
        freeObject(budget, atLimit, kLuaMemoryBudgetBytes);
        check(budget.allocated == 0, "unwound to 0 after the at-limit allocation is freed");

        void* overLimit = newObject(budget, 5, kLuaMemoryBudgetBytes + 1);
        check(overLimit == nullptr, "an allocation of budget+1 is rejected from empty");
        check(budget.allocated == 0, "a from-empty rejection leaves the tracked total at 0");
    }

    if (failures == 0) { std::cout << "All Lua memory budget tests passed.\n"; return 0; }
    std::cerr << failures << " Lua memory budget test(s) failed.\n";
    return 1;
}
