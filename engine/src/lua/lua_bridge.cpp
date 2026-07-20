// engine/src/lua/lua_bridge.cpp
// ─────────────────────────────────────────────────────────────
// Lua Bridge implementation — Sol2 bindings and script execution.
// ─────────────────────────────────────────────────────────────

#include "beigebox/lua/lua_bridge.h"
#include "beigebox/ecs/components.h"
#include "beigebox/core/fixed_point.h"
#include "beigebox/world/thaw_grid.h"

#include <SDL2/SDL.h>
#include <cstdio>

namespace beigebox {

// ── Lifecycle ────────────────────────────────────────────────

LuaBridge::~LuaBridge()
{
    Shutdown();
}

bool LuaBridge::Init(entt::registry& registry)
{
    registry_ = &registry;

    // ── Open Lua VM ─────────────────────────────────────────
    lua_.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table
    );
    // NOTE: sol::lib::package, io, os are NOT loaded.
    // This sandboxes Lua — scripts cannot access the filesystem.

    // ── Register C++ API namespaces ─────────────────────────
    RegisterTransformAPI();
    RegisterCombatAPI();
    RegisterOrdersAPI();
    RegisterEconomyAPI();
    RegisterFactoryAPI();
    RegisterThawAPI();

    SDL_Log("LuaBridge: initialized (Lua %s)", LUA_VERSION);
    return true;
}

void LuaBridge::Shutdown()
{
    scripts_.clear();
    lua_.collect_garbage();
    registry_ = nullptr;
}

// ── Script Management ────────────────────────────────────────

bool LuaBridge::LoadScript(entt::entity entity,
                           const std::string& eventName,
                           const std::string& luaCode)
{
    // ── Compile the Lua chunk ────────────────────────────────
    auto result = lua_.safe_script(luaCode, sol::script_pass_on_error);
    if (!result.valid())
    {
        sol::error err = result;
        SDL_Log("LuaBridge: script compile error [entity %u, event '%s']: %s",
                EntityToID(entity), eventName.c_str(), err.what());
        return false;
    }

    // ── Extract the event handler function ──────────────────
    sol::protected_function fn = lua_[eventName];
    if (!fn.valid())
    {
        SDL_Log("LuaBridge: script missing function '%s' [entity %u]",
                eventName.c_str(), EntityToID(entity));
        return false;
    }

    // ── Clear the global name to prevent cross-entity pollution ──
    // The function is now owned by `fn` and stored in `scripts_`.
    lua_[eventName] = sol::nil;

    // ── Store for later invocation ───────────────────────────
    scripts_[EntityToID(entity)][eventName] = fn;
    SDL_Log("LuaBridge: loaded '%s' script for entity %u",
            eventName.c_str(), EntityToID(entity));
    return true;
}

// ── Event Firing ─────────────────────────────────────────────

void LuaBridge::FireEvent(entt::entity entity, const std::string& eventName)
{
    uint32_t eid = EntityToID(entity);

    // ── Trace ────────────────────────────────────────────────
    if (traceEnabled_)
    {
        EventTrace trace;
        trace.entityId = eid;
        trace.eventName = eventName;
        trace.frame = traceFrame_++;
        trace.breakpoint = false;

        if (traces_.size() >= kMaxTraces)
            traces_.erase(traces_.begin());
        traces_.push_back(trace);
    }

    // ── Check breakpoint ─────────────────────────────────────
    if (!paused_ || stepMode_)
    {
        bool hitBreakpoint = HasBreakpoint(entity, eventName);
        if (hitBreakpoint)
        {
            paused_ = true;
            stepMode_ = false;
            lastBreakEntity_ = eid;
            lastBreakEvent_ = eventName;

            // Mark last trace as breakpoint
            if (!traces_.empty())
                traces_.back().breakpoint = true;

            if (breakCallback_)
                breakCallback_(eid, eventName);

            SDL_Log("LuaBridge: BREAKPOINT hit — entity %u, event '%s'", eid, eventName.c_str());
        }
    }

    // ── If paused and not stepping, skip execution ───────────
    if (paused_ && !stepMode_)
        return;
    stepMode_ = false; // consumed the step

    auto entityIt = scripts_.find(eid);
    if (entityIt == scripts_.end())
        return;

    auto eventIt = entityIt->second.find(eventName);
    if (eventIt == entityIt->second.end())
        return;

    sol::protected_function& fn = eventIt->second;
    auto result = fn(eid);
    if (!result.valid())
    {
        sol::error err = result;
        SDL_Log("LuaBridge: runtime error [entity %u, event '%s']: %s",
                eid, eventName.c_str(), err.what());
    }
}

