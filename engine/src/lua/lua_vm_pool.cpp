// engine/src/lua/lua_vm_pool.cpp
// ─────────────────────────────────────────────────────────────
// Per-thread Lua VM pool implementation.
// ─────────────────────────────────────────────────────────────

#include "beigebox/lua/lua_vm_pool.h"
#include "beigebox/ecs/components.h"
#include "beigebox/core/fixed_point.h"

#include <SDL2/SDL.h>
#include <thread>

namespace beigebox {

LuaVmPool::~LuaVmPool()
{
    Shutdown();
}

void LuaVmPool::Init(entt::registry& registry, int count)
{
    if (count <= 0)
    {
        int hw = static_cast<int>(std::thread::hardware_concurrency());
        count = std::max(1, hw - 1);
    }

    Shutdown();

    for (int i = 0; i < count; ++i)
    {
        PooledVm pv;
        pv.state = std::make_unique<sol::state>();
        pv.inUse = false;
        InitSingleVm(*pv.state, registry);
        vms_.push_back(std::move(pv));
    }

    SDL_Log("LuaVmPool: %d worker VMs initialized", count);
}

void LuaVmPool::Shutdown()
{
    vms_.clear();
}

sol::state* LuaVmPool::AcquireVm()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pv : vms_)
    {
        if (!pv.inUse)
        {
            pv.inUse = true;
            return pv.state.get();
        }
    }
    return nullptr; // all in use
}

void LuaVmPool::ReturnVm(sol::state* vm)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pv : vms_)
    {
        if (pv.state.get() == vm)
        {
            pv.inUse = false;
            return;
        }
    }
}

// ── Minimal API registration for worker VMs ─────────────────
// Worker VMs need the same API surface as the main VM so scripts
// can call Transform.GetPosition(), Combat.DealDamage(), etc.
// We register a subset of the full LuaBridge API for thread safety.

