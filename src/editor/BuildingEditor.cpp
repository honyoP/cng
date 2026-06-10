#include "editor/BuildingEditor.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <ranges>
#include <vector>

namespace {
constexpr float degrees_to_radians = 3.1415926535F / 180.0F;

Vec3 inverse_rotate_view(Vec3 value, const CameraView& camera) {
    const float pitch = camera.pitch_degrees * degrees_to_radians;
    const float pitch_sin = std::sin(pitch);
    const float pitch_cos = std::cos(pitch);
    const Vec3 after_pitch{
        value.x,
        pitch_cos * value.y + pitch_sin * value.z,
        -pitch_sin * value.y + pitch_cos * value.z,
    };

    const float yaw = -camera.yaw_degrees * degrees_to_radians;
    const float yaw_sin = std::sin(yaw);
    const float yaw_cos = std::cos(yaw);
    return {
        yaw_cos * after_pitch.x + yaw_sin * after_pitch.z,
        after_pitch.y,
        -yaw_sin * after_pitch.x + yaw_cos * after_pitch.z,
    };
}

void apply_floor(World& world, Int2 position) {
    world.ensure_tile_loaded(position);
    Tile tile = *world.get_tile(position);
    tile.terrain = TerrainType::floor;
    tile.walkable = true;
    tile.blocking = false;
    tile.movement_cost = 1.0F;
    world.set_tile(position, std::move(tile));
}

void erase_tile(World& world, Int2 position) {
    world.restore_generated_terrain(position);
}

void apply_rectangle(Int2 a, Int2 b, const auto& function) {
    for (int y = std::min(a.y, b.y); y <= std::max(a.y, b.y); ++y) {
        for (int x = std::min(a.x, b.x); x <= std::max(a.x, b.x); ++x) {
            function(Int2{x, y});
        }
    }
}

const BuildingBlueprint* selected_blueprint(const BuildingEditorState& editor, const DataLoadResult& game_data) {
    std::vector<const BuildingBlueprint*> blueprints;
    for (const auto& [id, blueprint] : game_data.data.building_blueprints) {
        (void)id;
        blueprints.push_back(&blueprint);
    }
    std::ranges::sort(blueprints, {}, &BuildingBlueprint::id);
    if (blueprints.empty()) {
        return nullptr;
    }
    return blueprints[static_cast<std::size_t>(
        std::clamp(editor.selected_blueprint, 0, static_cast<int>(blueprints.size()) - 1))];
}

void apply_click_tool(BuildingEditorState& editor, World& world, const DataLoadResult& game_data) {
    const Int2 tile = editor.hovered.tile;
    switch (editor.tool) {
    case BuildingTool::paint_floor:
        apply_floor(world, tile);
        break;
    case BuildingTool::place_wall:
        world.set_edge(tile, editor.hovered.edge, {EdgeType::wall, false});
        break;
    case BuildingTool::place_door:
        world.set_edge(tile, editor.hovered.edge, {EdgeType::door, editor.door_open});
        break;
    case BuildingTool::place_window:
        world.set_edge(tile, editor.hovered.edge, {EdgeType::window, false});
        break;
    case BuildingTool::place_furniture: {
        world.ensure_tile_loaded(tile);
        Tile value = *world.get_tile(tile);
        value.furniture_id = editor.furniture_id.data();
        world.set_tile(tile, std::move(value));
        break;
    }
    case BuildingTool::erase:
        world.set_edge(tile, editor.hovered.edge, {});
        erase_tile(world, tile);
        break;
    case BuildingTool::place_blueprint:
        if (const BuildingBlueprint* blueprint = selected_blueprint(editor, game_data)) {
            editor.status = place_building_blueprint(world, *blueprint, tile)
                ? "Placed " + blueprint->id
                : "Blueprint placement failed";
        } else {
            editor.status = "No loaded blueprint selected";
        }
        break;
    case BuildingTool::define_loot_zone:
    case BuildingTool::define_blueprint_bounds:
        break;
    }
}
} // namespace

const char* building_tool_name(BuildingTool tool) {
    switch (tool) {
    case BuildingTool::paint_floor: return "Paint Floor [1]";
    case BuildingTool::place_wall: return "Place Wall [2]";
    case BuildingTool::place_door: return "Place Door [3]";
    case BuildingTool::place_window: return "Place Window [4]";
    case BuildingTool::place_furniture: return "Place Furniture [5]";
    case BuildingTool::erase: return "Erase [6]";
    case BuildingTool::define_loot_zone: return "Define Loot Zone [7]";
    case BuildingTool::define_blueprint_bounds: return "Define Blueprint Bounds [8]";
    case BuildingTool::place_blueprint: return "Place Blueprint [9]";
    }
    return "Unknown";
}

