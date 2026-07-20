// editor/src/panels/unit_templates.h
#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace beigebox {

struct UnitTemplate {
    std::string name;
    int hp = 100, damage = 15, range = 2, speed = 2;
    int faction = 0;
    std::string sprite;
};

class UnitTemplateManager {
public:
    void Load(const std::string& path);
    void Save(const std::string& path);
    std::vector<UnitTemplate>& Templates() { return templates_; }
    const UnitTemplate* Find(const std::string& name) const;
    void Add(const UnitTemplate& t) { templates_.push_back(t); }
    void Draw();
private:
    std::vector<UnitTemplate> templates_;
};

} // namespace beigebox
