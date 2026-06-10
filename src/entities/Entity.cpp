#include "entities/Entity.h"

#include "world/World.h"

#include <utility>

Entity& EntityStore::spawn(EntityType type, Int2 position, std::string name) {
    entities_.push_back(Entity{
        .id = next_id_++,
        .type = type,
        .tile_position = position,
        .visual_position = tile_to_world_position(position.x, position.y, 0),
        .name = std::move(name),
    });
    return entities_.back();
}

Entity* EntityStore::find(EntityId id) {
    for (Entity& entity : entities_) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

const Entity* EntityStore::find(EntityId id) const {
    for (const Entity& entity : entities_) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}