bool LuaBridge::HasScript(entt::entity entity, const std::string& eventName) const
{
    auto entityIt = scripts_.find(EntityToID(entity));
    if (entityIt == scripts_.end())
        return false;
    return entityIt->second.find(eventName) != entityIt->second.end();
}

// ── Breakpoint Management ────────────────────────────────────

void LuaBridge::AddBreakpoint(entt::entity entity, const std::string& eventName)
{
    breakpoints_.insert({EntityToID(entity), eventName});
    SDL_Log("LuaBridge: breakpoint set on entity %u, event '%s'",
            EntityToID(entity), eventName.c_str());
}

void LuaBridge::RemoveBreakpoint(entt::entity entity, const std::string& eventName)
{
    breakpoints_.erase({EntityToID(entity), eventName});
}

bool LuaBridge::HasBreakpoint(entt::entity entity, const std::string& eventName) const
{
    // Check specific entity breakpoint
    if (breakpoints_.count({EntityToID(entity), eventName}) > 0)
        return true;
    // Check wildcard (entity 0xFFFFFFFF = all entities)
    if (breakpoints_.count({0xFFFFFFFF, eventName}) > 0)
        return true;
    return false;
}

// ── Transform API Registration ───────────────────────────────
//
// Exposes the following Lua namespace:
//
//   Transform.GetPosition(entity_id) -> x_raw, y_raw
//   Transform.SetPosition(entity_id, x_raw, y_raw)
//   Transform.GetDistance(e1, e2) -> raw_distance
//
// All values are passed as raw int32_t (Q24.8). Lua sees them
// as plain integers. Conversion to/from FixedPoint happens at
// the C++ boundary — Lua never touches floating-point.
// ─────────────────────────────────────────────────────────────

void LuaBridge::RegisterTransformAPI()
{
    auto transform = lua_.create_named_table("Transform");

    // Transform.GetPosition(entity_id) → x_raw, y_raw
    transform.set_function("GetPosition",
        [this](int entityId) -> std::tuple<int, int>
        {
            auto entity = IDFromLua(entityId);
            if (!registry_->valid(entity) || !registry_->all_of<Transform>(entity))
                return {0, 0};

            auto& t = registry_->get<Transform>(entity);
            return {t.x.Raw(), t.y.Raw()};
        });

    // Transform.SetPosition(entity_id, x_raw, y_raw)
    transform.set_function("SetPosition",
        [this](int entityId, int xRaw, int yRaw)
        {
            auto entity = IDFromLua(entityId);
            if (!registry_->valid(entity) || !registry_->all_of<Transform>(entity))
                return;

            auto& t = registry_->get<Transform>(entity);
            t.x = FixedPoint(xRaw);
            t.y = FixedPoint(yRaw);
        });

    // Transform.GetDistance(e1, e2) → raw_distance
    // Uses Manhattan distance for now (cheap, deterministic).
    transform.set_function("GetDistance",
        [this](int e1Id, int e2Id) -> int
        {
            auto e1 = IDFromLua(e1Id);
            auto e2 = IDFromLua(e2Id);

            if (!registry_->valid(e1) || !registry_->all_of<Transform>(e1))
                return -1;
            if (!registry_->valid(e2) || !registry_->all_of<Transform>(e2))
                return -1;

            auto& t1 = registry_->get<Transform>(e1);
            auto& t2 = registry_->get<Transform>(e2);

            FixedPoint dx = Abs(t1.x - t2.x);
            FixedPoint dy = Abs(t1.y - t2.y);
            return (dx + dy).Raw();
        });
}

// ── Combat API Registration ──────────────────────────────────
//
// Exposes:
//   Combat.DealDamage(target_id, amount_raw, damage_type)
//   Combat.GetHealth(entity_id) -> current_raw, max_raw
//   Combat.IsEnemy(e1, e2) -> bool
//   Combat.GetEnemiesInRadius(entity_id, radius_raw) -> table {id=hp, ...}

