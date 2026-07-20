// engine/include/beigebox/lua/lua_vm_pool.h
// ─────────────────────────────────────────────────────────────
// Per-Thread Lua VM Pool
//
// Maintains N independent sol::state instances for use by
// worker threads. Each thread acquires a VM, executes scripts,
// and returns it. No locking during execution — each VM is
// exclusively owned by its thread while checked out.
//
// The main LuaBridge VM is separate and runs on the main thread.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <sol/sol.hpp>
#include <entt/entt.hpp>

#include <vector>
#include <mutex>
#include <memory>

namespace beigebox {

class LuaVmPool
{
public:
    LuaVmPool() = default;
    ~LuaVmPool();

    LuaVmPool(const LuaVmPool&) = delete;
    LuaVmPool& operator=(const LuaVmPool&) = delete;

    // Initialize N VMs. Each gets the same registry + API bindings
    // as the main LuaBridge. count = 0 uses hardware_concurrency - 1.
    void Init(entt::registry& registry, int count = 0);

    void Shutdown();

    int Count() const { return static_cast<int>(vms_.size()); }

    // Check out a VM for exclusive use. Returns nullptr if none available.
    // Call ReturnVm() when done.
    sol::state* AcquireVm();

    // Return a VM to the pool.
    void ReturnVm(sol::state* vm);

private:
    struct PooledVm
    {
        std::unique_ptr<sol::state> state;
        bool inUse = false;
    };

    void InitSingleVm(sol::state& lua, entt::registry& registry);

    std::vector<PooledVm> vms_;
    std::mutex mutex_;
};

} // namespace beigebox
