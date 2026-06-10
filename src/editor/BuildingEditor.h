#pragma once

#include "data/GameData.h"
#include "renderer/Camera.h"
#include "world/World.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>

enum class BuildingTool {
    paint_floor,
    place_wall,
    place_door,
    place_window,
    place_furniture,
    erase,
    define_loot_zone,
    define_blueprint_bounds,
    place_blueprint,
};

struct HoveredWorld {
    Int2 tile{};
    Direction edge = Direction::north;
    bool valid = false;
};

struct BuildingEditorState {
    bool enabled = false;
    BuildingTool tool = BuildingTool::paint_floor;
    HoveredWorld hovered{};
    std::optional<Int2> drag_start;
    std::optional<Int2> blueprint_min;
    std::optional<Int2> blueprint_max;
    std::vector<BlueprintLootZone> authored_loot_zones;
    int selected_blueprint = 0;
    bool door_open = false;
    std::array<char, 64> blueprint_id{"new_building"};
    std::array<char, 64> furniture_id{"furniture_placeholder"};
    std::array<char, 64> loot_zone_id{"loot_zone"};
    std::string status;
};

struct BuildingEditorInput {
    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    bool left_pressed = false;
    bool left_down = false;
    bool left_released = false;
    bool right_pressed = false;
    bool rotate_pressed = false;
};

[[nodiscard]] const char* building_tool_name(BuildingTool tool);
[[nodiscard]] HoveredWorld pick_world_tile(
    const World& world,
    const CameraView& camera,
    Vec3 camera_target,
    int viewport_width,
    int viewport_height,
    float mouse_x,
    float mouse_y);
void update_building_editor(
    BuildingEditorState& editor,
    const BuildingEditorInput& input,
    World& world,
    const DataLoadResult& game_data);
[[nodiscard]] BuildingBlueprint capture_building_blueprint(
    const BuildingEditorState& editor,
    const World& world);
void draw_building_editor_status(
    BuildingEditorState& editor,
    const World& world,
    DataLoadResult& game_data,
    const std::filesystem::path& data_root);
