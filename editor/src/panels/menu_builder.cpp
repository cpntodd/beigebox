// editor/src/panels/menu_builder.cpp
// ─────────────────────────────────────────────────────────────
// Menu Builder — full implementation.
// ─────────────────────────────────────────────────────────────

#include "menu_builder.h"
#include "ai_chat.h"

#include <imgui.h>
#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>

#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace beigebox {

using json = nlohmann::json;

// ═════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════

MenuBuilder::MenuBuilder()
{
    screen_.name = "MainMenu";
    screen_.screenW = 1920;
    screen_.screenH = 1080;
}

void MenuBuilder::Log(const std::string& msg)
{
    if (aiChat_) aiChat_->AppendMessage("system", "[Menu] " + msg);
    SDL_Log("[Menu] %s", msg.c_str());
}

std::string MenuBuilder::MenusDir() const
{
    return rootPath_ + "/ui/menus";
}

const char* MenuBuilder::WidgetTypeName(WidgetType t) const
{
    switch (t) {
        case WidgetType::Button: return "Button";
        case WidgetType::Label: return "Label";
        case WidgetType::Image: return "Image";
        case WidgetType::Panel: return "Panel";
        case WidgetType::Slider: return "Slider";
        case WidgetType::Checkbox: return "Checkbox";
        case WidgetType::Dropdown: return "Dropdown";
        case WidgetType::TextInput: return "TextInput";
        case WidgetType::ProgressBar: return "ProgressBar";
        case WidgetType::Spacer: return "Spacer";
        case WidgetType::TabGroup: return "TabGroup";
        case WidgetType::ScrollArea: return "ScrollArea";
        case WidgetType::ListBox: return "ListBox";
        case WidgetType::ColorPicker: return "ColorPicker";
        default: return "???";
    }
}

const char* MenuBuilder::AnchorName(Anchor a) const
{
    switch (a) {
        case Anchor::TopLeft: return "top-left";
        case Anchor::TopCenter: return "top-center";
        case Anchor::TopRight: return "top-right";
        case Anchor::CenterLeft: return "center-left";
        case Anchor::Center: return "center";
        case Anchor::CenterRight: return "center-right";
        case Anchor::BottomLeft: return "bottom-left";
        case Anchor::BottomCenter: return "bottom-center";
        case Anchor::BottomRight: return "bottom-right";
        default: return "center";
    }
}

ImVec2 MenuBuilder::AnchorToScreenPos(Anchor anchor, int ox, int oy, int w, int h) const
{
    float sx = 0, sy = 0;
    float sw = static_cast<float>(previewW_);
    float sh = static_cast<float>(previewH_);

    switch (anchor) {
        case Anchor::TopLeft:      sx = 0;           sy = 0;           break;
        case Anchor::TopCenter:    sx = sw/2 - w/2;   sy = 0;           break;
        case Anchor::TopRight:     sx = sw - w;       sy = 0;           break;
        case Anchor::CenterLeft:   sx = 0;           sy = sh/2 - h/2;   break;
        case Anchor::Center:       sx = sw/2 - w/2;  sy = sh/2 - h/2;   break;
        case Anchor::CenterRight:  sx = sw - w;      sy = sh/2 - h/2;   break;
        case Anchor::BottomLeft:   sx = 0;           sy = sh - h;       break;
        case Anchor::BottomCenter: sx = sw/2 - w/2;  sy = sh - h;       break;
        case Anchor::BottomRight:  sx = sw - w;      sy = sh - h;       break;
    }
    return ImVec2(sx + ox, sy + oy);
}

// ═════════════════════════════════════════════════════════════
// Main Draw
// ═════════════════════════════════════════════════════════════

