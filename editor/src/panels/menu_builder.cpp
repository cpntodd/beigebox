// editor/src/panels/menu_builder.cpp
// ─────────────────────────────────────────────────────────────
// Menu Builder — WYSIWYG with drag-drop canvas + hierarchy.
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
#include <algorithm>
#include <cmath>

namespace beigebox {
using json = nlohmann::json;
using WT = WidgetType;

// C++17 enum-class aliases (avoid C++20 `using enum`)
constexpr WT Button = WT::Button, Label = WT::Label, Image = WT::Image;
constexpr WT Panel = WT::Panel, Slider = WT::Slider, Checkbox = WT::Checkbox;
constexpr WT Dropdown = WT::Dropdown, TextInput = WT::TextInput;
constexpr WT ProgressBar = WT::ProgressBar, Spacer = WT::Spacer;
constexpr WT TabGroup = WT::TabGroup, ScrollArea = WT::ScrollArea;
constexpr WT ListBox = WT::ListBox, ColorPicker = WT::ColorPicker;

// Icon table
const char* MenuBuilder::WidgetIcon(WidgetType t) {
    switch (t) {
        case Button: return "BTN"; case Label: return "LBL";
        case Image: return "IMG"; case Panel: return "PNL";
        case Slider: return "SLD"; case Checkbox: return "CHK";
        case Dropdown: return "DRO"; case TextInput: return "TXT";
        case ProgressBar: return "BAR"; case Spacer: return "SPC";
        case TabGroup: return "TAB"; case ScrollArea: return "SCR";
        case ListBox: return "LST"; case ColorPicker: return "CLR";
        default: return "?";
    }
}

const char* MenuBuilder::WidgetTypeName(WidgetType t) {
    switch (t) {
        case Button: return "Button"; case Label: return "Label";
        case Image: return "Image"; case Panel: return "Panel";
        case Slider: return "Slider"; case Checkbox: return "Checkbox";
        case Dropdown: return "Dropdown"; case TextInput: return "TextInput";
        case ProgressBar: return "ProgressBar"; case Spacer: return "Spacer";
        case TabGroup: return "TabGroup"; case ScrollArea: return "ScrollArea";
        case ListBox: return "ListBox"; case ColorPicker: return "ColorPicker";
        default: return "???";
    }
}

const char* MenuBuilder::AnchorName(Anchor a) {
    switch (a) {
        case Anchor::TopLeft: return "top-left"; case Anchor::TopCenter: return "top-center";
        case Anchor::TopRight: return "top-right"; case Anchor::CenterLeft: return "center-left";
        case Anchor::Center: return "center"; case Anchor::CenterRight: return "center-right";
        case Anchor::BottomLeft: return "bottom-left"; case Anchor::BottomCenter: return "bottom-center";
        case Anchor::BottomRight: return "bottom-right"; default: return "center";
    }
}

MenuBuilder::MenuBuilder() { screen_.name = "MainMenu"; }
void MenuBuilder::Log(const std::string& msg) {
    if (aiChat_) aiChat_->AppendMessage("system", "[Menu] " + msg);
    SDL_Log("[Menu] %s", msg.c_str());
}
std::string MenuBuilder::MenusDir() const { return rootPath_ + "/ui/menus"; }

std::vector<int> MenuBuilder::GetChildrenOf(int parentId) const {
    std::vector<int> r;
    for (int i = 0; i < (int)screen_.widgets.size(); ++i)
        if (screen_.widgets[i].parentId == parentId) r.push_back(i);
    return r;
}

// ═══════════════════════════════════════════════════════════
// MAIN DRAW
// ═══════════════════════════════════════════════════════════

void MenuBuilder::Draw() {
    ImGui::Begin("Menu Builder");
    if (ImGui::Button("New")) showNewDialog_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Save")) SaveScreen();
    ImGui::SameLine();
    if (ImGui::Button("Load...")) RefreshScreenList();
    ImGui::SameLine();
    ImGui::Text("Screen: %s", screen_.name.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);
    ImGui::Checkbox("Design", &designMode_);
    ImGui::SameLine();
    if (ImGui::Button("Export")) showExportDialog_ = true;
    ImGui::Separator();

    // ── 3-panel layout: palette | canvas | hierarchy ──────
    // Use child windows instead of Columns for reliable fill behaviour
    float availW = ImGui::GetContentRegionAvail().x;
    float availH = ImGui::GetContentRegionAvail().y;
    float palW = 52, hierW = 200;
    float canvasW = availW - palW - hierW;
    if (canvasW < 100) canvasW = 100;

    // Left: palette
    ImGui::BeginChild("##palettePane", ImVec2(palW, availH), false);
    DrawPalette();
    ImGui::EndChild();
    ImGui::SameLine();

    // Center: canvas (fills remaining space)
    ImGui::BeginChild("##canvasPane", ImVec2(canvasW, availH), false);
    DrawCanvasContent();
    ImGui::EndChild();
    ImGui::SameLine();

    // Right: hierarchy
    ImGui::BeginChild("##hierPane", ImVec2(hierW, availH), false);
    DrawHierarchyTree();
    ImGui::EndChild();

    // ── Properties panel (shown when a widget is selected) ──
    if (!selectedWidgets_.empty())
        DrawPropertiesPanel();

    if (showNewDialog_) {
        ImGui::OpenPopup("New Screen");
        if (ImGui::BeginPopupModal("New Screen", &showNewDialog_)) {
            ImGui::InputText("Name", screenNameBuf_, sizeof(screenNameBuf_));
            if (ImGui::Button("Create") && screenNameBuf_[0]) {
                screen_.name = screenNameBuf_; screen_.widgets.clear();
                selectedWidgets_.clear(); showNewDialog_ = false;
            }
            ImGui::SameLine(); if (ImGui::Button("Cancel")) showNewDialog_ = false;
            ImGui::EndPopup();
        }
    }
    if (showExportDialog_) {
        ImGui::OpenPopup("Export Lua");
        if (ImGui::BeginPopupModal("Export Lua", &showExportDialog_)) {
            ImGui::Text("Export %s.lua?", screen_.name.c_str());
            if (ImGui::Button("Export")) { ExportLua(); showExportDialog_ = false; }
            ImGui::SameLine(); if (ImGui::Button("Cancel")) showExportDialog_ = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

// ═══════════════════════════════════════════════════════════
// PALETTE
// ═══════════════════════════════════════════════════════════

void MenuBuilder::DrawPalette() {
    ImGui::TextDisabled("Widgets"); ImGui::Separator();
    static const WidgetType types[] = {
        Button, Label, Image, Panel, Slider, Checkbox, Dropdown, TextInput,
        ProgressBar, Spacer, TabGroup, ScrollArea, ListBox, ColorPicker
    };
    for (auto t : types) {
        ImGui::PushID((int)t);
        std::string label = std::string(WidgetIcon(t)) + "##" + WidgetTypeName(t);

        // Highlight selected tool (capture bool before button may change it)
        bool isSel = (selectedTool_ == (int)t);
        if (isSel)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));

        if (ImGui::Button(label.c_str(), ImVec2(40, 30))) {
            // Click to select tool (click-to-place mode)
            selectedTool_ = (selectedTool_ == (int)t) ? -1 : (int)t;
            Log(std::string("Tool selected: ") + WidgetTypeName(t));
        }

        if (isSel)
            ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", WidgetTypeName(t));
        if (ImGui::BeginDragDropSource()) {
            int ti = (int)t;
            ImGui::SetDragDropPayload("MENU_WIDGET_TYPE", &ti, sizeof(ti));
            ImGui::Text("%s", WidgetTypeName(t));
            Log(std::string("Drag started: ") + WidgetTypeName(t));
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    if (selectedTool_ >= 0)
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f),
            "Tool: %s", WidgetTypeName((WidgetType)selectedTool_));
    else
        ImGui::TextDisabled("Click tool, then click canvas");
}

// ═══════════════════════════════════════════════════════════
// CANVAS
// ═══════════════════════════════════════════════════════════

void MenuBuilder::DrawCanvasContent() {
    ImVec2 cp = ImGui::GetCursorScreenPos();
    ImVec2 ca = ImGui::GetContentRegionAvail();
    float cw = ca.x - 4, ch = ca.y - 4;
    if (cw < 100) cw = 100; if (ch < 100) ch = 100;

    // Scale: fit-to-view base × user zoom factor (scroll-wheel adjustable)
    float baseScale = std::min(cw / screen_.screenW, ch / screen_.screenH);
    canvasScale_ = baseScale * zoomFactor_;
    canvasW_ = (int)(screen_.screenW * canvasScale_);
    canvasH_ = (int)(screen_.screenH * canvasScale_);
    // Always center canvas in available space
    canvasOffsetX_ = cp.x + (cw - canvasW_) / 2.0f;
    canvasOffsetY_ = cp.y + (ch - canvasH_) / 2.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(canvasOffsetX_, canvasOffsetY_),
        ImVec2(canvasOffsetX_ + canvasW_, canvasOffsetY_ + canvasH_),
        designMode_ ? IM_COL32(30, 30, 40, 255) : IM_COL32(10, 10, 20, 255));

    if (designMode_) {
        int gs = (int)(50 * canvasScale_);
        for (float x = canvasOffsetX_; x < canvasOffsetX_ + canvasW_; x += gs)
            dl->AddLine(ImVec2(x, canvasOffsetY_), ImVec2(x, canvasOffsetY_ + canvasH_), IM_COL32(50, 50, 60, 60));
        for (float y = canvasOffsetY_; y < canvasOffsetY_ + canvasH_; y += gs)
            dl->AddLine(ImVec2(canvasOffsetX_, y), ImVec2(canvasOffsetX_ + canvasW_, y), IM_COL32(50, 50, 60, 60));
    }

    dl->AddRect(ImVec2(canvasOffsetX_, canvasOffsetY_),
        ImVec2(canvasOffsetX_ + canvasW_, canvasOffsetY_ + canvasH_), IM_COL32(100, 100, 120, 150));

    for (auto& w : screen_.widgets)
        if (w.visible) DrawWidgetOnCanvas(w);

    if (showSnapGuides_) DrawSnapGuides();
    if (marqueeSelect_) DrawMarqueeRect();

    // Drag-drop target — accept widgets dropped from palette
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MENU_WIDGET_TYPE");
        if (p) {
            int ti = *(const int*)p->Data;
            ImVec2 m = ImGui::GetMousePos();
            int cx = (int)((m.x - canvasOffsetX_) / canvasScale_);
            int cy = (int)((m.y - canvasOffsetY_) / canvasScale_);
            AddWidget((WidgetType)ti, cx, cy);
        }
        ImGui::EndDragDropTarget();
    }

