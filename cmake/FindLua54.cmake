# FindLua54.cmake
# ─────────────────────────────────────────────────────────────
# Custom find module for Lua 5.4.
# Debian trixie does not ship a cmake config for Lua 5.4,
# and the headers live in /usr/include/lua5.4/.
#
# Exports:
#   Lua54_FOUND          — TRUE if found
#   Lua54::Lua54         — imported target
#   LUA54_INCLUDE_DIR    — path to lua.h et al.
#   LUA54_LIBRARY        — path to liblua5.4.so
# ─────────────────────────────────────────────────────────────

find_path(LUA54_INCLUDE_DIR
    NAMES lua.h
    PATH_SUFFIXES
        lua5.4
        lua54
        lua-5.4
        lua
    DOC "Lua 5.4 include directory"
)

find_library(LUA54_LIBRARY
    NAMES
        lua5.4
        lua54
        lua-5.4
        lua
    DOC "Lua 5.4 library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lua54
    REQUIRED_VARS LUA54_LIBRARY LUA54_INCLUDE_DIR
    VERSION_VAR   LUA54_VERSION
)

if(Lua54_FOUND AND NOT TARGET Lua54::Lua54)
    add_library(Lua54::Lua54 UNKNOWN IMPORTED)
    set_target_properties(Lua54::Lua54 PROPERTIES
        IMPORTED_LOCATION "${LUA54_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LUA54_INCLUDE_DIR}"
    )

    # Attempt to detect version from lua.h
    if(LUA54_INCLUDE_DIR)
        file(STRINGS "${LUA54_INCLUDE_DIR}/lua.h" _lua_version_def
            REGEX "^#define[ \t]+LUA_VERSION_[A-Z]+\t+[0-9]+$"
        )
        if(_lua_version_def)
            string(REGEX MATCH "LUA_VERSION_MAJOR\t+([0-9]+)" _ "${_lua_version_def}")
            set(_major ${CMAKE_MATCH_1})
            string(REGEX MATCH "LUA_VERSION_MINOR\t+([0-9]+)" _ "${_lua_version_def}")
            set(_minor ${CMAKE_MATCH_1})
            set(LUA54_VERSION "${_major}.${_minor}" CACHE INTERNAL "Lua 5.4 version")
        endif()
    endif()
endif()

mark_as_advanced(LUA54_INCLUDE_DIR LUA54_LIBRARY)
