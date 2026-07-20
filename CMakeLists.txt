# ─────────────────────────────────────────────────────────────
# Project M.A.D. — BeigeBox Engine
# Isometric RTS Engine for low-spec hardware
# ─────────────────────────────────────────────────────────────
cmake_minimum_required(VERSION 3.16)
project(BeigeBox
    VERSION 0.1.0
    DESCRIPTION "Project M.A.D. — Isometric RTS Engine"
    LANGUAGES CXX
)

# ── C++ Standard ─────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── CMake Module Path ────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ── Build Options ────────────────────────────────────────────
option(BUILD_EDITOR "Build the ImGui-based M.A.D. Editor" ON)
option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_LOCKSTEP_DEBUG "Enable deterministic lockstep debugging assertions" OFF)
option(ENABLE_LTO "Enable link-time optimization (release)" ON)
option(TARGET_X86_64_BASELINE "Target x86-64 baseline (no SSE4) for potato PCs" ON)

# ── Release optimization flags ───────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(TARGET_X86_64_BASELINE)
        # x86-64 baseline: circa-2003 Athlon 64 / Pentium 4.
        # SSE2 only — no SSE4, no AVX. Safe for all 64-bit CPUs.
        add_compile_options(-march=x86-64 -mtune=generic)
    else()
        # x86-64-v2: circa-2008 (SSE4.2, POPCNT, CX16). ~10% faster.
        add_compile_options(-march=x86-64-v2 -mtune=generic)
    endif()
    add_compile_options(-O3)

    if(ENABLE_LTO)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    endif()
endif()

# ── System Dependencies ──────────────────────────────────────
# SDL2 — Windowing, input, audio, OpenGL context
find_package(SDL2 REQUIRED)

# OpenGL — Rendering (3.1 core / GLES 2.0 fallback)
find_package(OpenGL REQUIRED)

# Lua 5.4 — Scripting runtime (custom find module for Debian)
find_package(Lua54 REQUIRED)

# ── FetchContent — Header-only / lightweight dependencies ────
include(FetchContent)

# EnTT — ECS (header-only, v3.15+)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.15.0
)
FetchContent_MakeAvailable(entt)

# Sol2 — Lua/C++ binding (header-only, v3.5+)
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(sol2)

# STB — Single-header image loading (public domain)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# Dear ImGui — Editor UI (docking branch)
if(BUILD_EDITOR)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.9-docking
    )
    FetchContent_MakeAvailable(imgui)

    # nlohmann/json — JSON parsing for MCP transport (header-only, MIT)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ── Subdirectories ───────────────────────────────────────────
add_subdirectory(engine)

if(BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