HoveredWorld pick_world_tile(
    const World& world,
    const CameraView& camera,
    Vec3 camera_target,
    int viewport_width,
    int viewport_height,
    float mouse_x,
    float mouse_y) {
    if (viewport_width <= 0 || viewport_height <= 0) {
        return {};
    }
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);
    const float view_x = (mouse_x / static_cast<float>(viewport_width) * 2.0F - 1.0F)
        * camera.half_view_height * aspect;
    const float view_y = (1.0F - mouse_y / static_cast<float>(viewport_height) * 2.0F)
        * camera.half_view_height;
    const Vec3 origin_offset = inverse_rotate_view({view_x, view_y, 100.0F}, camera);
    const Vec3 direction = inverse_rotate_view({0.0F, 0.0F, -1.0F}, camera);
    const Vec3 origin{
        camera_target.x + origin_offset.x,
        camera_target.y + origin_offset.y,
        camera_target.z + origin_offset.z,
    };

    float best_distance = 100000.0F;
    Vec3 hit{};
    bool found = false;
    for (int elevation = MIN_TERRAIN_ELEVATION; elevation <= MAX_TERRAIN_ELEVATION; ++elevation) {
        const float surface_y = static_cast<float>(elevation) * TILE_HEIGHT;
        const float distance = (surface_y - origin.y) / direction.y;
        if (distance < 0.0F || distance >= best_distance) {
            continue;
        }
        const Vec3 point{
            origin.x + direction.x * distance,
            surface_y,
            origin.z + direction.z * distance,
        };
        const Int2 tile{
            static_cast<int>(std::floor(point.x / TILE_SIZE + 0.5F)),
            static_cast<int>(std::floor(point.z / TILE_SIZE + 0.5F)),
        };
        const Tile* value = world.get_tile(tile);
        if (value && value->elevation == elevation) {
            best_distance = distance;
            hit = point;
            found = true;
        }
    }
    if (!found) {
        return {};
    }

    HoveredWorld hovered;
    hovered.tile = {
        static_cast<int>(std::floor(hit.x / TILE_SIZE + 0.5F)),
        static_cast<int>(std::floor(hit.z / TILE_SIZE + 0.5F)),
    };
    const float local_x = hit.x / TILE_SIZE - static_cast<float>(hovered.tile.x);
    const float local_z = hit.z / TILE_SIZE - static_cast<float>(hovered.tile.y);
    const float north = std::abs(local_z + 0.5F);
    const float south = std::abs(0.5F - local_z);
    const float west = std::abs(local_x + 0.5F);
    const float east = std::abs(0.5F - local_x);
    const float nearest = std::min({north, east, south, west});
    hovered.edge = nearest == north ? Direction::north
        : nearest == east ? Direction::east
        : nearest == south ? Direction::south
        : Direction::west;
    hovered.valid = true;
    return hovered;
}

void update_building_editor(
    BuildingEditorState& editor,
    const BuildingEditorInput& input,
    World& world,
    const DataLoadResult& game_data) {
    if (!editor.enabled || !editor.hovered.valid) {
        return;
    }
    if (input.rotate_pressed) {
        editor.door_open = !editor.door_open;
    }
    if (input.right_pressed) {
        if (editor.drag_start) {
            editor.drag_start.reset();
            editor.status = "Action cancelled";
        } else {
            world.set_edge(editor.hovered.tile, editor.hovered.edge, {});
            erase_tile(world, editor.hovered.tile);
        }
        return;
    }
    if (input.left_pressed) {
        if (editor.tool == BuildingTool::define_loot_zone || editor.tool == BuildingTool::define_blueprint_bounds) {
            editor.drag_start = editor.hovered.tile;
        } else {
            apply_click_tool(editor, world, game_data);
        }
    }
    if (input.left_down && editor.tool == BuildingTool::paint_floor) {
        apply_floor(world, editor.hovered.tile);
    }
    if (input.left_released && editor.drag_start) {
        const Int2 start = *editor.drag_start;
        if (editor.tool == BuildingTool::define_blueprint_bounds) {
            editor.blueprint_min = {std::min(start.x, editor.hovered.tile.x), std::min(start.y, editor.hovered.tile.y)};
            editor.blueprint_max = {std::max(start.x, editor.hovered.tile.x), std::max(start.y, editor.hovered.tile.y)};
            editor.status = "Blueprint bounds selected";
        } else if (editor.tool == BuildingTool::define_loot_zone) {
            BlueprintLootZone zone{
                editor.loot_zone_id.data(),
                {std::min(start.x, editor.hovered.tile.x), std::min(start.y, editor.hovered.tile.y)},
                {std::max(start.x, editor.hovered.tile.x), std::max(start.y, editor.hovered.tile.y)},
            };
            editor.authored_loot_zones.push_back(zone);
            apply_rectangle(zone.min, zone.max, [&](Int2 position) {
                world.ensure_tile_loaded(position);
                Tile tile = *world.get_tile(position);
                if (std::ranges::find(tile.loot_zones, zone.id) == tile.loot_zones.end()) {
                    tile.loot_zones.push_back(zone.id);
                    world.set_tile(position, std::move(tile));
                }
            });
            editor.status = "Loot zone defined";
        }
        editor.drag_start.reset();
    }
}