    HandleCanvasInput();

    ImVec2 ip(canvasOffsetX_ + 4, canvasOffsetY_ + canvasH_ - 18);
    dl->AddText(ip, IM_COL32(100, 100, 120, 200),
        (std::to_string(screen_.screenW) + "x" + std::to_string(screen_.screenH)
         + " | " + std::to_string(screen_.widgets.size()) + " widgets"
         + " | " + std::to_string((int)(zoomFactor_ * 100)) + "%").c_str());
}

ImVec2 MenuBuilder::AnchorToCanvasPos(const WidgetDef& w) const {
    float sx = 0, sy = 0;
    float sw = (float)canvasW_, sh = (float)canvasH_;
    float ww = w.width * canvasScale_, wh = w.height * canvasScale_;
    switch (w.anchor) {
        case Anchor::TopLeft: break;
        case Anchor::TopCenter: sx = sw/2 - ww/2; break;
        case Anchor::TopRight: sx = sw - ww; break;
        case Anchor::CenterLeft: sy = sh/2 - wh/2; break;
        case Anchor::Center: sx = sw/2 - ww/2; sy = sh/2 - wh/2; break;
        case Anchor::CenterRight: sx = sw - ww; sy = sh/2 - wh/2; break;
        case Anchor::BottomLeft: sy = sh - wh; break;
        case Anchor::BottomCenter: sx = sw/2 - ww/2; sy = sh - wh; break;
        case Anchor::BottomRight: sx = sw - ww; sy = sh - wh; break;
    }
    sx += w.offsetX * canvasScale_; sy += w.offsetY * canvasScale_;
    return ImVec2(canvasOffsetX_ + sx, canvasOffsetY_ + sy);
}