void MenuBuilder::Draw()
{
    ImGui::Begin("Menu Builder");

    if (ImGui::BeginTabBar("##menuTabs"))
    {
        if (ImGui::BeginTabItem("Designer")) { DrawDesignerTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Preview"))  { DrawPreviewTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Export"))   { DrawExportTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ── Designer Tab ────────────────────────────────────────────

void MenuBuilder::DrawDesignerTab()
{
    // ── Screen management toolbar ────────────────────────────
    if (ImGui::Button("New Screen"))
        showNewDialog_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Save"))
        SaveScreen();
    ImGui::SameLine();
    if (ImGui::Button("Load..."))
        RefreshScreenList();
    ImGui::SameLine();
    ImGui::Text("Screen: %s", screen_.name.c_str());

    ImGui::Separator();

    // ── Two-panel layout ─────────────────────────────────────
    // Left: widget list + add buttons
    ImGui::BeginChild("##leftPanel", ImVec2(200, 0), true);

    ImGui::Text("Add Widget:");
    if (ImGui::Button("Button"))     AddWidget(WidgetType::Button);
    if (ImGui::Button("Label"))      AddWidget(WidgetType::Label);
    if (ImGui::Button("Image"))      AddWidget(WidgetType::Image);
    if (ImGui::Button("Panel"))      AddWidget(WidgetType::Panel);
    if (ImGui::Button("Slider"))     AddWidget(WidgetType::Slider);
    if (ImGui::Button("Checkbox"))   AddWidget(WidgetType::Checkbox);
    if (ImGui::Button("Dropdown"))   AddWidget(WidgetType::Dropdown);
    if (ImGui::Button("TextInput"))  AddWidget(WidgetType::TextInput);
    if (ImGui::Button("ProgressBar"))AddWidget(WidgetType::ProgressBar);
    if (ImGui::Button("Spacer"))     AddWidget(WidgetType::Spacer);
    if (ImGui::Button("TabGroup"))   AddWidget(WidgetType::TabGroup);
    if (ImGui::Button("ScrollArea")) AddWidget(WidgetType::ScrollArea);
    if (ImGui::Button("ListBox"))    AddWidget(WidgetType::ListBox);
    if (ImGui::Button("ColorPicker"))AddWidget(WidgetType::ColorPicker);

    ImGui::Separator();
    ImGui::Text("Widgets (%zu):", screen_.widgets.size());

    for (int i = 0; i < static_cast<int>(screen_.widgets.size()); ++i)
    {
        auto& w = screen_.widgets[i];
        ImGui::PushID(i);
        std::string label = std::string(WidgetTypeName(w.type)) + ": " +
            (w.name.empty() ? w.text.substr(0, 15) : w.name);
        if (ImGui::Selectable(label.c_str(), selectedWidget_ == i))
            selectedWidget_ = i;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Duplicate")) DuplicateWidget(i);
            if (ImGui::MenuItem("Delete"))   RemoveWidget(i);
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // Right: widget properties
    ImGui::BeginChild("##rightPanel", ImVec2(0, 0), false);
    DrawWidgetProps();
    ImGui::EndChild();

    // ── New screen dialog ────────────────────────────────────
    if (showNewDialog_)
    {
        ImGui::OpenPopup("New Screen");
        if (ImGui::BeginPopupModal("New Screen", &showNewDialog_))
        {
            ImGui::InputText("Name", screenNameBuf_, sizeof(screenNameBuf_));
            if (ImGui::Button("Create") && screenNameBuf_[0])
            {
                screen_.name = screenNameBuf_;
                screen_.widgets.clear();
                selectedWidget_ = -1;
                showNewDialog_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) showNewDialog_ = false;
            ImGui::EndPopup();
        }
    }
}

// ── Widget Properties Panel ─────────────────────────────────

void MenuBuilder::DrawWidgetProps()
{
    if (selectedWidget_ < 0 || selectedWidget_ >= static_cast<int>(screen_.widgets.size()))
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "Select a widget to edit its properties.");
        return;
    }

    auto& w = screen_.widgets[selectedWidget_];
    ImGui::Text("Editing: %s", WidgetTypeName(w.type));
    ImGui::Separator();

    // Name
    ImGui::InputText("ID", &w.name[0], w.name.size() + 1);
    if (w.name.size() < 64) w.name.resize(64);

    // Layout
    ImGui::Text("Layout:");
    int anchorIdx = static_cast<int>(w.anchor);
    if (ImGui::Combo("Anchor", &anchorIdx,
            [](void*, int idx, const char** out) {
                static const char* names[] = {"TopLeft","TopCenter","TopRight",
                    "CenterLeft","Center","CenterRight",
                    "BottomLeft","BottomCenter","BottomRight"};
                *out = names[idx]; return true;
            }, nullptr, 9))
        w.anchor = static_cast<Anchor>(anchorIdx);

    ImGui::InputInt("Offset X", &w.offsetX);
    ImGui::InputInt("Offset Y", &w.offsetY);
    ImGui::InputInt("Width", &w.width);
    ImGui::InputInt("Height", &w.height);
    ImGui::InputInt("Grid Row", &w.gridRow);
    ImGui::InputInt("Grid Col", &w.gridCol);

    ImGui::Separator();
    ImGui::Text("Appearance:");

    if (w.type != WidgetType::Spacer && w.type != WidgetType::Panel)
    {
        ImGui::InputText("Text", &w.text[0], w.text.size() + 1);
        if (w.text.size() < 128) w.text.resize(128);
    }

    if (w.type == WidgetType::Image)
    {
        ImGui::InputText("Image", &w.image[0], w.image.size() + 1);
        if (w.image.size() < 128) w.image.resize(128);
    }

    ImGui::InputInt("Font Size", &w.fontSize);
    ImGui::ColorEdit4("Color", &w.colorR, ImGuiColorEditFlags_NoInputs);

    ImGui::Separator();
    ImGui::Text("Behavior:");

    ImGui::InputText("OnClick", &w.onClick[0], w.onClick.size() + 1);
    if (w.onClick.size() < 64) w.onClick.resize(64);

    ImGui::InputText("Data Binding", &w.binding[0], w.binding.size() + 1);
    if (w.binding.size() < 64) w.binding.resize(64);

    // Type-specific params
    if (w.type == WidgetType::Slider || w.type == WidgetType::ProgressBar)
    {
        ImGui::SliderFloat("Value", &w.curVal, w.minVal, w.maxVal);
        ImGui::InputFloat("Min", &w.minVal);
        ImGui::InputFloat("Max", &w.maxVal);
    }
    if (w.type == WidgetType::Checkbox)
        ImGui::Checkbox("Checked", &w.checked);
    if (w.type == WidgetType::Dropdown || w.type == WidgetType::ListBox)
    {
        ImGui::InputInt("Selected", &w.selectedOption);
        static char optBuf[128] = {};
        ImGui::InputText("Add Option", optBuf, sizeof(optBuf));
        ImGui::SameLine();
        if (ImGui::Button("+") && optBuf[0])
        {
            w.options.push_back(optBuf);
            optBuf[0] = '\0';
        }
        for (int i = 0; i < static_cast<int>(w.options.size()); ++i)
        {
            ImGui::BulletText("%s", w.options[i].c_str());
            ImGui::SameLine();
            ImGui::PushID(9000 + i);
            if (ImGui::SmallButton("X"))
                w.options.erase(w.options.begin() + i);
            ImGui::PopID();
        }
    }
}

// ── Widget CRUD ─────────────────────────────────────────────

void MenuBuilder::AddWidget(WidgetType type)
{
    WidgetDef w;
    w.id = widgetIdCounter_++;
    w.type = type;
    w.name = std::string(WidgetTypeName(type)) + "_" + std::to_string(w.id);
    if (type == WidgetType::Button) w.text = "Button";
    if (type == WidgetType::Label)  w.text = "Label";
    screen_.widgets.push_back(w);
    selectedWidget_ = static_cast<int>(screen_.widgets.size()) - 1;
}

void MenuBuilder::RemoveWidget(int index)
{
    if (index >= 0 && index < static_cast<int>(screen_.widgets.size()))
    {
        screen_.widgets.erase(screen_.widgets.begin() + index);
        if (selectedWidget_ >= static_cast<int>(screen_.widgets.size()))
            selectedWidget_ = static_cast<int>(screen_.widgets.size()) - 1;
    }
}

void MenuBuilder::DuplicateWidget(int index)
{
    if (index >= 0 && index < static_cast<int>(screen_.widgets.size()))
    {
        WidgetDef dup = screen_.widgets[index];
        dup.id = widgetIdCounter_++;
        dup.name += "_copy";
        screen_.widgets.push_back(dup);
    }
}

// ── Preview Tab ─────────────────────────────────────────────

void MenuBuilder::DrawPreviewTab()
{
    ImGui::Text("Live Preview — %s", screen_.name.c_str());
    ImGui::SameLine();
    ImGui::SliderFloat("Scale", &previewScale_, 0.25f, 1.0f, "%.1f");
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        previewRunning_ = true;

    previewW_ = static_cast<int>(screen_.screenW * previewScale_);
    previewH_ = static_cast<int>(screen_.screenH * previewScale_);

    ImGui::Separator();

    // Preview canvas
    ImVec2 canvasSize(previewW_ + 4, previewH_ + 4);
    ImGui::BeginChild("##previewCanvas", canvasSize, true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Background
    dl->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + previewW_, canvasPos.y + previewH_),
        IM_COL32(15, 15, 25, 255));

    // Grid lines
    for (int x = 0; x < previewW_; x += 50)
        dl->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
            ImVec2(canvasPos.x + x, canvasPos.y + previewH_),
            IM_COL32(30, 30, 45, 80));
    for (int y = 0; y < previewH_; y += 50)
        dl->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
            ImVec2(canvasPos.x + previewW_, canvasPos.y + y),
            IM_COL32(30, 30, 45, 80));

    // Render widgets
    for (auto& w : screen_.widgets)
    {
        ImVec2 pos = AnchorToScreenPos(w.anchor, w.offsetX, w.offsetY, w.width, w.height);
        pos.x += canvasPos.x;
        pos.y += canvasPos.y;

        ImU32 col = IM_COL32(
            static_cast<int>(w.colorR * 255),
            static_cast<int>(w.colorG * 255),
            static_cast<int>(w.colorB * 255),
            static_cast<int>(w.colorA * 255));

        switch (w.type)
        {
            case WidgetType::Button:
                dl->AddRectFilled(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                    IM_COL32(50, 100, 180, 255));
                dl->AddRect(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                    IM_COL32(100, 150, 255, 200));
                dl->AddText(ImVec2(pos.x + 8, pos.y + w.height/2 - 8),
                    IM_COL32(255, 255, 255, 255), w.text.c_str());
                break;

            case WidgetType::Label:
                dl->AddText(pos, col, w.text.c_str());
                break;

            case WidgetType::Image:
                dl->AddRectFilled(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                    IM_COL32(40, 40, 60, 200));
                dl->AddText(pos, IM_COL32(150, 150, 150, 255), w.image.c_str());
                break;

            case WidgetType::Panel:
                dl->AddRectFilled(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                    IM_COL32(25, 25, 40, 220));
                dl->AddRect(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                    IM_COL32(60, 60, 90, 150));
                break;

            case WidgetType::Slider: {
                dl->AddRectFilled(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                    IM_COL32(30, 30, 50, 200));
                float fill = (w.curVal - w.minVal) / (w.maxVal - w.minVal);
                dl->AddRectFilled(pos,
                    ImVec2(pos.x + w.width * fill, pos.y + w.height),
                    IM_COL32(60, 180, 60, 255));
                break;
            }

            case WidgetType::Checkbox:
                dl->AddRect(pos, ImVec2(pos.x + 20, pos.y + 20),
                    IM_COL32(200, 200, 200, 200));
                if (w.checked)
                    dl->AddText(pos, IM_COL32(100, 255, 100, 255), "✓");
                dl->AddText(ImVec2(pos.x + 28, pos.y), col, w.text.c_str());
                break;

            case WidgetType::ProgressBar: {
                dl->AddRect(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                    IM_COL32(100, 100, 100, 150));
                float pct = (w.curVal - w.minVal) / (w.maxVal - w.minVal);
                dl->AddRectFilled(pos,
                    ImVec2(pos.x + w.width * pct, pos.y + w.height),
                    IM_COL32(60, 180, 60, 200));
                break;
            }

            case WidgetType::Dropdown:
                dl->AddRectFilled(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                    IM_COL32(50, 50, 70, 255));
                dl->AddText(ImVec2(pos.x + 4, pos.y + 2), col,
                    w.options.empty() ? "Dropdown" : w.options[0].c_str());
                break;

            default:
                // Spacer, TextInput, TabGroup, ScrollArea, ListBox, ColorPicker
                dl->AddRect(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                    IM_COL32(80, 80, 80, 100));
                dl->AddText(pos, IM_COL32(150, 150, 150, 150), WidgetTypeName(w.type));
                break;
        }

        // Selection highlight
        if (static_cast<int>(&w - &screen_.widgets[0]) == selectedWidget_)
        {
            dl->AddRect(pos, ImVec2(pos.x + w.width, pos.y + w.height),
                IM_COL32(255, 200, 50, 180), 0, 0, 2.0f);
        }
    }

    ImGui::EndChild();

    ImGui::TextDisabled("Preview is rendered via ImGui draw lists. Interactive in runtime build.");
}

