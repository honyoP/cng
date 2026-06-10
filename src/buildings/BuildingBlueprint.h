#pragma once

#include "world/World.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct BlueprintTerrain {
    Int2 position{};
    TerrainType terrain = TerrainType::grass;
};

struct BlueprintEdge {
    Int2 position{};
    Direction direction = Direction::north;
    EdgeObject edge{};
};

struct BlueprintFurniture {
    Int2 position{};
    std::string id;
};

struct BlueprintLootZone {
    std::string id;
    Int2 min{};
    Int2 max{};
};

struct BuildingBlueprint {
    std::string id = "new_building";
    std::string name = "New Building";
    int width = 5;
    int height = 5;
    std::vector<BlueprintTerrain> terrain;
    std::vector<BlueprintEdge> edges;
    std::vector<BlueprintFurniture> furniture;
    std::vector<BlueprintLootZone> loot_zones;
};

using BuildingBlueprintLibrary = std::unordered_map<std::string, BuildingBlueprint>;

struct BlueprintFileResult {
    std::optional<BuildingBlueprint> blueprint;
    std::vector<std::string> errors;

    [[nodiscard]] bool ok() const { return blueprint.has_value() && errors.empty(); }
};

[[nodiscard]] BuildingBlueprint make_empty_building_blueprint(
    std::string id,
    std::string name,
    int width,
    int height);
[[nodiscard]] BlueprintFileResult load_building_blueprint(const std::filesystem::path& path);
[[nodiscard]] bool save_building_blueprint(
    const BuildingBlueprint& blueprint,
    const std::filesystem::path& path,
    std::vector<std::string>& errors);
[[nodiscard]] bool place_building_blueprint(World& world, const BuildingBlueprint& blueprint, Int2 origin);
[[nodiscard]] bool place_building_blueprint(
    World& world,
    const BuildingBlueprintLibrary& blueprints,
    std::string_view blueprint_id,
    int origin_x,
    int origin_y);
[[nodiscard]] const char* direction_name(Direction direction);

