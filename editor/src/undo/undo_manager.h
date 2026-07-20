// editor/src/undo/undo_manager.h
// ─────────────────────────────────────────────────────────────
// Undo/Redo System — Command Pattern
//
// Tracks editor actions and supports Ctrl+Z / Ctrl+Y.
// Commands snapshot ECS state before/after mutations.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>
#include "beigebox/ecs/components.h"
#include "beigebox/lua/lua_bridge.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace beigebox {

// ── Abstract Command ─────────────────────────────────────────
class UndoCommand
{
public:
    virtual ~UndoCommand() = default;
    virtual void Execute() = 0;
    virtual void Undo()    = 0;
    virtual std::string Description() const = 0;
};

// ── Undo Manager ─────────────────────────────────────────────
class UndoManager
{
public:
    UndoManager() = default;

    // Execute a command (adds to undo stack, clears redo stack).
    void Execute(std::unique_ptr<UndoCommand> cmd);

    // Undo the last command.
    void Undo();

    // Redo the last undone command.
    void Redo();

    // Check if undo/redo is available.
    bool CanUndo() const { return !undoStack_.empty(); }
    bool CanRedo() const { return !redoStack_.empty(); }

    // Get the description of the next undo/redo.
    std::string UndoDescription() const;
    std::string RedoDescription() const;

    // Clear all history.
    void Clear();

    // Maximum undo depth.
    static constexpr int kMaxUndo = 64;

private:
    std::vector<std::unique_ptr<UndoCommand>> undoStack_;
    std::vector<std::unique_ptr<UndoCommand>> redoStack_;
};

} // namespace beigebox