void MenuBuilder::DrawWidgetOnCanvas(const WidgetDef& w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = AnchorToCanvasPos(w);
    float ww = w.width * canvasScale_, wh = w.height * canvasScale_;
    bool sel = selectedWidgets_.count(w.id) > 0;

    ImU32 fill, border;
    if (designMode_) {
        fill = sel ? IM_COL32(60, 120, 200, 120) : IM_COL32(50, 55, 70, 100);
        border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(100, 110, 130, 180);
    } else {
        fill = (w.type == Button) ? IM_COL32(50, 100, 180, 220) :
               (w.type == Panel)  ? IM_COL32(25, 25, 40, 200)  :
                                    IM_COL32(35, 35, 50, 180);
        border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(80, 80, 100, 120);
    }

    dl->AddRectFilled(pos, ImVec2(pos.x + ww, pos.y + wh), fill, 4.0f * canvasScale_);
    dl->AddRect(pos, ImVec2(pos.x + ww, pos.y + wh), border, 0, 0, 1.5f);

    std::string label = designMode_ ?
        (std::string(WidgetIcon(w.type)) + " " + w.name) :
        (w.text.empty() ? w.name : w.text);
    if (ww > 20 && wh > 10)
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * canvasScale_ * 0.6f,
            ImVec2(pos.x + 3, pos.y + wh * 0.3f), IM_COL32(220, 220, 220, 255), label.c_str());

    if (sel) DrawResizeHandles(w);
}

void MenuBuilder::DrawResizeHandles(const WidgetDef& w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = AnchorToCanvasPos(w);
    float ww = w.width * canvasScale_, wh = w.height * canvasScale_, hs = 5;
    ImU32 hc = IM_COL32(255, 200, 50, 255);
    ImVec2 h[8] = {{pos.x, pos.y}, {pos.x+ww/2, pos.y}, {pos.x+ww, pos.y},
        {pos.x+ww, pos.y+wh/2}, {pos.x+ww, pos.y+wh}, {pos.x+ww/2, pos.y+wh},
        {pos.x, pos.y+wh}, {pos.x, pos.y+wh/2}};
    for (int i = 0; i < 8; ++i)
        dl->AddRectFilled(ImVec2(h[i].x-hs/2, h[i].y-hs/2),
            ImVec2(h[i].x+hs/2, h[i].y+hs/2), hc);
}