void LuaBridge::RegisterCombatAPI()
{
    auto combat = lua_.create_named_table("Combat");

    // Combat.DealDamage(target_id, amount_raw, damage_type=0)
    combat.set_function("DealDamage",
        [this](int targetId, int amountRaw, sol::optional<int> damageType)
        {
            auto target = IDFromLua(targetId);
            if (!registry_->valid(target) || !registry_->all_of<Health>(target))
                return;

            auto& health = registry_->get<Health>(target);
            FixedPoint damage(amountRaw);
            health.current = health.current - damage;
            if (health.current.Raw() < 0)
                health.current = FixedPoint::FromInt(0);

            // If killed, tag as Dead
            if (health.current.Raw() <= 0)
                registry_->emplace_or_replace<Dead>(target);

            // Fire OnTakeDamage event
            FireEvent(target, "OnTakeDamage");
        });

    // Combat.GetHealth(entity_id) -> current_raw, max_raw
    combat.set_function("GetHealth",
        [this](int entityId) -> std::tuple<int, int>
        {
            auto entity = IDFromLua(entityId);
            if (!registry_->valid(entity) || !registry_->all_of<Health>(entity))
                return {0, 0};

            auto& h = registry_->get<Health>(entity);
            return {h.current.Raw(), h.max.Raw()};
        });

    // Combat.IsEnemy(e1, e2) -> bool
    combat.set_function("IsEnemy",
        [this](int e1Id, int e2Id) -> bool
        {
            auto e1 = IDFromLua(e1Id);
            auto e2 = IDFromLua(e2Id);

            auto* p1 = registry_->try_get<Player>(e1);
            auto* p2 = registry_->try_get<Player>(e2);
            if (!p1 || !p2) return false;
            return p1->factionId != p2->factionId && p1->factionId > 0 && p2->factionId > 0;
        });

    // Combat.GetEnemiesInRadius(entity_id, radius_raw) -> {entity_id = hp_raw, ...}
    combat.set_function("GetEnemiesInRadius",
        [this](int entityId, int radiusRaw) -> sol::table
        {
            sol::table result = lua_.create_table();
            auto center = IDFromLua(entityId);

            if (!registry_->valid(center) || !registry_->all_of<Transform>(center))
                return result;

            auto& ct = registry_->get<Transform>(center);
            auto* cp = registry_->try_get<Player>(center);
            int myFaction = cp ? cp->factionId : 0;

            FixedPoint radius(radiusRaw);

            auto view = registry_->view<Transform, Health, Player>();
            for (auto other : view)
            {
                if (other == center) continue;
                auto& op = registry_->get<Player>(other);
                if (op.factionId == myFaction || op.factionId == 0) continue;

                auto& ot = registry_->get<Transform>(other);
                FixedPoint dx = Abs(ct.x - ot.x);
                FixedPoint dy = Abs(ct.y - ot.y);
                FixedPoint dist = dx + dy;

                if (dist <= radius)
                {
                    auto& oh = registry_->get<Health>(other);
                    result[EntityToID(other)] = oh.current.Raw();
                }
            }

            return result;
        });
}

// ── Orders API Registration ──────────────────────────────────
//
// Exposes:
//   Orders.MoveTo(entity_id, target_x_raw, target_y_raw)
//   Orders.AttackTarget(entity_id, target_id)
//   Orders.Stop(entity_id)

void LuaBridge::RegisterOrdersAPI()
{
    auto orders = lua_.create_named_table("Orders");

    // Orders.MoveTo(entity_id, target_x_raw, target_y_raw)
    orders.set_function("MoveTo",
        [this](int entityId, int targetXRaw, int targetYRaw)
        {
            auto entity = IDFromLua(entityId);
            if (!registry_->valid(entity) || !registry_->all_of<Transform>(entity))
                return;

            FixedPoint tx(targetXRaw);
            FixedPoint ty(targetYRaw);
            FixedPoint defaultSpeed = FixedPoint::FromInt(2); // 2 tiles/sec

            registry_->emplace_or_replace<Movement>(entity, tx, ty, defaultSpeed);
        });

    // Orders.AttackTarget(entity_id, target_id)
    orders.set_function("AttackTarget",
        [this](int entityId, int targetId)
        {
            auto entity = IDFromLua(entityId);
            auto target = IDFromLua(targetId);

            if (!registry_->valid(entity) || !registry_->valid(target))
                return;
            if (!registry_->all_of<Transform>(entity) || !registry_->all_of<Transform>(target))
                return;

            // Move to target position, then OnTakeDamage handles combat
            auto& tt = registry_->get<Transform>(target);
            registry_->emplace_or_replace<Movement>(entity, tt.x, tt.y,
                FixedPoint::FromInt(3)); // faster when attacking
        });

    // Orders.Stop(entity_id)
    orders.set_function("Stop",
        [this](int entityId)
        {
            auto entity = IDFromLua(entityId);
            if (registry_->valid(entity) && registry_->all_of<Movement>(entity))
                registry_->remove<Movement>(entity);
        });
}

// ── Economy API Registration ─────────────────────────────────
//
// Exposes:
//   Economy.GiveSalvage(faction_id, amount_raw)
//   Economy.SpendHeat(faction_id, amount_raw) -> bool