BuildingBlueprint capture_building_blueprint(const BuildingEditorState& editor, const World& world) {
    if (!editor.blueprint_min || !editor.blueprint_max) {
        return {};
    }
    const Int2 min = *editor.blueprint_min;
    const Int2 max = *editor.blueprint_max;
    BuildingBlueprint blueprint = make_empty_building_blueprint(
        editor.blueprint_id.data(),
        editor.blueprint_id.data(),
        max.x - min.x + 1,
        max.y - min.y + 1);
    for (int y = min.y; y <= max.y; ++y) {
        for (int x = min.x; x <= max.x; ++x) {
            const Int2 world_position{x, y};
            const Int2 local{x - min.x, y - min.y};
            const Tile* tile = world.get_tile(world_position);
            if (!tile) continue;
            blueprint.terrain.push_back({local, tile->terrain});
            if (!tile->furniture_id.empty()) blueprint.furniture.push_back({local, tile->furniture_id});
            for (Direction direction : {Direction::north, Direction::east, Direction::south, Direction::west}) {
                const EdgeObject edge = world.get_edge(world_position, direction);
                if (edge.type != EdgeType::empty) blueprint.edges.push_back({local, direction, edge});
            }
        }
    }
    for (const BlueprintLootZone& zone : editor.authored_loot_zones) {
        if (zone.min.x >= min.x && zone.min.y >= min.y && zone.max.x <= max.x && zone.max.y <= max.y) {
            blueprint.loot_zones.push_back({
                zone.id,
                {zone.min.x - min.x, zone.min.y - min.y},
                {zone.max.x - min.x, zone.max.y - min.y},
            });
        }
    }
    return blueprint;
}

void draw_building_editor_status(
    BuildingEditorState& editor,
    const World& world,
    DataLoadResult& game_data,
    const std::filesystem::path& data_root) {
    if (!ImGui::CollapsingHeader("World Building Authoring")) return;
    ImGui::Text("Mode: %s", editor.enabled ? "enabled [F8]" : "disabled [F8]");
    ImGui::Text("Tool: %s", building_tool_name(editor.tool));
    if (editor.hovered.valid) {
        ImGui::Text("Hovered: %d, %d / %s", editor.hovered.tile.x, editor.hovered.tile.y, direction_name(editor.hovered.edge));
    }
    ImGui::InputText("Blueprint ID", editor.blueprint_id.data(), editor.blueprint_id.size());
    ImGui::InputText("Furniture ID", editor.furniture_id.data(), editor.furniture_id.size());
    ImGui::InputText("Loot zone ID", editor.loot_zone_id.data(), editor.loot_zone_id.size());
    ImGui::Checkbox("Doors start open [R]", &editor.door_open);

    std::vector<const BuildingBlueprint*> blueprints;
    for (const auto& [id, blueprint] : game_data.data.building_blueprints) {
        (void)id;
        blueprints.push_back(&blueprint);
    }
    std::ranges::sort(blueprints, {}, &BuildingBlueprint::id);
    if (!blueprints.empty()) {
        editor.selected_blueprint = std::clamp(editor.selected_blueprint, 0, static_cast<int>(blueprints.size()) - 1);
        if (ImGui::BeginCombo("Placement blueprint", blueprints[static_cast<std::size_t>(editor.selected_blueprint)]->id.c_str())) {
            for (int index = 0; index < static_cast<int>(blueprints.size()); ++index) {
                if (ImGui::Selectable(blueprints[static_cast<std::size_t>(index)]->id.c_str(), index == editor.selected_blueprint)) {
                    editor.selected_blueprint = index;
                }
            }
            ImGui::EndCombo();
        }
    }
    if (ImGui::Button("Save selected bounds")) {
        if (!editor.blueprint_min || !editor.blueprint_max) {
            editor.status = "Select blueprint bounds in-world first";
        } else {
            const BuildingBlueprint blueprint = capture_building_blueprint(editor, world);
            std::vector<std::string> errors;
            const auto path = data_root / "blueprints" / "buildings" / (blueprint.id + ".json");
            if (save_building_blueprint(blueprint, path, errors)) {
                game_data = load_game_data(data_root);
                editor.status = "Saved " + path.string();
            } else {
                editor.status = errors.empty() ? "Save failed" : errors.front();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload blueprints")) game_data = load_game_data(data_root);
    ImGui::TextDisabled("World-space controls: 1-9 tools, left apply/drag, right erase/cancel.");
    if (!editor.status.empty()) ImGui::TextWrapped("%s", editor.status.c_str());
}