void MenuBuilder::DrawSnapGuides() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (snapLineH_.x > 0)
        dl->AddLine(ImVec2(canvasOffsetX_, snapLineH_.y),
            ImVec2(canvasOffsetX_ + canvasW_, snapLineH_.y), IM_COL32(255, 100, 100, 150));
    if (snapLineV_.y > 0)
        dl->AddLine(ImVec2(snapLineV_.x, canvasOffsetY_),
            ImVec2(snapLineV_.x, canvasOffsetY_ + canvasH_), IM_COL32(255, 100, 100, 150));
}

void MenuBuilder::DrawMarqueeRect() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a(marqueeStart_.x, marqueeStart_.y), b(marqueeEnd_.x, marqueeEnd_.y);
    dl->AddRectFilled(a, b, IM_COL32(100, 150, 255, 40));
    dl->AddRect(a, b, IM_COL32(100, 150, 255, 180));
}

// ═══════════════════════════════════════════════════════════
// CANVAS INPUT
// ═══════════════════════════════════════════════════════════

int MenuBuilder::HitTest(int mx, int my) const {
    for (int i = (int)screen_.widgets.size() - 1; i >= 0; --i) {
        auto& w = screen_.widgets[i];
        if (!w.visible || w.locked) continue;
        ImVec2 pos = AnchorToCanvasPos(w);
        float ww = w.width * canvasScale_, wh = w.height * canvasScale_;
        if (mx >= pos.x && mx <= pos.x + ww && my >= pos.y && my <= pos.y + wh) return i;
    }
    return -1;
}

// Returns resize handle index (0-7) if (mx,my) is near a handle of widget at idx, else -1.
// 0=TL 1=TC 2=TR  3=CR  4=BR  5=BC  6=BL  7=CL
int MenuBuilder::HitTestResizeHandles(int idx, int mx, int my) const {
    if (idx < 0 || idx >= (int)screen_.widgets.size()) return -1;
    const auto& w = screen_.widgets[idx];
    ImVec2 pos = AnchorToCanvasPos(w);
    float ww = w.width * canvasScale_, wh = w.height * canvasScale_;
    float hs = 6.0f;  // hit radius (pixels)
    ImVec2 h[8] = {
        {pos.x, pos.y}, {pos.x+ww/2, pos.y}, {pos.x+ww, pos.y},
        {pos.x+ww, pos.y+wh/2}, {pos.x+ww, pos.y+wh}, {pos.x+ww/2, pos.y+wh},
        {pos.x, pos.y+wh}, {pos.x, pos.y+wh/2}
    };
    for (int i = 0; i < 8; ++i)
        if (fabsf(mx - h[i].x) <= hs && fabsf(my - h[i].y) <= hs) return i;
    return -1;
}

