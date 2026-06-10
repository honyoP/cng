#pragma once

#include "core/Types.h"

#include <cstdint>
#include <string>
#include <vector>

using EntityId = std::uint32_t;

enum class EntityType {
    player,
    zombie,
    item,
};

struct Entity {
    EntityId id = 0;
    EntityType type = EntityType::item;
    Int2 tile_position{};
    Vec3 visual_position{};
    std::string name;
    bool active = true;
};

class EntityStore {
public:
    // Store EntityId across spawns; vector growth can invalidate Entity references.
    Entity& spawn(EntityType type, Int2 position, std::string name);
    [[nodiscard]] Entity* find(EntityId id);
    [[nodiscard]] const Entity* find(EntityId id) const;
    [[nodiscard]] std::vector<Entity>& all() { return entities_; }
    [[nodiscard]] const std::vector<Entity>& all() const { return entities_; }

private:
    EntityId next_id_ = 1;
    std::vector<Entity> entities_;
};
