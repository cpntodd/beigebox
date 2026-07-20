// editor/src/panels/menu_builder.h
// ─────────────────────────────────────────────────────────────
// Menu Builder — GUI screen designer for game menus and HUD.
//
// Per-screen JSON files in ui/menus/{screen}.json.
// 14+ widget types with anchor+grid layout.
// Live ImGui preview with working button callbacks.
// Exports to standalone .lua files for the game runtime.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace beigebox {

class AIChatPanel;

// ── Widget types ────────────────────────────────────────────
enum class WidgetType
{
    Button, Label, Image, Panel, Slider, Checkbox,
    Dropdown, TextInput, ProgressBar, Spacer,
    TabGroup, ScrollArea, ListBox, ColorPicker
};

enum class Anchor
{
    TopLeft, TopCenter, TopRight,
    CenterLeft, Center, CenterRight,
    BottomLeft, BottomCenter, BottomRight
};

// ── Widget definition ───────────────────────────────────────
struct WidgetDef
{
    int  id = 0;
    WidgetType type = WidgetType::Button;
    std::string name;

    // Layout
    Anchor anchor = Anchor::Center;
    int    offsetX = 0, offsetY = 0;
    int    width = 200, height = 40;
    int    gridRow = 0, gridCol = 0; // for grid layout within panels

    // Appearance
    std::string text;
    std::string image;
    int    fontSize = 18;
    float  colorR = 1.0f, colorG = 1.0f, colorB = 1.0f, colorA = 1.0f;

    // Behavior
    std::string onClick;      // Lua callback name
    std::string onHover;
    std::string binding;      // data binding (e.g. "player.hp")

    // Type-specific
    float  minVal = 0, maxVal = 100, curVal = 50;  // Slider/ProgressBar
    bool   checked = false;                          // Checkbox
    std::vector<std::string> options;                // Dropdown/ListBox
    int    selectedOption = 0;
    std::vector<int> children;                       // Panel/TabGroup/ScrollArea child widget IDs
};

// ── Screen definition ───────────────────────────────────────
struct ScreenDef
{
    std::string name;
    std::string background;
    std::string music;
    std::string transition;  // "fade", "slide_left", "none"
    std::vector<WidgetDef> widgets;
    int    screenW = 1920, screenH = 1080;
};

class MenuBuilder
{
public:
    MenuBuilder();
    ~MenuBuilder() = default;

    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    void SetRootPath(const std::string& p) { rootPath_ = p; }
    void Draw();

private:
    // ── Tabs ─────────────────────────────────────────────────
    void DrawDesignerTab();
    void DrawWidgetProps();
    void DrawPreviewTab();
    void DrawExportTab();

    // ── Widget creation ──────────────────────────────────────
    void AddWidget(WidgetType type);
    void RemoveWidget(int index);
    void DuplicateWidget(int index);

    // ── Preview rendering ────────────────────────────────────
    void RenderPreviewWidget(const WidgetDef& w);
    ImVec2 AnchorToScreenPos(Anchor anchor, int offsetX, int offsetY, int w, int h) const;

    // ── Layout helpers ───────────────────────────────────────
    const char* WidgetTypeName(WidgetType t) const;
    const char* AnchorName(Anchor a) const;

    // ── I/O ──────────────────────────────────────────────────
    void NewScreen();
    void LoadScreen(const std::string& name);
    void SaveScreen();
    void ExportLua();
    void RefreshScreenList();
    std::string MenusDir() const;
    void Log(const std::string& msg);

    // ── State ────────────────────────────────────────────────
    AIChatPanel* aiChat_ = nullptr;
    std::string  rootPath_ = ".";

    ScreenDef    screen_;
    int          selectedWidget_ = -1;
    int          widgetIdCounter_ = 1;

    // UI state
    char  screenNameBuf_[64] = {};
    char  screenFilter_[64]  = {};
    int   activeTab_ = 0;
    std::vector<std::string> screenList_;
    bool  showNewDialog_ = false;

    // Preview state
    bool  previewRunning_ = false;
    int   previewW_ = 960, previewH_ = 540;
    float previewScale_ = 0.5f;
};

} // namespace beigebox
