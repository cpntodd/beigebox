// editor/src/panels/entity_editor.h
// ─────────────────────────────────────────────────────────────
// Entity Editor — full unit/building/hero designer.
//
// Features:
//   - Multi-file storage: data/{units,buildings,heroes,projectiles}/*.json
//   - Component-based abilities (each ability = ECS component set)
//   - Sandbox preview viewport
//   - Balance spreadsheet view
//   - Tech tree dropdown integration
//   - Category tabs + search
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include <SDL2/SDL_opengl.h>
#include <nlohmann/json.hpp>

namespace beigebox {

class LuaBridge;
class Renderer;
class ThawGrid;
class AIChatPanel;

// ── Weapon definition ───────────────────────────────────────
struct WeaponDef
{
    std::string name;
    int    damage     = 25;
    int    range      = 2;
    float  attackRate = 1.0f;
    int    damageType = 0;
};

// ── Entity template (unit, building, hero, projectile) ──────
struct EntityTemplate
{
    std::string name;
    std::string category;    // "units", "buildings", "heroes", "projectiles"
    std::string filePath;    // relative path, e.g. "data/units/driller.json"

    // Stats
    int    hp         = 100;
    int    armor      = 0;
    int    speed      = 2;
    int    sight      = 6;
    int    cost       = 100;
    int    buildTime  = 15;
    int    population = 1;

    // Visual / Audio
    std::string sprite;
    std::string soundMove;
    std::string soundAttack;
    std::string soundDeath;
    std::string soundSelect;

    // Weapons
    std::vector<WeaponDef> weapons;

    // Abilities (component names to attach at spawn)
    std::vector<std::string> abilities;
    nlohmann::json abilityParams;

    // Tech / Build
    std::string techRequired;
    std::string builtFrom;

    // Faction / AI
    int    faction     = 0;
    int    veterancyLevel = 0;
    std::string personality;  // "Aggressive", "Defensive", "Explorer", "Coward"

    // Superweapon (if any)
    std::string superweaponType;
    int    superweaponCooldown = 120;

    // Serialization
    void ToJson(nlohmann::json& j) const;
    void FromJson(const nlohmann::json& j);
};

// ── Ability definition registry ─────────────────────────────
struct AbilityDef
{
    std::string name;
    std::string description;
    std::vector<std::string> components; // ECS component names
    nlohmann::json defaultParams;
};

class EntityEditor
{
public:
    EntityEditor(LuaBridge& lua, Renderer& renderer, ThawGrid& thaw);
    ~EntityEditor() = default;

    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    void SetRootPath(const std::string& p) { rootPath_ = p; }
    void Draw();

    // Access template list (for other panels like Sound event bindings)
    const std::vector<EntityTemplate>& Templates() const { return templates_; }
    std::vector<std::string> UnitTypeNames() const;

private:
    // ── Tabs ─────────────────────────────────────────────────
    void DrawDesignerTab();
    void DrawSpreadsheetTab();

    // ── Template editing ─────────────────────────────────────
    void DrawTemplateEditor(EntityTemplate& tmpl);
    void DrawStatsSection(EntityTemplate& tmpl);
    void DrawWeaponsSection(EntityTemplate& tmpl);
    void DrawAbilitiesSection(EntityTemplate& tmpl);
    void DrawPreviewViewport(EntityTemplate& tmpl);

    // ── Data management ──────────────────────────────────────
    void LoadAllTemplates();
    void SaveTemplate(const EntityTemplate& tmpl);
    void DeleteTemplate(const std::string& filePath);
    std::string DataDir() const;
    void ScanCategories();

    // ── Helpers ──────────────────────────────────────────────
    void Log(const std::string& msg);
    void LoadAbilityRegistry();

    // ── State ────────────────────────────────────────────────
    LuaBridge*     lua_;
    Renderer*      renderer_;
    ThawGrid*      thaw_;
    AIChatPanel*   aiChat_ = nullptr;
    std::string    rootPath_ = ".";

    std::vector<EntityTemplate> templates_;
    std::vector<std::string>    categories_;
    std::vector<AbilityDef>     abilityRegistry_;
    int              activeTemplate_ = -1;
    int              selectedCategory_ = 0;
    bool             showNewDialog_ = false;
    char             newNameBuf_[64] = {};
    char             searchBuf_[64]  = {};

    // Sandbox state
    int    sandboxW_ = 320;
    int    sandboxH_ = 240;
    GLuint sandboxFbo_ = 0;
    GLuint sandboxTex_ = 0;
};

} // namespace beigebox
