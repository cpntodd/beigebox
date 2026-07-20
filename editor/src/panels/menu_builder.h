// editor/src/panels/menu_builder.h
// ─────────────────────────────────────────────────────────────
// Menu Builder — WYSIWYG GUI screen designer.
//
// Vertical widget palette (drag to canvas) → visual canvas
// with move/resize/snap → hierarchy tree → properties panel.
// Toggle Design (wireframe) / Preview (styled) modes.
// Per-screen JSON in ui/menus/{screen}.json.
// Exports to standalone .lua files for the game runtime.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <vector>
#include <set>
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
    int  parentId = -1;  // -1 = root level, otherwise parent widget id
    WidgetType type = WidgetType::Button;
    std::string name;

    // Layout
    Anchor anchor = Anchor::Center;
    int    offsetX = 0, offsetY = 0;
    int    width = 200, height = 40;
    int    gridRow = 0, gridCol = 0;

    // Appearance
    std::string text;
    std::string image;
    int    fontSize = 18;
    float  colorR = 1.0f, colorG = 1.0f, colorB = 1.0f, colorA = 1.0f;

    // Behavior
    std::string onClick;
    std::string binding;

    // Type-specific
    float  minVal = 0, maxVal = 100, curVal = 50;
    bool   checked = false;
    std::vector<std::string> options;
    int    selectedOption = 0;

    // Hierarchy
    bool   visible = true;
    bool   locked  = false;
};

// ── Screen definition ───────────────────────────────────────
struct ScreenDef
{
    std::string name;
    std::string background;
    std::string music;
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

    // Called when a widget is selected/deselected (nullptr = deselect)
    std::function<void(WidgetDef*)> onWidgetSelect;

    // Widget type icon (Unicode) for palette
    static const char* WidgetIcon(WidgetType t);
    static const char* WidgetTypeName(WidgetType t);
    static const char* AnchorName(Anchor a);

    // ── I/O (public for project-explorer routing) ────────────
    void LoadScreen(const std::string& name);

private:
    // ── Layout regions ───────────────────────────────────────
    void DrawPalette();          // left sidebar: vertical widget icons
    void DrawCanvasContent();    // center: WYSIWYG design surface (no inner child; parent provides container)
    void DrawHierarchyTree();    // right panel: parent-child tree
    void DrawPropertiesPanel();  // bottom or overlay: selected widget props

    // ── Canvas rendering ────────────────────────────────────
    void RenderDesignCanvas();   // wireframe mode
    void RenderPreviewCanvas();  // styled mode
    void DrawWidgetOnCanvas(const WidgetDef& w);
    void DrawResizeHandles(const WidgetDef& w);
    void DrawAnchorHandle(const WidgetDef& w);
    void DrawSnapGuides();
    void DrawMarqueeRect();

    // ── Canvas interaction ───────────────────────────────────
    void HandleCanvasInput();
    void HandleDragDrop();
    int  HitTest(int mx, int my) const;  // returns widget index at point
    int  HitTestResizeHandles(int widgetIdx, int mx, int my) const;  // returns handle 0-7 or -1
    ImVec2 AnchorToCanvasPos(const WidgetDef& w) const;
    void StartMoving(int widgetIdx, int mx, int my);
    void StartResizing(int widgetIdx, int handle, int mx, int my);
    void ApplySnap(int& x, int& y, int w, int h);

    // ── Widget management ────────────────────────────────────
    void AddWidget(WidgetType type, int x, int y);
    void RemoveWidget(int index);
    void BringToFront(int index);
    void SendToBack(int index);

    // ── Hierarchy helpers ────────────────────────────────────
    std::vector<int> GetChildrenOf(int parentId) const;
    void DrawHierarchyNode(int widgetIdx, int depth);
    int  GetRootParent(int widgetIdx) const;

    // ── I/O ──────────────────────────────────────────────────
    void NewScreen();
    void SaveScreen();
    void ExportLua();
    void RefreshScreenList();
    std::string MenusDir() const;
    void Log(const std::string& msg);
    void NotifySelectionChanged();

    // ── State ────────────────────────────────────────────────
    AIChatPanel* aiChat_ = nullptr;
    std::string  rootPath_ = ".";

    ScreenDef    screen_;
    int          widgetIdCounter_ = 1;

    // Selection
    std::set<int> selectedWidgets_;  // multi-select support
    int  hoveredWidget_ = -1;

    // Canvas interaction state
    bool  designMode_   = true;  // true=wireframe, false=styled preview
    bool  dragging_     = false;
    bool  resizing_     = false;
    bool  marqueeSelect_ = false;
    int   resizeHandle_ = -1;    // 0-7 = corner/edge handle index, -1 = none
    ImVec2 dragStartPos_;
    ImVec2 dragOffset_;
    ImVec2 marqueeStart_;
    ImVec2 marqueeEnd_;
    int   snappedX_ = 0, snappedY_ = 0;
    bool  showSnapGuides_ = false;
    ImVec2 snapLineH_, snapLineV_;

    // Canvas viewport
    float canvasScale_  = 0.4f;
    float canvasOffsetX_ = 0, canvasOffsetY_ = 0;
    int   canvasW_ = 1920, canvasH_ = 1080;
    float zoomFactor_  = 1.0f;  // user scroll-wheel zoom (0.1–2.0)

    // UI state
    char  screenNameBuf_[64] = {};
    std::vector<std::string> screenList_;
    bool  showNewDialog_ = false;
    bool  showExportDialog_ = false;
    bool  showAnchorPresets_ = false;

    // Click-to-place: selected palette tool (-1 = none)
    int   selectedTool_ = -1;  // WidgetType cast to int, or -1 for none
};

} // namespace beigebox

