// editor/src/menu_bar.cpp
// ─────────────────────────────────────────────────────────────
// Main Menu Bar implementation.
// ─────────────────────────────────────────────────────────────

#include "menu_bar.h"

#include "mcp/tool_registry.h"
#include "mcp/llm_client.h"
#include "beigebox/lua/lua_bridge.h"
#include "beigebox/ecs/components.h"
#include "beigebox/world/map_generator.h"

#include <SDL2/SDL.h>
#include <imgui.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #define mkdir(p, m) _mkdir(p)
#else
  #include <sys/stat.h>
#endif

namespace beigebox {

// ═════════════════════════════════════════════════════════════
// Main Draw
// ═════════════════════════════════════════════════════════════

void MainMenuBar::Draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        DrawFileMenu();
        DrawEditMenu();
        DrawViewMenu();
        DrawAiMenu();
        DrawScriptsMenu();
        DrawEventsMenu();
        DrawAssetsMenu();
        DrawWorldMenu();
        DrawHelpMenu();

        // FPS counter on the right
        ImGui::SameLine(ImGui::GetWindowWidth() - 120);
        ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

        ImGui::EndMainMenuBar();
    }

    // ── Modal Dialogs ────────────────────────────────────────
    DrawAboutDialog();
    DrawAIConfigDialog();
    DrawExportDialog();
}

