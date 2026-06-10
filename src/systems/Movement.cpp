#include "systems/Movement.h"

#include <algorithm>
#include <cmath>

void movement::update_player(
    Entity& player,
    PlayerMovementState& movement_state,
    const InputState& input,
    const World& world,
    double fixed_dt) {
    movement_state.repeat_timer = std::max(0.0, movement_state.repeat_timer - fixed_dt);

    const Int2 direction{
        static_cast<int>(input.move_right) - static_cast<int>(input.move_left),
        static_cast<int>(input.move_down) - static_cast<int>(input.move_up),
    };
    if (direction == Int2{}) {
        // Releasing movement makes the next key press respond immediately.
        movement_state.repeat_timer = 0.0;
        return;
    }
    if (movement_state.repeat_timer > 0.0) {
        return;
    }
    movement_state.blocked_by_height = false;

    // Resolve axes separately so diagonal input can slide along a blocked wall.
    Int2 next = player.tile_position;
    const auto try_move = [&](Int2 target) {
        if (can_move_between_tiles(world, next, target)) {
            next = target;
            return;
        }

        const Tile* current_tile = world.get_tile(next);
        const Tile* target_tile = world.get_tile(target);
        if (current_tile && target_tile && target_tile->walkable && !target_tile->blocking
            && std::abs(target_tile->elevation - current_tile->elevation) > MAX_STEP_HEIGHT) {
            movement_state.blocked_by_height = true;
            movement_state.blocked_target = target;
            movement_state.blocked_from_elevation = current_tile->elevation;
            movement_state.blocked_target_elevation = target_tile->elevation;
        }
    };

    if (direction.x != 0) {
        try_move({next.x + direction.x, next.y});
    }
    if (direction.y != 0) {
        try_move({next.x, next.y + direction.y});
    }

    player.tile_position = next;
    const Tile* destination = world.get_tile(next);
    const double movement_cost = destination ? static_cast<double>(destination->movement_cost) : 1.0;
    movement_state.repeat_timer = 0.14 * movement_cost;
}

void movement::update_visual_positions(EntityStore& entities, const World& world, float frame_dt) {
    constexpr float visual_speed = 9.0F;
    for (Entity& entity : entities.all()) {
        const Vec3 target = tile_to_world_position(world, entity.tile_position);
        entity.visual_position = move_towards(entity.visual_position, target, visual_speed * frame_dt);
    }
}