// ── Export Tab ──────────────────────────────────────────────

void MenuBuilder::DrawExportTab()
{
    ImGui::Text("Export screen to Lua");
    ImGui::Separator();
    ImGui::Text("Screen: %s (%zu widgets)", screen_.name.c_str(), screen_.widgets.size());

    if (ImGui::Button("Export .lua"))
        ExportLua();

    ImGui::SameLine();
    if (ImGui::Button("Export All Screens"))
    {
        RefreshScreenList();
        for (auto& name : screenList_)
        {
            LoadScreen(name);
            ExportLua();
        }
        Log("Exported all screens.");
    }

    ImGui::Separator();
    ImGui::Text("Preview of generated Lua:");
    ImGui::BeginChild("##luaPreview", ImVec2(0, 0), true);

    std::string lua = "-- " + screen_.name + ".lua (auto-generated)\n";
    lua += "local M = {}\n\n";
    lua += "function M.Create(ui)\n";
    for (auto& w : screen_.widgets)
    {
        lua += "  ui:Add" + std::string(WidgetTypeName(w.type)) + "{\n";
        lua += "    id = \"" + w.name + "\",\n";
        lua += "    anchor = \"" + std::string(AnchorName(w.anchor)) + "\",\n";
        lua += "    offsetX = " + std::to_string(w.offsetX) + ",\n";
        lua += "    offsetY = " + std::to_string(w.offsetY) + ",\n";
        lua += "    width = " + std::to_string(w.width) + ",\n";
        lua += "    height = " + std::to_string(w.height) + ",\n";
        if (!w.text.empty())
            lua += "    text = \"" + w.text + "\",\n";
        if (!w.onClick.empty())
            lua += "    onClick = \"" + w.onClick + "\",\n";
        lua += "  }\n";
    }
    lua += "end\n\n";
    lua += "return M\n";

    ImGui::TextUnformatted(lua.c_str());
    ImGui::EndChild();
}

