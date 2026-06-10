#pragma once

#include "app/InputState.h"
#include "entities/Entity.h"
#include "world/World.h"

struct PlayerMovementState {
    double repeat_timer = 0.0;
    bool blocked_by_height = false;
    Int2 blocked_target{};
    int blocked_from_elevation = 0;
    int blocked_target_elevation = 0;
};

namespace movement {

void update_player(
    Entity& player,
    PlayerMovementState& movement_state,
    const InputState& input,
    const World& world,
    double fixed_dt);

void update_visual_positions(EntityStore& entities, const World& world, float frame_dt);

} // namespace movement
