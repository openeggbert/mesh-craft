#pragma once

#include <cstddef>
#include <cstdlib>

namespace MeshCraft::Editor {

inline constexpr std::size_t kLuaMemoryBudgetBytes = 16U * 1024U * 1024U;

struct LuaMemoryBudget {
    std::size_t allocated{0};
};

// SYS-W2-06: the allocator Lua uses for every VM allocation (strings,
// tables, userdata, VM bookkeeping). Its `oldSize` contract lets the runner
// enforce a strict aggregate budget without exposing a custom allocator or
// an unbounded auxiliary allocation map to script code.
//
// Header-only and pulled out of LuaScriptRunner.cpp's anonymous namespace
// specifically so it can be unit-tested directly against Lua's real
// lua_Alloc contract (test/lua_memory_budget_test.cpp) without depending on
// Lua's own GC scheduling to indirectly probe the accounting.
//
// Per Lua 5.4's lmem.c (luaM_realloc_/luaM_free_): `(osize == 0) == (block
// == NULL)` holds for every *resize or free* of an existing block -- osize
// is genuinely the real prior size there, and genuinely 0 when block is
// NULL. But luaM_malloc_ (used by luaC_newobj for every brand-new
// string/table/userdata/thread) calls the allocator as
// `frealloc(ud, NULL, tag, size)`, passing the object's small type tag
// (LUA_TSTRING, LUA_TTABLE, ...) as `osize` -- NOT a real previous size,
// because there is no previous block. Treating that tag as if it were a
// real retained size (as this function used to, unconditionally) silently
// erodes the tracked total on every new allocation once it has grown large
// enough for the clamp below to stop catching it, letting real usage drift
// above the declared budget across many small allocations.
inline void* budgetedLuaAllocator(void* userData, void* pointer, std::size_t oldSize,
                                   std::size_t newSize) noexcept
{
    auto& budget = *static_cast<LuaMemoryBudget*>(userData);
    const std::size_t retained = (pointer == nullptr)
        ? budget.allocated
        : (oldSize <= budget.allocated ? budget.allocated - oldSize : 0);
    if (newSize == 0) {
        std::free(pointer);
        budget.allocated = retained;
        return nullptr;
    }
    if (newSize > kLuaMemoryBudgetBytes - retained) return nullptr;
    void* replacement = std::realloc(pointer, newSize);
    if (!replacement) return nullptr;
    budget.allocated = retained + newSize;
    return replacement;
}

} // namespace MeshCraft::Editor