void LuaBridge::RegisterEconomyAPI()
{
    auto economy = lua_.create_named_table("Economy");

    // Find or create the faction's resource entity
    auto getFactionEntity = [this](int factionId) -> entt::entity {
        auto view = registry_->view<Player, FactionResources>();
        for (auto e : view) {
            if (registry_->get<Player>(e).factionId == factionId)
                return e;
        }
        // Create faction resource entity if it doesn't exist
        auto e = registry_->create();
        registry_->emplace<Player>(e, factionId);
        registry_->emplace<FactionResources>(e, FixedPoint::FromInt(0), FixedPoint::FromInt(0));
        return e;
    };

    economy.set_function("GiveSalvage",
        [this, getFactionEntity](int factionId, int amountRaw)
        {
            auto e = getFactionEntity(factionId);
            auto& res = registry_->get<FactionResources>(e);
            res.salvage = res.salvage + FixedPoint(amountRaw);
        });

    economy.set_function("SpendHeat",
        [this, getFactionEntity](int factionId, int amountRaw) -> bool
        {
            auto e = getFactionEntity(factionId);
            auto& res = registry_->get<FactionResources>(e);
            FixedPoint cost(amountRaw);
            if (res.heat >= cost)
            {
                res.heat = res.heat - cost;
                return true;
            }
            return false;
        });
}

// ── Factory API Registration ─────────────────────────────────
//
// Exposes:
//   Factory.SpawnUnit(faction_id, unit_type_str, x_raw, y_raw) -> entity_id

void LuaBridge::RegisterFactoryAPI()
{
    auto factory = lua_.create_named_table("Factory");

    factory.set_function("SpawnUnit",
        [this](int factionId, const std::string& unitType,
               sol::optional<int> xRaw, sol::optional<int> yRaw) -> int
        {
            auto entity = registry_->create();
            int px = xRaw.value_or(0);
            int py = yRaw.value_or(0);

            registry_->emplace<Transform>(entity, FixedPoint(px), FixedPoint(py));
            registry_->emplace<Health>(entity,
                FixedPoint::FromInt(100), FixedPoint::FromInt(100));
            registry_->emplace<Player>(entity, factionId);

            // Unit-type-specific stats (could load from data files later)
            if (unitType == "Driller")
            {
                registry_->emplace<Weapon>(entity,
                    FixedPoint::FromInt(15),   // damage
                    FixedPoint::FromInt(2),    // range
                    0);                         // kinetic
            }
            else if (unitType == "HeatLamp")
            {
                registry_->emplace<HeatSource>(entity,
                    FixedPoint::FromInt(3),    // radius
                    FixedPoint::FromInt(10),   // intensity
                    0u); // sourceId assigned by ThawGrid
            }

            return static_cast<int>(EntityToID(entity));
        });
}

// ── Thaw API Registration ────────────────────────────────────
//
// Exposes:
//   Thaw.GetHeatLevel(tile_x, tile_y) -> int (0-255)
//   Thaw.AddHeatSource(tile_x, tile_y, radius_raw, intensity_raw) -> source_id
//   Thaw.RemoveHeatSource(source_id)
//   World.IsFrozen(tile_x, tile_y) -> bool
//   World.IsBuildable(tile_x, tile_y) -> bool

void LuaBridge::RegisterThawAPI()
{
    auto thaw = lua_.create_named_table("Thaw");
    auto world = lua_.create_named_table("World");

    thaw.set_function("GetHeatLevel",
        [this](int tileX, int tileY) -> int
        {
            if (!thawGrid_) return 0;
            return static_cast<int>(thawGrid_->GetHeat(tileX, tileY));
        });

    thaw.set_function("AddHeatSource",
        [this](int tileX, int tileY, int radiusRaw, int intensityRaw) -> uint32_t
        {
            if (!thawGrid_) return 0;
            return thawGrid_->AddHeatSource(tileX, tileY,
                FixedPoint(radiusRaw), FixedPoint(intensityRaw));
        });

    thaw.set_function("RemoveHeatSource",
        [this](uint32_t sourceId)
        {
            if (thawGrid_)
                thawGrid_->RemoveHeatSource(sourceId);
        });

    world.set_function("IsFrozen",
        [this](int tileX, int tileY) -> bool
        {
            if (!thawGrid_) return true;
            return thawGrid_->IsFrozen(tileX, tileY);
        });

    world.set_function("IsBuildable",
        [this](int tileX, int tileY) -> bool
        {
            if (!thawGrid_) return false;
            return thawGrid_->IsBuildable(tileX, tileY);
        });
}

} // namespace beigebox
