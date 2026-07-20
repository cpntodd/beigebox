# engine/CMakeLists.txt
# ─────────────────────────────────────────────────────────────
# BeigeBox Engine — static library + standalone runtime binary.
# ─────────────────────────────────────────────────────────────

# ── Static Library (linked by editor and runtime) ────────────
add_library(beigebox_engine STATIC
    src/core/fixed_point.cpp
    src/core/job_system.cpp
    src/render/isometric.cpp
    src/render/renderer.cpp
    src/render/texture.cpp
    src/render/sprite_registry.cpp
    src/lua/lua_bridge.cpp
    src/lua/lua_vm_pool.cpp
    src/ecs/systems.cpp
    src/world/thaw_grid.cpp
    src/world/map_generator.cpp
    src/world/spatial_grid.cpp
    src/ai/pathfinding.cpp
    src/net/lockstep.cpp
)

# ── Include paths ────────────────────────────────────────────
target_include_directories(beigebox_engine
    PUBLIC
        include
    PRIVATE
        src
        ${stb_SOURCE_DIR}
)

# ── Compile definitions ─────────────────────────────────────
# GL_GLEXT_PROTOTYPES: force glext.h to declare GL 3.1+ functions
# as real prototypes (needed on Linux with Mesa's libGL.so).
target_compile_definitions(beigebox_engine
    PUBLIC
        GL_GLEXT_PROTOTYPES=1
)

# ── Dependencies ─────────────────────────────────────────────
target_link_libraries(beigebox_engine
    PUBLIC
        SDL2::SDL2
        OpenGL::GL
        Lua54::Lua54
        EnTT::EnTT
        sol2::sol2
)

# ── Standalone Runtime Executable ────────────────────────────
add_executable(beigebox_runtime
    src/main.cpp
)
target_link_libraries(beigebox_runtime
    PRIVATE
        beigebox_engine
)
