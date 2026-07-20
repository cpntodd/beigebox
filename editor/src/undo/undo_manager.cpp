// editor/src/undo/undo_manager.cpp
// ─────────────────────────────────────────────────────────────
// Undo/Redo implementation + concrete ECS mutation commands.
// ─────────────────────────────────────────────────────────────

#include "undo_manager.h"

#include <SDL2/SDL.h>

namespace beigebox {

// ── UndoManager ──────────────────────────────────────────────

void UndoManager::Execute(std::unique_ptr<UndoCommand> cmd)
{
    cmd->Execute();
    undoStack_.push_back(std::move(cmd));
    redoStack_.clear();

    // Enforce max depth
    if (static_cast<int>(undoStack_.size()) > kMaxUndo)
        undoStack_.erase(undoStack_.begin());
}

void UndoManager::Undo()
{
    if (undoStack_.empty()) return;
    auto cmd = std::move(undoStack_.back());
    undoStack_.pop_back();
    cmd->Undo();
    redoStack_.push_back(std::move(cmd));
}

void UndoManager::Redo()
{
    if (redoStack_.empty()) return;
    auto cmd = std::move(redoStack_.back());
    redoStack_.pop_back();
    cmd->Execute();
    undoStack_.push_back(std::move(cmd));
}

std::string UndoManager::UndoDescription() const
{
    if (undoStack_.empty()) return "";
    return undoStack_.back()->Description();
}

std::string UndoManager::RedoDescription() const
{
    if (redoStack_.empty()) return "";
    return redoStack_.back()->Description();
}

void UndoManager::Clear()
{
    undoStack_.clear();
    redoStack_.clear();
}

// ── Concrete Commands ────────────────────────────────────────

// SetComponentProperty: undoable property change on an entity's component.
class SetPropCmd : public UndoCommand
{
public:
    SetPropCmd(entt::registry& reg, entt::entity entity,
               const std::string& desc,
               std::function<void()> apply,
               std::function<void()> revert)
        : reg_(&reg), entity_(entity), desc_(desc),
          apply_(std::move(apply)), revert_(std::move(revert)) {}

    void Execute() override { apply_(); }
    void Undo()    override { revert_(); }
    std::string Description() const override { return desc_; }

private:
    entt::registry* reg_;
    entt::entity    entity_;
    std::string     desc_;
    std::function<void()> apply_;
    std::function<void()> revert_;
};

// CreateEntityCmd: undoable entity creation.
class CreateEntityCmd : public UndoCommand
{
public:
    CreateEntityCmd(entt::registry& reg, entt::entity entity)
        : reg_(&reg), entity_(entity) {}

    void Execute() override { /* already created */ }
    void Undo()    override { if (reg_->valid(entity_)) reg_->destroy(entity_); }
    std::string Description() const override {
        return "Create entity " + std::to_string(static_cast<uint32_t>(entt::to_integral(entity_)));
    }

private:
    entt::registry* reg_;
    entt::entity    entity_;
};

// DeleteEntityCmd: undoable entity deletion.
class DeleteEntityCmd : public UndoCommand
{
public:
    DeleteEntityCmd(entt::registry& reg, entt::entity entity)
        : reg_(&reg), entity_(entity)
    {
        // Snapshot components before deletion
        if (reg_->all_of<Transform>(entity_))
            snap_.tx = reg_->get<Transform>(entity_).x.Raw();
            snap_.ty = reg_->get<Transform>(entity_).y.Raw();
        if (reg_->all_of<Health>(entity_)) {
            snap_.hp = reg_->get<Health>(entity_).current.Raw();
            snap_.maxHp = reg_->get<Health>(entity_).max.Raw();
        }
        if (reg_->all_of<Player>(entity_))
            snap_.faction = reg_->get<Player>(entity_).factionId;
        snap_.hasTransform = reg_->all_of<Transform>(entity_);
        snap_.hasHealth    = reg_->all_of<Health>(entity_);
        snap_.hasPlayer    = reg_->all_of<Player>(entity_);
    }

    void Execute() override {
        if (reg_->valid(entity_)) reg_->destroy(entity_);
    }
    void Undo() override {
        auto e = reg_->create();
        // Note: entity_ is stale after destroy; we recreate with same data
        if (snap_.hasTransform)
            reg_->emplace<Transform>(e, FixedPoint(snap_.tx), FixedPoint(snap_.ty));
        if (snap_.hasHealth)
            reg_->emplace<Health>(e, FixedPoint(snap_.hp), FixedPoint(snap_.maxHp));
        if (snap_.hasPlayer)
            reg_->emplace<Player>(e, snap_.faction);
    }
    std::string Description() const override {
        return "Delete entity " + std::to_string(static_cast<uint32_t>(entt::to_integral(entity_)));
    }

private:
    entt::registry* reg_;
    entt::entity    entity_;
    struct Snap {
        int tx, ty, hp, maxHp, faction;
        bool hasTransform, hasHealth, hasPlayer;
    } snap_;
};

// ── Factory Helpers ──────────────────────────────────────────

// Create an undoable SetProperty command from current + new values.
inline std::unique_ptr<UndoCommand> MakeSetTransformCmd(
    entt::registry& reg, entt::entity entity,
    FixedPoint oldX, FixedPoint oldY,
    FixedPoint newX, FixedPoint newY)
{
    return std::make_unique<SetPropCmd>(reg, entity,
        "Move entity",
        [&reg, entity, newX, newY]() {
            auto& t = reg.get<Transform>(entity);
            t.x = newX; t.y = newY;
        },
        [&reg, entity, oldX, oldY]() {
            auto& t = reg.get<Transform>(entity);
            t.x = oldX; t.y = oldY;
        });
}

inline std::unique_ptr<UndoCommand> MakeCreateEntityCmd(
    entt::registry& reg, entt::entity entity)
{
    return std::make_unique<CreateEntityCmd>(reg, entity);
}

inline std::unique_ptr<UndoCommand> MakeDeleteEntityCmd(
    entt::registry& reg, entt::entity entity)
{
    return std::make_unique<DeleteEntityCmd>(reg, entity);
}

} // namespace beigebox