// ═════════════════════════════════════════════════════════════
// File I/O
// ═════════════════════════════════════════════════════════════

void MenuBuilder::NewScreen()
{
    screen_.widgets.clear();
    selectedWidget_ = -1;
    widgetIdCounter_ = 1;
}

void MenuBuilder::SaveScreen()
{
    std::string dir = MenusDir();
    mkdir(dir.c_str(), 0755);

    json j;
    j["name"] = screen_.name;
    j["background"] = screen_.background;
    j["music"] = screen_.music;
    j["screenW"] = screen_.screenW;
    j["screenH"] = screen_.screenH;

    json widgets = json::array();
    for (auto& w : screen_.widgets)
    {
        json wj;
        wj["id"] = w.id;
        wj["type"] = static_cast<int>(w.type);
        wj["name"] = w.name;
        wj["anchor"] = static_cast<int>(w.anchor);
        wj["offsetX"] = w.offsetX;
        wj["offsetY"] = w.offsetY;
        wj["width"] = w.width;
        wj["height"] = w.height;
        wj["gridRow"] = w.gridRow;
        wj["gridCol"] = w.gridCol;
        if (!w.text.empty()) wj["text"] = w.text;
        if (!w.image.empty()) wj["image"] = w.image;
        wj["fontSize"] = w.fontSize;
        wj["color"] = {w.colorR, w.colorG, w.colorB, w.colorA};
        if (!w.onClick.empty()) wj["onClick"] = w.onClick;
        if (!w.binding.empty()) wj["binding"] = w.binding;
        wj["minVal"] = w.minVal;
        wj["maxVal"] = w.maxVal;
        wj["curVal"] = w.curVal;
        wj["checked"] = w.checked;
        if (!w.options.empty()) wj["options"] = w.options;
        wj["selectedOption"] = w.selectedOption;
        widgets.push_back(wj);
    }
    j["widgets"] = widgets;

    std::string path = dir + "/" + screen_.name + ".json";
    std::ofstream f(path);
    f << j.dump(2);
    f.close();
    Log("Saved: " + path);
}

