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
    # ── Future test files added below ────────────────────────
    # test_lockstep.cpp
)

target_link_libraries(beigebox_tests
    PRIVATE
        beigebox_engine
        doctest::doctest
)

# ── Register with CTest ──────────────────────────────────────
add_test(NAME beigebox_tests COMMAND beigebox_tests)
