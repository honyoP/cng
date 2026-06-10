#include "app/InputState.h"
#include "buildings/BuildingBlueprint.h"
#include "data/GameData.h"
#include "entities/Entity.h"
#include "renderer/Camera.h"
#include "renderer/ChunkMesh.h"
#include "systems/Movement.h"
#include "world/World.h"
#include "world/WorldGen.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <type_traits>

namespace {
constexpr std::uint64_t test_seed = 123456789ULL;
static_assert(std::is_same_v<decltype(Tile{}.elevation), int>);

void assert_same_tile(const Tile& left, const Tile& right) {
    assert(left.terrain == right.terrain);
    assert(left.elevation == right.elevation);
    assert(left.walkable == right.walkable);
    assert(left.movement_cost == right.movement_cost);
    assert(left.blocking == right.blocking);
}

void test_negative_coordinate_conversion() {
    assert(world_to_chunk({0, 0}) == Int2(0, 0));
    assert(world_to_chunk({31, 31}) == Int2(0, 0));
    assert(world_to_chunk({32, 32}) == Int2(1, 1));
    assert(world_to_chunk({-1, -1}) == Int2(-1, -1));
    assert(world_to_chunk({-32, -32}) == Int2(-1, -1));
    assert(world_to_chunk({-33, -33}) == Int2(-2, -2));
    assert(world_to_local({-1, -1}) == Int2(31, 31));
    assert(world_to_local({-32, -32}) == Int2(0, 0));
}

void test_determinism_and_chunk_borders() {
    World world(test_seed, 1);
    world.update_loaded_chunks({31, 0});

    const Tile left = *world.get_tile({31, 0});
    const Tile right = *world.get_tile({32, 0});
    assert_same_tile(left, worldgen::generate_tile(test_seed, {31, 0}));
    assert_same_tile(right, worldgen::generate_tile(test_seed, {32, 0}));

    // Neighboring border tiles come from the same continuous noise field, not separate chunk-local noise.
    assert(std::abs(left.elevation - right.elevation) <= 1);
}

void test_unload_and_return_is_stable() {
    World world(test_seed, 1);
    world.update_loaded_chunks({0, 0});
    assert(world.loaded_chunk_count() == 9);
    const Tile original = *world.get_tile({0, 0});

    world.update_loaded_chunks({chunk_size * 20, -chunk_size * 20});
    assert(world.loaded_chunk_count() == 9);
    assert(world.get_tile({0, 0}) == nullptr);

    world.update_loaded_chunks({0, 0});
    assert_same_tile(original, *world.get_tile({0, 0}));
}

void test_loading_radius_can_change_at_runtime() {
    World world(test_seed, 1);
    world.update_loaded_chunks({0, 0});
    assert(world.loaded_chunk_count() == 9);

    world.set_loading_radius(2);
    world.update_loaded_chunks({0, 0});
    assert(world.loading_radius() == 2);
    assert(world.loaded_chunk_count() == 25);

    world.set_loading_radius(-5);
    world.update_loaded_chunks({0, 0});
    assert(world.loading_radius() == 0);
    assert(world.loaded_chunk_count() == 1);
}

void test_camera_zoom_clamps_and_resets() {
    CameraView camera;
    zoom_camera(camera, 1.0F);
    assert(camera.half_view_height == 18.0F);

    zoom_camera(camera, 100.0F);
    assert(camera.half_view_height == MIN_CAMERA_HALF_VIEW_HEIGHT);

    zoom_camera(camera, -100.0F);
    assert(camera.half_view_height == MAX_CAMERA_HALF_VIEW_HEIGHT);

    reset_camera_zoom(camera);
    assert(camera.half_view_height == 20.0F);
    rotate_camera(camera, 20.0F);
    assert(camera.yaw_degrees == -40.0F);
}

void test_shared_edges_block_movement() {
    World world(test_seed, 1);
    world.update_loaded_chunks({0, 0});
    Tile left = *world.get_tile({0, 0});
    Tile right = *world.get_tile({1, 0});
    left.elevation = 2;
    right.elevation = 2;
    left.walkable = right.walkable = true;
    left.blocking = right.blocking = false;
    world.set_tile({0, 0}, left);
    world.set_tile({1, 0}, right);

    world.set_edge({0, 0}, Direction::east, {EdgeType::wall, false});
    assert(world.get_edge({1, 0}, Direction::west).type == EdgeType::wall);
    assert(!can_move_between_tiles(world, {0, 0}, {1, 0}));
    assert(!can_move_between_tiles(world, {1, 0}, {0, 0}));

    world.set_edge({0, 0}, Direction::east, {EdgeType::door, true});
    assert(can_move_between_tiles(world, {0, 0}, {1, 0}));
    assert(can_move_between_tiles(world, {1, 0}, {0, 0}));

    world.set_edge({0, 0}, Direction::east, {EdgeType::window, false});
    assert(!can_move_between_tiles(world, {0, 0}, {1, 0}));
}

void test_restore_generated_terrain_preserves_edges() {
    World world(test_seed, 1);
    world.update_loaded_chunks({0, 0});
    const Tile generated = worldgen::generate_tile(test_seed, {0, 0});
    Tile edited = *world.get_tile({0, 0});
    edited.terrain = TerrainType::floor;
    edited.furniture_id = "chair";
    edited.loot_zones.push_back("loot");
    world.set_tile({0, 0}, edited);
    world.set_edge({0, 0}, Direction::north, {EdgeType::wall, false});

    world.restore_generated_terrain({0, 0});
    const Tile& restored = *world.get_tile({0, 0});
    assert(restored.terrain == generated.terrain);
    assert(restored.elevation == generated.elevation);
    assert(restored.furniture_id.empty());
    assert(restored.loot_zones.empty());
    assert(world.get_edge({0, 0}, Direction::north).type == EdgeType::wall);
}

void test_blueprint_round_trip_and_cross_chunk_placement() {
    BuildingBlueprint blueprint = make_empty_building_blueprint("test_building", "Test Building", 3, 2);
    blueprint.terrain.push_back({{0, 0}, TerrainType::floor});
    blueprint.terrain.push_back({{1, 0}, TerrainType::sand});
    blueprint.edges.push_back({{0, 0}, Direction::west, {EdgeType::wall, false}});
    blueprint.edges.push_back({{1, 0}, Direction::east, {EdgeType::door, true}});
    blueprint.furniture.push_back({{1, 1}, "chair_placeholder"});
    blueprint.loot_zones.push_back({"test_loot", {0, 0}, {1, 1}});

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "cng-blueprint-test";
    std::filesystem::remove_all(root);
    std::vector<std::string> errors;
    const auto path = root / "blueprints" / "buildings" / "test_building.json";
    assert(save_building_blueprint(blueprint, path, errors));
    assert(errors.empty());
    const BlueprintFileResult loaded = load_building_blueprint(path);
    assert(loaded.ok());
    assert(loaded.blueprint->edges.size() == 2);
    assert(loaded.blueprint->furniture.front().id == "chair_placeholder");

    std::filesystem::create_directories(root / "items");
    {
        std::ofstream output(root / "items" / "empty.json");
        output << "[]\n";
    }
    const DataLoadResult reloaded_data = load_game_data(root);
    assert(reloaded_data.ok());
    assert(reloaded_data.data.building_blueprints.contains("test_building"));

    World world(test_seed, 0);
    world.update_loaded_chunks({0, 0});
    assert(place_building_blueprint(
        world,
        reloaded_data.data.building_blueprints,
        "test_building",
        31,
        -33));
    assert(world.get_tile({32, -32})->furniture_id == "chair_placeholder");
    assert(world.get_tile({31, -33})->terrain == TerrainType::floor);
    assert(world.get_edge({31, -33}, Direction::west).type == EdgeType::wall);
    assert(world.get_edge({32, -33}, Direction::east).door_open);
    assert(std::ranges::find(world.get_tile({31, -33})->loot_zones, "test_loot")
        != world.get_tile({31, -33})->loot_zones.end());

    world.update_loaded_chunks({1000, 1000});
    assert(world.get_tile({31, -33}) == nullptr);
    world.update_loaded_chunks({31, -33});
    assert(world.get_edge({31, -33}, Direction::west).type == EdgeType::wall);
    assert(world.get_tile({32, -32}) == nullptr);
    world.update_loaded_chunks({32, -32});
    assert(world.get_tile({32, -32})->furniture_id == "chair_placeholder");
    std::filesystem::remove_all(root);
}

void test_spawn_and_cross_chunk_movement() {
    World world(test_seed, 1);
    const Int2 spawn = world.find_walkable_spawn();
    assert(world.is_walkable(spawn));

    Int2 crossing_start{};
    bool found_crossing = false;
    for (int y = -256; y <= 256 && !found_crossing; ++y) {
        world.update_loaded_chunks({31, y});
        if (can_move_between_tiles(world, {31, y}, {32, y})) {
            crossing_start = {31, y};
            found_crossing = true;
        }
    }
    assert(found_crossing);

    EntityStore entities;
    Entity& player = entities.spawn(EntityType::player, crossing_start, "player");
    PlayerMovementState movement_state;
    movement::update_player(player, movement_state, InputState{.move_right = true}, world, 1.0 / 60.0);
    assert(player.tile_position == Int2(32, crossing_start.y));
    assert(world_to_chunk(player.tile_position).x == 1);
}

void test_surface_height_and_step_rules() {
    World world(test_seed, 1);
    world.update_loaded_chunks({0, 0});

    const Tile& origin = *world.get_tile({0, 0});
    assert(std::abs(get_surface_world_y(world, 0, 0)
        - static_cast<float>(origin.elevation) * TILE_HEIGHT) < 0.000001F);
    const Vec3 world_position = tile_to_world_position(world, {0, 0});
    assert(world_position.x == 0.0F);
    assert(std::abs(world_position.y - get_surface_world_y(world, 0, 0)) < 0.000001F);
    assert(world_position.z == 0.0F);

    const Vec3 explicit_position = tile_to_world_position(2, -3, 4);
    assert(explicit_position.x == 2.0F * TILE_SIZE);
    assert(explicit_position.y == 4.0F * TILE_HEIGHT);
    assert(explicit_position.z == -3.0F * TILE_SIZE);

    bool found_walkable_pair_with_height_difference = false;
    for (int y = -64; y <= 64 && !found_walkable_pair_with_height_difference; ++y) {
        for (int x = -64; x <= 64; ++x) {
            world.update_loaded_chunks({x, y});
            const Tile* from = world.get_tile({x, y});
            const Tile* to = world.get_tile({x + 1, y});
            if (!from || !to || !from->walkable || from->blocking || !to->walkable || to->blocking) {
                continue;
            }
            const int difference = std::abs(to->elevation - from->elevation);
            if (difference > 0) {
                assert(!can_move_between_tiles(world, {x, y}, {x + 1, y}, difference - 1));
                assert(can_move_between_tiles(world, {x, y}, {x + 1, y}, difference));
                found_walkable_pair_with_height_difference = true;
            }
        }
    }
    assert(found_walkable_pair_with_height_difference);
}

void test_entity_visual_height_interpolates() {
    World world(test_seed, 1);
    world.update_loaded_chunks({0, 0});

    Int2 target{};
    bool found_nonzero_surface = false;
    for (int y = -32; y <= 32 && !found_nonzero_surface; ++y) {
        for (int x = -32; x <= 32; ++x) {
            world.update_loaded_chunks({x, y});
            if (get_surface_world_y(world, x, y) > 0.1F) {
                target = {x, y};
                found_nonzero_surface = true;
                break;
            }
        }
    }
    assert(found_nonzero_surface);

    EntityStore entities;
    Entity& entity = entities.spawn(EntityType::player, target, "player");
    entity.visual_position.y = 0.0F;
    const float target_height = get_surface_world_y(world, target.x, target.y);
    movement::update_visual_positions(entities, world, 0.01F);
    assert(entity.visual_position.y > 0.0F);
    assert(entity.visual_position.y < target_height);
    movement::update_visual_positions(entities, world, 10.0F);
    assert(std::abs(entity.visual_position.y - target_height) < 0.000001F);
}

void test_seed_changes_terrain() {
    bool found_difference = false;
    for (int y = -10; y <= 10 && !found_difference; ++y) {
        for (int x = -10; x <= 10; ++x) {
            const Tile first = worldgen::generate_tile(test_seed, {x, y});
            const Tile second = worldgen::generate_tile(test_seed + 1, {x, y});
            if (first.terrain != second.terrain || first.elevation != second.elevation) {
                found_difference = true;
                break;
            }
        }
    }
    assert(found_difference);
}

void test_all_v0_1_terrain_types_are_reachable() {
    std::array<bool, 7> found{};
    for (int y = -2048; y <= 2048; y += 8) {
        for (int x = -2048; x <= 2048; x += 8) {
            const Tile tile = worldgen::generate_tile(test_seed, {x, y});
            assert(tile.elevation >= MIN_TERRAIN_ELEVATION);
            assert(tile.elevation <= MAX_TERRAIN_ELEVATION);
            found[static_cast<std::size_t>(tile.terrain)] = true;
        }
    }
    for (bool terrain_was_found : found) {
        assert(terrain_was_found);
    }
}

void test_chunk_mesh_bakes_visible_faces_and_tracks_borders() {
    World world(test_seed, 1);
    world.update_loaded_chunks({0, 0});

    for (int y = -chunk_size; y < chunk_size * 2; ++y) {
        for (int x = -chunk_size; x < chunk_size * 2; ++x) {
            Tile* tile = world.get_tile({x, y});
            assert(tile != nullptr);
            tile->elevation = 2;
            tile->terrain = TerrainType::grass;
        }
    }

    const ChunkMeshData flat = chunk_mesh::build(world, {0, 0});
    const std::size_t top_only_vertices = static_cast<std::size_t>(chunk_size * chunk_size * 6);
    assert(flat.vertices.size() == top_only_vertices);

    for (int y = 0; y < chunk_size; ++y) {
        world.get_tile({chunk_size, y})->elevation = 1;
    }
    const std::uint64_t changed_signature = chunk_mesh::source_signature(world, {0, 0});
    assert(changed_signature != flat.source_signature);

    const ChunkMeshData exposed_border = chunk_mesh::build(world, {0, 0});
    assert(exposed_border.vertices.size() == top_only_vertices + static_cast<std::size_t>(chunk_size * 6));
    assert(exposed_border.vertices.size() % 3 == 0);
}

void test_json_game_data_loader() {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "cng-json-data-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "items" / "weapons");

    {
        std::ofstream output(root / "items" / "weapons" / "valid.json");
        output << R"([
          {
            "id": "test_pipe",
            "name": "test pipe",
            "sprite": "items/test_pipe",
            "weight_kg": 1.5,
            "volume_l": 1.0,
            "components": {
              "weapon": {
                "damage_bash": 10,
                "damage_cut": 0,
                "attack_time": 100,
                "stamina_cost": 4
              }
            }
          }
        ])";
    }

    DataLoadResult valid = load_game_data(root);
    assert(valid.ok());
    assert(valid.files_scanned == 1);
    assert(valid.data.items.size() == 1);
    const ItemDef* pipe = find_item(valid.data, "test_pipe");
    assert(pipe != nullptr);
    assert(pipe->weapon.has_value());
    assert(pipe->weapon->damage_bash == 10);

    {
        std::ofstream output(root / "items" / "duplicate_and_invalid.json");
        output << R"([
          {
            "id": "test_pipe",
            "name": "duplicate",
            "sprite": "items/duplicate",
            "weight_kg": 1.0,
            "volume_l": 1.0
          },
          {
            "id": "bad_item",
            "name": "bad item",
            "sprite": "items/bad",
            "weight_kg": -1.0,
            "volume_l": 1.0
          }
        ])";
    }

    DataLoadResult invalid = load_game_data(root);
    assert(!invalid.ok());
    assert(invalid.files_scanned == 2);
    assert(invalid.data.items.size() == 1);
    assert(invalid.errors.size() == 2);
    std::filesystem::remove_all(root);
}
} // namespace

int main() {
    test_negative_coordinate_conversion();
    test_determinism_and_chunk_borders();
    test_unload_and_return_is_stable();
    test_loading_radius_can_change_at_runtime();
    test_camera_zoom_clamps_and_resets();
    test_shared_edges_block_movement();
    test_restore_generated_terrain_preserves_edges();
    test_blueprint_round_trip_and_cross_chunk_placement();
    test_spawn_and_cross_chunk_movement();
    test_surface_height_and_step_rules();
    test_entity_visual_height_interpolates();
    test_seed_changes_terrain();
    test_all_v0_1_terrain_types_are_reachable();
    test_chunk_mesh_bakes_visible_faces_and_tracks_borders();
    test_json_game_data_loader();
    std::cout << "Worldgen tests passed.\n";
}