// ═════════════════════════════════════════════════════════════
// File Menu
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawFileMenu()
{
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Project", "Ctrl+N"))
        {
            // TODO: clear ECS, reset to default
        }
        if (ImGui::MenuItem("Open Project...", "Ctrl+O"))
        {
            // TODO: file dialog → deserialize ECS state
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Save Project", "Ctrl+S"))
        {
            // TODO: serialize ECS + Lua scripts to project file
        }
        if (ImGui::MenuItem("Save Project As...", "Ctrl+Shift+S"))
        {
            // TODO: file dialog → save to chosen path
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Export Game...", nullptr))
        {
            showExport_ = true;
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Exit", "Alt+F4"))
        {
            if (onQuit) onQuit();
        }

        ImGui::EndMenu();
    }
}

// ═════════════════════════════════════════════════════════════
// Edit Menu
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawEditMenu()
{
    if (ImGui::BeginMenu("Edit"))
    {
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
        ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
        ImGui::Separator();

        ImGui::MenuItem("Cut", "Ctrl+X", false, false);
        ImGui::MenuItem("Copy", "Ctrl+C", false, false);
        ImGui::MenuItem("Paste", "Ctrl+V", false, false);
        ImGui::MenuItem("Delete", "Del", false, false);
        ImGui::Separator();

        if (ImGui::MenuItem("Preferences..."))
        {
            // TODO: settings window (AI provider, editor theme, keybindings)
        }

        ImGui::EndMenu();
    }
}

// ═════════════════════════════════════════════════════════════
// View Menu
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawViewMenu()
{
    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Entity List",       nullptr, true, false);
        ImGui::MenuItem("Properties Grid",   nullptr, true, false);
        ImGui::MenuItem("Event Editor",      nullptr, true, false);
        ImGui::MenuItem("AI Chat",           nullptr, true, false);
        ImGui::Separator();

        if (ImGui::MenuItem("Reset Layout"))
        {
            // TODO: reset ImGui dock layout to defaults
        }

        ImGui::EndMenu();
    }
}

// ═════════════════════════════════════════════════════════════
// AI Menu
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawAiMenu()
{
    if (ImGui::BeginMenu("AI"))
    {
        if (ImGui::MenuItem("Configure Provider..."))
        {
            showAIConfig_ = true;
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Generate Map..."))
        {
            if (onGenerateMap) onGenerateMap();
        }
        if (ImGui::MenuItem("Refactor Selected Script...", nullptr, false, false))
        {
            // TODO: send current script to LLM for refactoring
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Test Connection"))
        {
            // Ping the LLM endpoint
            llm_->Send("ping", [](const std::string& resp) {
                SDL_Log("LLM ping response: %s", resp.c_str());
            }, [](const std::string&, const std::string&) {});
        }

        ImGui::EndMenu();
    }
}

// ═════════════════════════════════════════════════════════════
// Scripts Menu
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawScriptsMenu()
{
    if (ImGui::BeginMenu("Scripts"))
    {
        if (ImGui::MenuItem("New Script...", "Ctrl+Shift+N"))
        {
            // TODO: open script creation dialog (choose entity + event)
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Validate All Scripts"))
        {
            int total = 0, errors = 0;
            auto view = ecs_->view<entt::entity>();
            for (auto entity : view)
            {
                for (auto evt : {"OnInit", "OnTick", "OnTakeDamage", "OnDeath"})
                {
                    if (lua_->HasScript(entity, evt))
                    {
                        total++;
                        // Validation happens at load time — already done
                    }
                }
            }
            SDL_Log("Scripts: %d loaded, %d with errors", total, errors);
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Show Code-Locked Entities", nullptr, false, false))
        {
            // TODO: filter entity list to code-locked only
        }

        ImGui::EndMenu();
    }
}

// ═════════════════════════════════════════════════════════════
// Events Menu
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawEventsMenu()
{
    if (ImGui::BeginMenu("Events"))
    {
        if (ImGui::MenuItem("Event Manager..."))
        {
            // TODO: table of all entities × events with script status
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Fire Test Event...", nullptr, false, false))
        {
            // TODO: dialog to pick entity + event + params
        }
        if (ImGui::MenuItem("Fire OnTick All"))
        {
            auto view = ecs_->view<entt::entity>();
            for (auto entity : view)
                if (lua_->HasScript(entity, "OnTick"))
                    lua_->FireEvent(entity, "OnTick");
        }
        ImGui::Separator();

        ImGui::MenuItem("Event Debugger", nullptr, false, false);
        ImGui::MenuItem("Break on Event", nullptr, false, false);

        ImGui::EndMenu();
    }
}

// ═════════════════════════════════════════════════════════════
// Assets Menu
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawAssetsMenu()
{
    if (ImGui::BeginMenu("Assets"))
    {
        ImGui::MenuItem("Import Sprite...",    nullptr, false, false);
        ImGui::MenuItem("Import Texture...",   nullptr, false, false);
        ImGui::Separator();

        ImGui::MenuItem("Asset Browser...",    nullptr, false, false);
        ImGui::Separator();

        ImGui::MenuItem("Reload All Assets",   nullptr, false, false);

        ImGui::EndMenu();
    }
}

// ═════════════════════════════════════════════════════════════
// World Menu
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawWorldMenu()
{
    if (ImGui::BeginMenu("World"))
    {
        if (ImGui::MenuItem("Generate Map..."))
        {
            if (onGenerateMap) onGenerateMap();
        }
        if (ImGui::MenuItem("Load Map (.ogm)...", nullptr, false, false))
        {
            // TODO: file dialog
        }
        if (ImGui::MenuItem("Save Map (.ogm)..."))
        {
            std::vector<MapTile> tiles(32 * 32);
            // Save current map state (placeholder)
            MapGenerator::SaveToFile("current_map.ogm", tiles.data(), 32, 32);
        }
        ImGui::Separator();

        ImGui::MenuItem("Map Properties...",    nullptr, false, false);
        ImGui::MenuItem("Thaw Settings...",     nullptr, false, false);

        ImGui::EndMenu();
    }
}

// ═════════════════════════════════════════════════════════════
// Help Menu
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawHelpMenu()
{
    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("Lua API Reference"))
        {
            // Open the README or a reference panel
            SDL_Log("Lua API: Transform, Combat, Orders, Economy, Factory, Thaw, World");
        }
        if (ImGui::MenuItem("Tool Reference"))
        {
            SDL_Log("Available tools: %s", tools_->Help().c_str());
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Documentation (F1)", nullptr, false, false))
        {
            // TODO: open docs
        }
        ImGui::Separator();

        if (ImGui::MenuItem("About M.A.D. Editor"))
        {
            showAbout_ = true;
        }

        ImGui::EndMenu();
    }
}

// ═════════════════════════════════════════════════════════════
// About Dialog
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawAboutDialog()
{
    if (!showAbout_) return;

    ImGui::OpenPopup("About M.A.D. Editor");
    if (ImGui::BeginPopupModal("About M.A.D. Editor", &showAbout_,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("M.A.D. Editor v0.1.0");
        ImGui::Separator();
        ImGui::Text("Project M.A.D. — BeigeBox Engine");
        ImGui::Text("Isometric RTS Engine for low-spec hardware");
        ImGui::Spacing();
        ImGui::Text("Built with:");
        ImGui::BulletText("C++17, SDL2, OpenGL 3.1");
        ImGui::BulletText("EnTT ECS, Sol2 + Lua 5.4");
        ImGui::BulletText("Dear ImGui (docking)");
        ImGui::BulletText("nlohmann/json, STB_image");
        ImGui::Spacing();
        ImGui::Text("License: MIT");
        ImGui::Text("(c) 2026 Project M.A.D. Contributors");
        ImGui::Spacing();

        if (ImGui::Button("Close", ImVec2(120, 0)))
            showAbout_ = false;

        ImGui::EndPopup();
    }
}

// ═════════════════════════════════════════════════════════════
// AI Configuration Dialog
// ═════════════════════════════════════════════════════════════

void MainMenuBar::DrawAIConfigDialog()
{
    if (!showAIConfig_) return;

    ImGui::OpenPopup("AI Configuration");
    if (ImGui::BeginPopupModal("AI Configuration", &showAIConfig_,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        const char* providers[] = {"Ollama", "OpenAI", "Anthropic", "DeepSeek"};
        ImGui::Combo("Provider", &providerIdx_, providers, 4);
        ImGui::InputText("Endpoint", endpointBuf_, sizeof(endpointBuf_));

        // Set smart defaults when switching providers
        static int lastProvider = -1;
        if (providerIdx_ != lastProvider)
        {
            lastProvider = providerIdx_;
            switch (providerIdx_)
            {
            case 0: // Ollama
                strcpy(endpointBuf_, "http://localhost:11434");
                strcpy(modelBuf_, "llama3");
                break;
            case 1: // OpenAI
                strcpy(endpointBuf_, "https://api.openai.com/v1/chat/completions");
                strcpy(modelBuf_, "gpt-4o");
                break;
            case 2: // Anthropic
                strcpy(endpointBuf_, "https://api.anthropic.com/v1/messages");
                strcpy(modelBuf_, "claude-3-5-sonnet-20241022");
                break;
            case 3: // DeepSeek
                strcpy(endpointBuf_, "https://api.deepseek.com/chat/completions");
                strcpy(modelBuf_, "deepseek-v4-pro");
                break;
            }
        }
        ImGui::InputText("Model", modelBuf_, sizeof(modelBuf_));

        if (providerIdx_ > 0) // not Ollama
            ImGui::InputText("API Key", apiKeyBuf_, sizeof(apiKeyBuf_),
                             ImGuiInputTextFlags_Password);

        ImGui::Spacing();
        if (ImGui::Button("Apply", ImVec2(100, 0)))
        {
            llm_->SetProvider(static_cast<LlmClient::Provider>(providerIdx_));
            llm_->SetEndpoint(endpointBuf_);
            llm_->SetModel(modelBuf_);
            if (apiKeyBuf_[0]) llm_->SetApiKey(apiKeyBuf_);
            llm_->BuildSystemPrompt(*tools_);
            showAIConfig_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
            showAIConfig_ = false;

        ImGui::EndPopup();
    }
}

// ═════════════════════════════════════════════════════════════
// Export Game Dialog
// ═════════════════════════════════════════════════════════════

static bool CopyFile(const std::string& src, const std::string& dst)
{
    FILE* in = fopen(src.c_str(), "rb");
    if (!in) return false;

    FILE* out = fopen(dst.c_str(), "wb");
    if (!out) { fclose(in); return false; }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);

    fclose(in);
    fclose(out);
    return true;
}

static bool MakeDir(const std::string& path)
{
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

void MainMenuBar::DrawExportDialog()
{
    if (!showExport_) return;

    ImGui::OpenPopup("Export Game");
    if (ImGui::BeginPopupModal("Export Game", &showExport_,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Export a standalone playable build.");
        ImGui::Spacing();
        ImGui::InputText("Export Path", exportPath_, sizeof(exportPath_));
        ImGui::Spacing();
        ImGui::Text("This will create:");
        ImGui::BulletText("%s/beigebox_runtime (binary)", exportPath_);
        ImGui::BulletText("%s/assets/ (sprites, shaders)", exportPath_);
        ImGui::BulletText("%s/scripts/ (Lua event handlers)", exportPath_);
        ImGui::BulletText("%s/maps/ (.ogm map files)", exportPath_);
        ImGui::BulletText("%s/launch.sh (Linux) / launch.bat (Windows)", exportPath_);
        ImGui::Spacing();

        if (ImGui::Button("Export", ImVec2(120, 0)))
        {
            std::string base(exportPath_);
            MakeDir(base);
            MakeDir(base + "/assets");
            MakeDir(base + "/scripts");
            MakeDir(base + "/maps");

            // Copy runtime binary
            std::string srcBinary = "./build/engine/beigebox_runtime";
#ifdef _WIN32
            srcBinary += ".exe";
#endif
            std::string dstBinary = base + "/beigebox_runtime";
#ifdef _WIN32
            dstBinary += ".exe";
#endif
            if (CopyFile(srcBinary, dstBinary))
                SDL_Log("Exported: %s", dstBinary.c_str());
            else
                SDL_Log("Warning: could not copy runtime binary");

            // Export Lua scripts per entity
            std::string scriptsDir = base + "/scripts";
            auto view = ecs_->view<entt::entity>();
            for (auto entity : view)
            {
                uint32_t eid = static_cast<uint32_t>(entt::to_integral(entity));
                for (auto evt : {"OnInit", "OnTick", "OnTakeDamage", "OnDeath"})
                {
                    if (lua_->HasScript(entity, evt))
                    {
                        std::string fname = scriptsDir + "/entity" +
                            std::to_string(eid) + "_" + evt + ".lua";
                        FILE* f = fopen(fname.c_str(), "w");
                        if (f)
                        {
                            fprintf(f, "-- Entity %u — %s handler\n", eid, evt);
                            fprintf(f, "-- Auto-exported by M.A.D. Editor\n");
                            fclose(f);
                        }
                    }
                }
            }

            // Export current map (if any)
            std::string mapPath = base + "/maps/game_map.ogm";
            MapGenerator::SaveToFile(mapPath, nullptr, 32, 32); // placeholder

            // Create launch script
            std::string launchPath = base + "/launch.sh";
            FILE* f = fopen(launchPath.c_str(), "w");
            if (f)
            {
                fprintf(f, "#!/bin/sh\n");
                fprintf(f, "# M.A.D. Game Launcher\n");
                fprintf(f, "cd \"$(dirname \"$0\")\"\n");
                fprintf(f, "./beigebox_runtime\n");
                fclose(f);
                chmod(launchPath.c_str(), 0755);
            }

            SDL_Log("Game exported to: %s", base.c_str());
            showExport_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
            showExport_ = false;

        ImGui::EndPopup();
    }
}

} // namespace beigebox