void MenuBuilder::LoadScreen(const std::string& name)
{
    std::string path = MenusDir() + "/" + name + ".json";
    std::ifstream f(path);
    if (!f.good()) { Log("Not found: " + path); return; }

    json j = json::parse(f);
    screen_.name = j.value("name", name);
    screen_.background = j.value("background", "");
    screen_.music = j.value("music", "");
    screen_.screenW = j.value("screenW", 1920);
    screen_.screenH = j.value("screenH", 1080);

    screen_.widgets.clear();
    for (auto& wj : j["widgets"])
    {
        WidgetDef w;
        w.id = wj.value("id", 0);
        w.type = static_cast<WidgetType>(wj.value("type", 0));
        w.name = wj.value("name", "");
        w.anchor = static_cast<Anchor>(wj.value("anchor", 4));
        w.offsetX = wj.value("offsetX", 0);
        w.offsetY = wj.value("offsetY", 0);
        w.width = wj.value("width", 200);
        w.height = wj.value("height", 40);
        w.gridRow = wj.value("gridRow", 0);
        w.gridCol = wj.value("gridCol", 0);
        w.text = wj.value("text", "");
        w.image = wj.value("image", "");
        w.fontSize = wj.value("fontSize", 18);
        if (wj.contains("color")) {
            w.colorR = wj["color"][0]; w.colorG = wj["color"][1];
            w.colorB = wj["color"][2]; w.colorA = wj["color"][3];
        }
        w.onClick = wj.value("onClick", "");
        w.binding = wj.value("binding", "");
        w.minVal = wj.value("minVal", 0.0f);
        w.maxVal = wj.value("maxVal", 100.0f);
        w.curVal = wj.value("curVal", 50.0f);
        w.checked = wj.value("checked", false);
        if (wj.contains("options"))
            for (auto& o : wj["options"]) w.options.push_back(o.get<std::string>());
        w.selectedOption = wj.value("selectedOption", 0);
        screen_.widgets.push_back(w);
    }

    widgetIdCounter_ = screen_.widgets.empty() ? 1 :
        screen_.widgets.back().id + 1;
    selectedWidget_ = -1;
    Log("Loaded: " + path + " (" + std::to_string(screen_.widgets.size()) + " widgets)");
}

