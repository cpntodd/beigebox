// editor/src/panels/unit_templates.cpp
#include "unit_templates.h"
#include <imgui.h>
#include <fstream>
#include <cstdio>

namespace beigebox {

void UnitTemplateManager::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    nlohmann::json j = nlohmann::json::parse(f);
    templates_.clear();
    for (auto& uj : j["units"]) {
        UnitTemplate ut;
        ut.name = uj.value("name", "Unknown");
        ut.hp = uj.value("hp", 100);
        ut.damage = uj.value("damage", 15);
        ut.range = uj.value("range", 2);
        ut.speed = uj.value("speed", 2);
        ut.faction = uj.value("faction", 0);
        ut.sprite = uj.value("sprite", "");
        templates_.push_back(ut);
    }
}

void UnitTemplateManager::Save(const std::string& path) {
    nlohmann::json j;
    j["units"] = nlohmann::json::array();
    for (auto& ut : templates_) {
        nlohmann::json uj;
        uj["name"] = ut.name; uj["hp"] = ut.hp; uj["damage"] = ut.damage;
        uj["range"] = ut.range; uj["speed"] = ut.speed;
        uj["faction"] = ut.faction; uj["sprite"] = ut.sprite;
        j["units"].push_back(uj);
    }
    std::ofstream f(path);
    f << j.dump(2);
}

const UnitTemplate* UnitTemplateManager::Find(const std::string& name) const {
    for (auto& t : templates_)
        if (t.name == name) return &t;
    return nullptr;
}

void UnitTemplateManager::Draw() {
    ImGui::Begin("Unit Templates");
    static char nameBuf[64], spriteBuf[128];
    static int hp=100, dmg=15, rng=2, spd=2, faction=0;

    ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
    ImGui::InputInt("HP", &hp); ImGui::InputInt("Damage", &dmg);
    ImGui::InputInt("Range", &rng); ImGui::InputInt("Speed", &spd);
    ImGui::InputInt("Faction", &faction);
    ImGui::InputText("Sprite", spriteBuf, sizeof(spriteBuf));
    if (ImGui::Button("Add Template")) {
        UnitTemplate ut;
        ut.name = nameBuf; ut.hp = hp; ut.damage = dmg;
        ut.range = rng; ut.speed = spd; ut.faction = faction; ut.sprite = spriteBuf;
        templates_.push_back(ut);
    }
    ImGui::Separator();
    for (auto& t : templates_) {
        ImGui::Text("%s: HP=%d DMG=%d RNG=%d SPD=%d Fac=%d",
            t.name.c_str(), t.hp, t.damage, t.range, t.speed, t.faction);
    }
    ImGui::End();
}

} // namespace beigebox