void MenuBuilder::HandleCanvasInput() {
    ImVec2 mouse = ImGui::GetMousePos();
    int mx = (int)mouse.x, my = (int)mouse.y;
    bool inCanvas = (mx >= canvasOffsetX_ && mx <= canvasOffsetX_ + canvasW_
                     && my >= canvasOffsetY_ && my <= canvasOffsetY_ + canvasH_);
    if (!inCanvas) { hoveredWidget_ = -1; return; }
    hoveredWidget_ = HitTest(mx, my);
    bool ctrl = ImGui::GetIO().KeyCtrl, shift = ImGui::GetIO().KeyShift;

    // ── Resize cursor feedback ────────────────────────────
    if (resizing_) {
        static const ImGuiMouseCursor rCur[8] = {
            ImGuiMouseCursor_ResizeNWSE, ImGuiMouseCursor_ResizeNS,  ImGuiMouseCursor_ResizeNESW,
            ImGuiMouseCursor_ResizeEW,   ImGuiMouseCursor_ResizeNWSE, ImGuiMouseCursor_ResizeNS,
            ImGuiMouseCursor_ResizeNESW, ImGuiMouseCursor_ResizeEW
        };
        ImGui::SetMouseCursor(rCur[resizeHandle_]);
    } else if (hoveredWidget_ >= 0 && selectedWidgets_.count(screen_.widgets[hoveredWidget_].id) > 0) {
        int h = HitTestResizeHandles(hoveredWidget_, mx, my);
        if (h >= 0) {
            static const ImGuiMouseCursor hCur[8] = {
                ImGuiMouseCursor_ResizeNWSE, ImGuiMouseCursor_ResizeNS,  ImGuiMouseCursor_ResizeNESW,
                ImGuiMouseCursor_ResizeEW,   ImGuiMouseCursor_ResizeNWSE, ImGuiMouseCursor_ResizeNS,
                ImGuiMouseCursor_ResizeNESW, ImGuiMouseCursor_ResizeEW
            };
            ImGui::SetMouseCursor(hCur[h]);
        }
    }

    // ── Scroll-wheel zoom (centered on canvas) ──────────
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
        zoomFactor_ += wheel * 0.1f;
        zoomFactor_ = std::max(0.1f, std::min(2.0f, zoomFactor_));
    }

    if (ImGui::IsMouseClicked(0)) {
        int hit = HitTest(mx, my);
        if (hit >= 0) {
            auto& w = screen_.widgets[hit];
            bool wasSel = selectedWidgets_.count(w.id) > 0;

            // Update selection
            if (ctrl) {
                if (wasSel) selectedWidgets_.erase(w.id); else selectedWidgets_.insert(w.id);
            } else if (shift) {
                selectedWidgets_.insert(w.id);
            } else if (!wasSel) {
                selectedWidgets_.clear();
                selectedWidgets_.insert(w.id);
            }

            // Check resize handles first (only if widget was already selected)
            int handle = wasSel ? HitTestResizeHandles(hit, mx, my) : -1;
            if (handle >= 0) {
                StartResizing(hit, handle, mx, my);
            } else {
                StartMoving(hit, mx, my);
            }
        } else if (selectedTool_ >= 0) {
            // Click-to-place: add widget from selected palette tool
            int cx = (int)((mx - canvasOffsetX_) / canvasScale_);
            int cy = (int)((my - canvasOffsetY_) / canvasScale_);
            AddWidget((WidgetType)selectedTool_, cx, cy);
            selectedTool_ = -1;  // deselect tool after placement
        } else {
            if (!ctrl && !shift) selectedWidgets_.clear();
            marqueeSelect_ = true; marqueeStart_ = mouse; marqueeEnd_ = mouse;
        }
    }

    if (dragging_ && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(0);
        int dx = (int)(delta.x / canvasScale_), dy = (int)(delta.y / canvasScale_);
        for (int id : selectedWidgets_)
            for (auto& w : screen_.widgets) if (w.id == id) { w.offsetX += dx; w.offsetY += dy; break; }
        ImGui::ResetMouseDragDelta(0);
    }

    if (resizing_ && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(0);
        int dw = (int)(delta.x / canvasScale_), dh = (int)(delta.y / canvasScale_);
        if (!selectedWidgets_.empty()) {
            int id = *selectedWidgets_.begin();
            for (auto& w : screen_.widgets) if (w.id == id) {
                // Directional resize based on handle index
                // 0=TL 1=TC 2=TR  3=CR  4=BR  5=BC  6=BL  7=CL
                switch (resizeHandle_) {
                    case 0: w.offsetX += dw; w.width -= dw; w.offsetY += dh; w.height -= dh; break;
                    case 1: w.offsetY += dh; w.height -= dh; break;
                    case 2: w.width += dw; w.offsetY += dh; w.height -= dh; break;
                    case 3: w.width += dw; break;
                    case 4: w.width += dw; w.height += dh; break;
                    case 5: w.height += dh; break;
                    case 6: w.offsetX += dw; w.width -= dw; w.height += dh; break;
                    case 7: w.offsetX += dw; w.width -= dw; break;
                    default: w.width += dw; w.height += dh; break;
                }
                // Minimum size
                if (w.width < 20) { w.offsetX -= (20 - w.width) * ((resizeHandle_ == 0 || resizeHandle_ == 6 || resizeHandle_ == 7) ? 1 : 0); w.width = 20; }
                if (w.height < 14) { w.offsetY -= (14 - w.height) * ((resizeHandle_ == 0 || resizeHandle_ == 1 || resizeHandle_ == 2) ? 1 : 0); w.height = 14; }
                break;
            }
        }
        ImGui::ResetMouseDragDelta(0);
    }

    if (marqueeSelect_ && ImGui::IsMouseDragging(0)) marqueeEnd_ = mouse;

    if (ImGui::IsMouseReleased(0)) {
        if (marqueeSelect_) {
            float x1 = std::min(marqueeStart_.x, marqueeEnd_.x), y1 = std::min(marqueeStart_.y, marqueeEnd_.y);
            float x2 = std::max(marqueeStart_.x, marqueeEnd_.x), y2 = std::max(marqueeStart_.y, marqueeEnd_.y);
            if (x2-x1 > 4 && y2-y1 > 4) {
                if (!ctrl && !shift) selectedWidgets_.clear();
                for (auto& w : screen_.widgets) {
                    ImVec2 p = AnchorToCanvasPos(w);
                    float ww = w.width * canvasScale_, wh = w.height * canvasScale_;
                    if (p.x+ww >= x1 && p.x <= x2 && p.y+wh >= y1 && p.y <= y2) selectedWidgets_.insert(w.id);
                }
            }
            marqueeSelect_ = false;
        }
        dragging_ = false; resizing_ = false; showSnapGuides_ = false;
    }

    if (ImGui::IsMouseClicked(1) && inCanvas) ImGui::OpenPopup("##canvasCtx");
    if (ImGui::BeginPopup("##canvasCtx")) {
        if (!selectedWidgets_.empty()) {
            if (ImGui::MenuItem("Bring to Front")) BringToFront(*selectedWidgets_.begin());
            if (ImGui::MenuItem("Send to Back")) SendToBack(*selectedWidgets_.begin());
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete Selected") && !selectedWidgets_.empty()) {
            std::vector<int> rm;
            for (int i = 0; i < (int)screen_.widgets.size(); ++i)
                if (selectedWidgets_.count(screen_.widgets[i].id)) rm.push_back(i);
            std::sort(rm.rbegin(), rm.rend());
            for (int i : rm) screen_.widgets.erase(screen_.widgets.begin() + i);
            selectedWidgets_.clear();
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !selectedWidgets_.empty()) {
        std::vector<int> rm;
        for (int i = 0; i < (int)screen_.widgets.size(); ++i)
            if (selectedWidgets_.count(screen_.widgets[i].id)) rm.push_back(i);
        std::sort(rm.rbegin(), rm.rend());
        for (int i : rm) screen_.widgets.erase(screen_.widgets.begin() + i);
        selectedWidgets_.clear();
    }
}