void MenuBuilder::ExportLua()
{
    std::string dir = MenusDir();
    mkdir(dir.c_str(), 0755);

    std::string lua = "-- " + screen_.name + ".lua (auto-generated by M.A.D. Menu Builder)\n";
    lua += "-- Screen: " + screen_.name + "\n";
    lua += "-- Widgets: " + std::to_string(screen_.widgets.size()) + "\n\n";
    lua += "local M = {}\n\n";
    lua += "function M.Create(ui)\n";
    lua += "  ui:BeginScreen(\"" + screen_.name + "\")\n";
    if (!screen_.background.empty())
        lua += "  ui:SetBackground(\"" + screen_.background + "\")\n";
    if (!screen_.music.empty())
        lua += "  ui:SetMusic(\"" + screen_.music + "\")\n\n";

    for (auto& w : screen_.widgets)
    {
        lua += "  -- " + std::string(WidgetTypeName(w.type)) + ": " + w.name + "\n";
        lua += "  ui:Add" + std::string(WidgetTypeName(w.type)) + "{\n";
        lua += "    id = \"" + w.name + "\",\n";
        lua += "    anchor = \"" + std::string(AnchorName(w.anchor)) + "\",\n";
        lua += "    offsetX = " + std::to_string(w.offsetX) + ",\n";
        lua += "    offsetY = " + std::to_string(w.offsetY) + ",\n";
        lua += "    width = " + std::to_string(w.width) + ",\n";
        lua += "    height = " + std::to_string(w.height) + ",\n";
        if (!w.text.empty())
            lua += "    text = \"" + w.text + "\",\n";
        if (!w.image.empty())
            lua += "    image = \"" + w.image + "\",\n";
        if (w.fontSize != 18)
            lua += "    fontSize = " + std::to_string(w.fontSize) + ",\n";
        if (!w.onClick.empty())
            lua += "    onClick = \"" + w.onClick + "\",\n";
        if (!w.binding.empty())
            lua += "    binding = \"" + w.binding + "\",\n";
        if (w.type == WidgetType::Slider || w.type == WidgetType::ProgressBar) {
            lua += "    min = " + std::to_string(w.minVal) + ",\n";
            lua += "    max = " + std::to_string(w.maxVal) + ",\n";
            lua += "    value = " + std::to_string(w.curVal) + ",\n";
        }
        lua += "  }\n\n";
    }

    lua += "  ui:EndScreen()\n";
    lua += "end\n\n";
    lua += "-- Callback stubs (implement these in your game logic):\n";
    for (auto& w : screen_.widgets)
    {
        if (!w.onClick.empty())
            lua += "-- function " + w.onClick + "() end\n";
    }
    lua += "\nreturn M\n";

    std::string path = dir + "/" + screen_.name + ".lua";
    std::ofstream f(path);
    f << lua;
    f.close();
    Log("Exported: " + path);
}

void MenuBuilder::RefreshScreenList()
{
    screenList_.clear();
    std::string dir = MenusDir();
    mkdir(dir.c_str(), 0755);

    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        std::string name(entry->d_name);
        if (entry->d_type == DT_REG && name.size() > 5 &&
            name.substr(name.size() - 5) == ".json")
        {
            screenList_.push_back(name.substr(0, name.size() - 5));
        }
    }
    closedir(d);
    std::sort(screenList_.begin(), screenList_.end());
}

} // namespace beigebox