void LuaVmPool::InitSingleVm(sol::state& lua, entt::registry& registry)
{
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    // ── Transform API ────────────────────────────────────────
    auto transformTable = lua.create_named_table("Transform");

    transformTable.set_function("GetPosition",
        [&registry](int entityId) -> std::tuple<int, int> {
            auto e = entt::entity(static_cast<uint32_t>(entityId));
            if (!registry.valid(e)) return {0, 0};
            if (auto* t = registry.try_get<Transform>(e))
                return {t->x.Raw(), t->y.Raw()};
            return {0, 0};
        });

    transformTable.set_function("SetPosition",
        [&registry](int entityId, int xRaw, int yRaw) {
            auto e = entt::entity(static_cast<uint32_t>(entityId));
            if (registry.valid(e))
            {
                auto& t = registry.get_or_emplace<Transform>(e);
                t.x = FixedPoint(xRaw);
                t.y = FixedPoint(yRaw);
            }
        });

    transformTable.set_function("GetDistance",
        [&registry](int e1Id, int e2Id) -> int {
            auto e1 = entt::entity(static_cast<uint32_t>(e1Id));
            auto e2 = entt::entity(static_cast<uint32_t>(e2Id));
            if (!registry.valid(e1) || !registry.valid(e2)) return 0;
            auto* t1 = registry.try_get<Transform>(e1);
            auto* t2 = registry.try_get<Transform>(e2);
            if (!t1 || !t2) return 0;
            FixedPoint dx = t1->x - t2->x;
            FixedPoint dy = t1->y - t2->y;
            return Abs(dx).Raw() + Abs(dy).Raw(); // Manhattan
        });

    // ── Combat API ───────────────────────────────────────────
    auto combatTable = lua.create_named_table("Combat");

    combatTable.set_function("DealDamage",
        [&registry](int targetId, int damageRaw, int damageType) {
            auto e = entt::entity(static_cast<uint32_t>(targetId));
            if (!registry.valid(e)) return;
            if (auto* h = registry.try_get<Health>(e))
            {
                h->current = h->current - FixedPoint(damageRaw);
                if (h->current.Raw() <= 0)
                    registry.emplace_or_replace<Dead>(e);
            }
        });

    combatTable.set_function("GetHealth",
        [&registry](int entityId) -> std::tuple<int, int> {
            auto e = entt::entity(static_cast<uint32_t>(entityId));
            if (!registry.valid(e)) return {0, 0};
            if (auto* h = registry.try_get<Health>(e))
                return {h->current.Raw(), h->max.Raw()};
            return {0, 0};
        });

    combatTable.set_function("IsEnemy",
        [&registry](int e1Id, int e2Id) -> bool {
            auto e1 = entt::entity(static_cast<uint32_t>(e1Id));
            auto e2 = entt::entity(static_cast<uint32_t>(e2Id));
            if (!registry.valid(e1) || !registry.valid(e2)) return false;
            auto* p1 = registry.try_get<Player>(e1);
            auto* p2 = registry.try_get<Player>(e2);
            if (!p1 || !p2) return false;
            return p1->factionId != p2->factionId;
        });

    // Capture a raw pointer to the lua state for safe lambda use
    sol::state* luaPtr = &lua;

    combatTable.set_function("GetEnemiesInRadius",
        [&registry, luaPtr](int entityId, int radiusRaw) -> sol::table {
            sol::table result = luaPtr->create_table();

            auto center = entt::entity(static_cast<uint32_t>(entityId));
            if (!registry.valid(center)) return result;

            auto* ct = registry.try_get<Transform>(center);
            auto* cp = registry.try_get<Player>(center);
            if (!ct || !cp) return result;

            FixedPoint radius(radiusRaw);

            auto view = registry.view<Transform, Player>();
            for (auto e : view)
            {
                if (e == center) continue;
                auto& pt = registry.get<Player>(e);
                if (pt.factionId == cp->factionId) continue;

                auto& tt = registry.get<Transform>(e);
                FixedPoint dx = tt.x - ct->x;
                FixedPoint dy = tt.y - ct->y;
                if (Abs(dx) + Abs(dy) <= radius)
                {
                    int hp = 0;
                    if (auto* h = registry.try_get<Health>(e))
                        hp = h->current.Raw();
                    result[static_cast<int>(entt::to_integral(e))] = hp;
                }
            }
            return result;
        });

    // ── Orders API ───────────────────────────────────────────
    auto ordersTable = lua.create_named_table("Orders");

    ordersTable.set_function("MoveTo",
        [&registry](int entityId, int targetX, int targetY) {
            auto e = entt::entity(static_cast<uint32_t>(entityId));
            if (!registry.valid(e)) return;
            registry.emplace_or_replace<Movement>(e,
                FixedPoint(targetX), FixedPoint(targetY), FixedPoint::FromInt(2));
        });

    ordersTable.set_function("AttackTarget",
        [&registry](int entityId, int targetId) {
            auto e = entt::entity(static_cast<uint32_t>(entityId));
            auto t = entt::entity(static_cast<uint32_t>(targetId));
            if (!registry.valid(e) || !registry.valid(t)) return;
            if (auto* tt = registry.try_get<Transform>(t))
                registry.emplace_or_replace<Movement>(e,
                    tt->x, tt->y, FixedPoint::FromInt(3));
        });

    ordersTable.set_function("Stop",
        [&registry](int entityId) {
            auto e = entt::entity(static_cast<uint32_t>(entityId));
            if (registry.valid(e))
                registry.remove<Movement>(e);
        });

    // ── Economy API ──────────────────────────────────────────
    auto economyTable = lua.create_named_table("Economy");
    economyTable.set_function("GiveSalvage",
        [](int faction, int amount) { /* stub — main VM handles this */ });
    economyTable.set_function("SpendHeat",
        [](int faction, int amount) -> bool { return true; });
}

} // namespace beigebox