void MenuBuilder::StartMoving(int, int mx, int my) {
    dragging_ = true; dragStartPos_ = ImVec2((float)mx, (float)my);
}
void MenuBuilder::StartResizing(int, int h, int mx, int my) {
    resizing_ = true; resizeHandle_ = h; dragStartPos_ = ImVec2((float)mx, (float)my);
}

// ═══════════════════════════════════════════════════════════
// PROPERTIES PANEL
// ═══════════════════════════════════════════════════════════

void MenuBuilder::DrawPropertiesPanel() {
    // Find the first selected widget
    WidgetDef* sel = nullptr;
    for (auto& w : screen_.widgets) {
        if (selectedWidgets_.count(w.id) > 0) { sel = &w; break; }
    }
    if (!sel) return;

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Properties: %s", sel->name.c_str());

    if (ImGui::BeginTable("##wprops", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

        // Name
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1);
        char nameBuf[128];
        strncpy(nameBuf, sel->name.c_str(), sizeof(nameBuf)-1);
        nameBuf[sizeof(nameBuf)-1] = 0;
        if (ImGui::InputText("##wname", nameBuf, sizeof(nameBuf)))
            sel->name = nameBuf;

        // Type (read-only)
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Type");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s %s", WidgetIcon(sel->type), WidgetTypeName(sel->type));

        // Position
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Position");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("X##wposx", &sel->offsetX);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("Y##wposy", &sel->offsetY);

        // Size
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Size");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("W##wsizeW", &sel->width);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("H##wsizeH", &sel->height);
        if (sel->width < 20) sel->width = 20;
        if (sel->height < 14) sel->height = 14;

        // Anchor
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Anchor");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1);
        const char* anchorNames[] = {"Top-Left","Top-Center","Top-Right",
            "Center-Left","Center","Center-Right",
            "Bottom-Left","Bottom-Center","Bottom-Right"};
        int aidx = (int)sel->anchor;
        if (ImGui::Combo("##wanchor", &aidx, anchorNames, 9))
            sel->anchor = (Anchor)aidx;

        // Text
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Text");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1);
        char txtBuf[256];
        strncpy(txtBuf, sel->text.c_str(), sizeof(txtBuf)-1);
        txtBuf[sizeof(txtBuf)-1] = 0;
        if (ImGui::InputText("##wtext", txtBuf, sizeof(txtBuf)))
            sel->text = txtBuf;

        // Visible / Locked
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Flags");
        ImGui::TableSetColumnIndex(1);
        ImGui::Checkbox("Visible", &sel->visible);
        ImGui::SameLine();
        ImGui::Checkbox("Locked", &sel->locked);

        ImGui::EndTable();
    }
}

// ═══════════════════════════════════════════════════════════
// HIERARCHY TREE
// ═══════════════════════════════════════════════════════════

void MenuBuilder::DrawHierarchyTree() {
    ImGui::TextDisabled("Hierarchy (%zu widgets)", screen_.widgets.size()); ImGui::Separator();
    ImGui::BeginChild("##hierList", ImVec2(0, std::max(50.f, ImGui::GetContentRegionAvail().y - 105)), false);
    auto roots = GetChildrenOf(-1);
    for (int idx : roots) DrawHierarchyNode(idx, 0);
    if (screen_.widgets.empty())
        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1), "Drag widgets from palette\nonto the canvas.");
    ImGui::EndChild();
    ImGui::Separator();
    if (ImGui::Button("Anchor Presets")) showAnchorPresets_ = !showAnchorPresets_;
    if (showAnchorPresets_) {
        const char* names[9] = {"TL","TC","TR","CL","C","CR","BL","BC","BR"};
        Anchor vals[9] = {Anchor::TopLeft,Anchor::TopCenter,Anchor::TopRight,
            Anchor::CenterLeft,Anchor::Center,Anchor::CenterRight,
            Anchor::BottomLeft,Anchor::BottomCenter,Anchor::BottomRight};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                int i = r*3+c; if (c>0) ImGui::SameLine();
                bool active = false;
                if (!selectedWidgets_.empty()) {
                    int sid = *selectedWidgets_.begin();
                    for (auto& w : screen_.widgets) if (w.id == sid) { active = (w.anchor == vals[i]); break; }
                }
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1));
                if (ImGui::Button(names[i], ImVec2(22, 22))) {
                    for (int sid : selectedWidgets_) for (auto& w : screen_.widgets) if (w.id == sid) w.anchor = vals[i];
                }
                if (active) ImGui::PopStyleColor();
            }
        }
    }
}

