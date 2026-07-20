# tests/CMakeLists.txt
# ─────────────────────────────────────────────────────────────
# Unit tests for the BeigeBox Engine.
# Uses doctest (header-only, fetched via FetchContent).
# ─────────────────────────────────────────────────────────────

include(FetchContent)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

# ── Test Executable ──────────────────────────────────────────
add_executable(beigebox_tests
    test_fixed_point.cpp
    test_lua_bridge.cpp
    test_combat_orders.cpp
    test_thaw.cpp
    test_jsonrpc.cpp
    test_map.cpp
    test_lockstep.cpp
    ${CMAKE_SOURCE_DIR}/editor/src/mcp/tool_registry.cpp
    ${CMAKE_SOURCE_DIR}/editor/src/mcp/json_rpc.cpp
)

target_include_directories(beigebox_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/editor/src
)

target_link_libraries(beigebox_tests
    PRIVATE
        beigebox_engine
        doctest::doctest
        nlohmann_json::nlohmann_json
)

# ── Register with CTest ──────────────────────────────────────
add_test(NAME beigebox_tests COMMAND beigebox_tests)