void MenuBuilder::DrawHierarchyNode(int idx, int depth) {
    if (idx < 0 || idx >= (int)screen_.widgets.size()) return;
    auto& w = screen_.widgets[idx];
    ImGui::PushID(w.id);
    if (depth > 0) ImGui::Indent(14);

    const char* eye = w.visible ? "[+]" : "[ ]";
    if (ImGui::SmallButton(eye)) w.visible = !w.visible; ImGui::SameLine();
    const char* lock = w.locked ? "{L}" : "{ }";
    if (ImGui::SmallButton(lock)) w.locked = !w.locked; ImGui::SameLine();

    std::string label = std::string(WidgetIcon(w.type)) + " " + w.name;
    bool sel = selectedWidgets_.count(w.id) > 0;
    if (ImGui::Selectable(label.c_str(), sel)) {
        if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) selectedWidgets_.clear();
        selectedWidgets_.insert(w.id);
    }

    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("MENU_REPARENT", &w.id, sizeof(w.id));
        ImGui::Text("Move: %s", w.name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MENU_REPARENT");
        if (p) { int cid = *(const int*)p->Data; for (auto& cw : screen_.widgets) if (cw.id == cid) { cw.parentId = w.id; break; } }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) { screen_.widgets.erase(screen_.widgets.begin() + idx); selectedWidgets_.clear(); ImGui::EndPopup(); ImGui::PopID(); return; }
        ImGui::EndPopup();
    }

    auto children = GetChildrenOf(w.id);
    for (int c : children) DrawHierarchyNode(c, depth + 1);
    if (depth > 0) ImGui::Unindent(14);
    ImGui::PopID();
}

// ═══════════════════════════════════════════════════════════
// WIDGET CRUD
// ═══════════════════════════════════════════════════════════

void MenuBuilder::AddWidget(WidgetType type, int x, int y) {
    WidgetDef w; w.id = widgetIdCounter_++; w.type = type;
    w.name = std::string(WidgetTypeName(type)) + "_" + std::to_string(w.id);
    w.width = 200; w.height = 40;
    if (type == Image) { w.width = 128; w.height = 128; }
    if (type == Panel) { w.width = 300; w.height = 200; }
    if (type == Checkbox) { w.width = 160; w.height = 24; }
    if (type == Slider || type == ProgressBar) { w.width = 200; w.height = 30; }
    if (type == Spacer) { w.width = 40; w.height = 40; }
    w.anchor = Anchor::TopLeft; w.offsetX = x; w.offsetY = y;
    w.text = WidgetTypeName(type);
    screen_.widgets.push_back(w);
    selectedWidgets_.clear(); selectedWidgets_.insert(w.id);
    Log("Added widget: " + w.name + " (" + std::to_string(screen_.widgets.size()) + " total)");
}

void MenuBuilder::RemoveWidget(int i) {
    if (i >= 0 && i < (int)screen_.widgets.size()) {
        selectedWidgets_.erase(screen_.widgets[i].id);
        screen_.widgets.erase(screen_.widgets.begin() + i);
    }
}

void MenuBuilder::BringToFront(int i) {
    if (i >= 0 && i < (int)screen_.widgets.size()) {
        WidgetDef w = screen_.widgets[i];
        screen_.widgets.erase(screen_.widgets.begin() + i);
        screen_.widgets.push_back(w);
    }
}

void MenuBuilder::SendToBack(int i) {
    if (i >= 0 && i < (int)screen_.widgets.size()) {
        WidgetDef w = screen_.widgets[i];
        screen_.widgets.erase(screen_.widgets.begin() + i);
        screen_.widgets.insert(screen_.widgets.begin(), w);
    }
}

// ═══════════════════════════════════════════════════════════
// FILE I/O
// ═══════════════════════════════════════════════════════════

void MenuBuilder::NewScreen() { screen_.widgets.clear(); selectedWidgets_.clear(); }

void MenuBuilder::RefreshScreenList() {
    screenList_.clear();
    std::string dir = MenusDir(); mkdir(dir.c_str(), 0755);
    DIR* d = opendir(dir.c_str()); if (!d) return;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string n(entry->d_name);
        if (entry->d_type == DT_REG && n.size() > 5 && n.substr(n.size()-5) == ".json")
            screenList_.push_back(n.substr(0, n.size()-5));
    }
    closedir(d);
    std::sort(screenList_.begin(), screenList_.end());
}

void MenuBuilder::LoadScreen(const std::string& name) {
    std::string path = MenusDir() + "/" + name + ".json";
    std::ifstream f(path); if (!f.good()) { Log("Not found: "+path); return; }
    json j = json::parse(f);
    screen_.name = j.value("name", name);
    screen_.background = j.value("background", "");
    screen_.music = j.value("music", "");
    screen_.screenW = j.value("screenW", 1920);
    screen_.screenH = j.value("screenH", 1080);
    screen_.widgets.clear();
    for (auto& wj : j["widgets"]) {
        WidgetDef w;
        w.id = wj.value("id", 0); w.parentId = wj.value("parentId", -1);
        w.type = (WidgetType)wj.value("type", 0); w.name = wj.value("name", "");
        w.anchor = (Anchor)wj.value("anchor", 4);
        w.offsetX = wj.value("offsetX", 0); w.offsetY = wj.value("offsetY", 0);
        w.width = wj.value("width", 200); w.height = wj.value("height", 40);
        w.text = wj.value("text", ""); w.image = wj.value("image", "");
        w.fontSize = wj.value("fontSize", 18);
        if (wj.contains("color")) {
            w.colorR = wj["color"][0]; w.colorG = wj["color"][1];
            w.colorB = wj["color"][2]; w.colorA = wj["color"][3];
        }
        w.onClick = wj.value("onClick", ""); w.binding = wj.value("binding", "");
        w.minVal = wj.value("minVal", 0.f); w.maxVal = wj.value("maxVal", 100.f);
        w.curVal = wj.value("curVal", 50.f); w.checked = wj.value("checked", false);
        w.visible = wj.value("visible", true); w.locked = wj.value("locked", false);
        screen_.widgets.push_back(w);
    }
    widgetIdCounter_ = screen_.widgets.empty() ? 1 : screen_.widgets.back().id + 1;
    selectedWidgets_.clear();
    Log("Loaded: " + path + " (" + std::to_string(screen_.widgets.size()) + " widgets)");
}

void MenuBuilder::SaveScreen() {
    std::string dir = MenusDir(); mkdir(dir.c_str(), 0755);
    json j;
    j["name"] = screen_.name; j["background"] = screen_.background;
    j["music"] = screen_.music; j["screenW"] = screen_.screenW;
    j["screenH"] = screen_.screenH;
    json wa = json::array();
    for (auto& w : screen_.widgets) {
        json wj;
        wj["id"] = w.id; wj["parentId"] = w.parentId;
        wj["type"] = (int)w.type; wj["name"] = w.name;
        wj["anchor"] = (int)w.anchor;
        wj["offsetX"] = w.offsetX; wj["offsetY"] = w.offsetY;
        wj["width"] = w.width; wj["height"] = w.height;
        if (!w.text.empty()) wj["text"] = w.text;
        if (!w.image.empty()) wj["image"] = w.image;
        wj["fontSize"] = w.fontSize;
        wj["color"] = {w.colorR, w.colorG, w.colorB, w.colorA};
        if (!w.onClick.empty()) wj["onClick"] = w.onClick;
        if (!w.binding.empty()) wj["binding"] = w.binding;
        wj["minVal"] = w.minVal; wj["maxVal"] = w.maxVal;
        wj["curVal"] = w.curVal; wj["checked"] = w.checked;
        wj["visible"] = w.visible; wj["locked"] = w.locked;
        wa.push_back(wj);
    }
    j["widgets"] = wa;
    std::string path = dir + "/" + screen_.name + ".json";
    std::ofstream f(path); f << j.dump(2); f.close();
    Log("Saved: " + path);
}

void MenuBuilder::ExportLua() {
    std::string dir = MenusDir(); mkdir(dir.c_str(), 0755);
    std::string lua = "-- " + screen_.name + ".lua\nlocal M={}\nfunction M.Create(ui)\n";
    lua += "  ui:BeginScreen(\"" + screen_.name + "\")\n";
    for (auto& w : screen_.widgets) {
        lua += "  ui:Add" + std::string(WidgetTypeName(w.type)) + "{\n";
        lua += "    id=\"" + w.name + "\",\n";
        lua += "    anchor=\"" + std::string(AnchorName(w.anchor)) + "\",\n";
        lua += "    offsetX=" + std::to_string(w.offsetX) + ",\n";
        lua += "    offsetY=" + std::to_string(w.offsetY) + ",\n";
        lua += "    width=" + std::to_string(w.width) + ",\n";
        lua += "    height=" + std::to_string(w.height) + ",\n";
        if (!w.text.empty()) lua += "    text=\"" + w.text + "\",\n";
        if (!w.onClick.empty()) lua += "    onClick=\"" + w.onClick + "\",\n";
        lua += "  }\n";
    }
    lua += "  ui:EndScreen()\nend\nreturn M\n";
    std::string path = dir + "/" + screen_.name + ".lua";
    std::ofstream f(path); f << lua; f.close();
    Log("Exported: " + path);
}

} // namespace beigebox
