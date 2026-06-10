#include "buildings/BuildingBlueprint.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

using json = nlohmann::json;

namespace {
constexpr int max_blueprint_dimension = 128;

bool in_bounds(const BuildingBlueprint& blueprint, Int2 position) {
    return position.x >= 0 && position.y >= 0
        && position.x < blueprint.width && position.y < blueprint.height;
}

std::optional<TerrainType> terrain_from_name(const std::string& name) {
    for (TerrainType terrain : {
             TerrainType::deep_water, TerrainType::shallow_water, TerrainType::sand,
             TerrainType::grass, TerrainType::forest, TerrainType::rock, TerrainType::mountain,
             TerrainType::floor}) {
        if (name == terrain_name(terrain)) {
            return terrain;
        }
    }
    return std::nullopt;
}

std::optional<Direction> direction_from_name(const std::string& name) {
    if (name == "north") return Direction::north;
    if (name == "east") return Direction::east;
    if (name == "south") return Direction::south;
    if (name == "west") return Direction::west;
    return std::nullopt;
}

std::optional<EdgeType> edge_from_name(const std::string& name) {
    if (name == "empty") return EdgeType::empty;
    if (name == "wall") return EdgeType::wall;
    if (name == "door") return EdgeType::door;
    if (name == "window") return EdgeType::window;
    return std::nullopt;
}

bool read_position(const json& value, Int2& position) {
    if (!value.is_object() || !value.contains("x") || !value.contains("y")
        || !value.at("x").is_number_integer() || !value.at("y").is_number_integer()) {
        return false;
    }
    position = {value.at("x").get<int>(), value.at("y").get<int>()};
    return true;
}

json position_json(Int2 position) {
    return {{"x", position.x}, {"y", position.y}};
}

void apply_terrain_rules(Tile& tile, TerrainType terrain) {
    tile.terrain = terrain;
    tile.walkable = terrain != TerrainType::deep_water && terrain != TerrainType::mountain;
    tile.blocking = !tile.walkable;
    switch (terrain) {
    case TerrainType::deep_water: tile.movement_cost = 0.0F; break;
    case TerrainType::shallow_water: tile.movement_cost = 2.5F; break;
    case TerrainType::sand: tile.movement_cost = 1.25F; break;
    case TerrainType::forest: tile.movement_cost = 1.8F; break;
    case TerrainType::rock: tile.movement_cost = 1.6F; break;
    case TerrainType::mountain: tile.movement_cost = 0.0F; break;
    case TerrainType::grass: tile.movement_cost = 1.0F; break;
    case TerrainType::floor: tile.movement_cost = 1.0F; break;
    }
}
} // namespace

BuildingBlueprint make_empty_building_blueprint(std::string id, std::string name, int width, int height) {
    BuildingBlueprint blueprint;
    blueprint.id = std::move(id);
    blueprint.name = std::move(name);
    blueprint.width = std::clamp(width, 1, max_blueprint_dimension);
    blueprint.height = std::clamp(height, 1, max_blueprint_dimension);
    return blueprint;
}

BlueprintFileResult load_building_blueprint(const std::filesystem::path& path) {
    BlueprintFileResult result;
    std::ifstream input(path);
    if (!input) {
        result.errors.push_back("failed to open file");
        return result;
    }

    try {
        const json root = json::parse(input);
        BuildingBlueprint blueprint;
        if (!root.is_object() || !root.contains("id") || !root.at("id").is_string()
            || !root.contains("name") || !root.at("name").is_string()
            || !root.contains("width") || !root.at("width").is_number_integer()
            || !root.contains("height") || !root.at("height").is_number_integer()) {
            result.errors.push_back("blueprint requires string id/name and integer width/height");
            return result;
        }
        blueprint.id = root.at("id").get<std::string>();
        blueprint.name = root.at("name").get<std::string>();
        blueprint.width = root.at("width").get<int>();
        blueprint.height = root.at("height").get<int>();
        if (blueprint.id.empty() || blueprint.width < 1 || blueprint.height < 1
            || blueprint.width > max_blueprint_dimension || blueprint.height > max_blueprint_dimension) {
            result.errors.push_back("invalid blueprint id or dimensions");
            return result;
        }

        for (const json& entry : root.value("terrain", json::array())) {
            Int2 position;
            if (!read_position(entry, position) || !entry.contains("type") || !entry.at("type").is_string()) {
                result.errors.push_back("invalid terrain entry");
                continue;
            }
            const auto terrain = terrain_from_name(entry.at("type").get<std::string>());
            if (!terrain || !in_bounds(blueprint, position)) {
                result.errors.push_back("terrain entry is out of bounds or has unknown type");
                continue;
            }
            blueprint.terrain.push_back({position, *terrain});
        }
        for (const json& entry : root.value("edges", json::array())) {
            Int2 position;
            if (!read_position(entry, position) || !entry.contains("direction") || !entry.contains("type")
                || !entry.at("direction").is_string() || !entry.at("type").is_string()) {
                result.errors.push_back("invalid edge entry");
                continue;
            }
            const auto direction = direction_from_name(entry.at("direction").get<std::string>());
            const auto type = edge_from_name(entry.at("type").get<std::string>());
            if (!direction || !type || !in_bounds(blueprint, position)) {
                result.errors.push_back("edge entry is out of bounds or has unknown type/direction");
                continue;
            }
            blueprint.edges.push_back({position, *direction, {*type, entry.value("open", false)}});
        }
        for (const json& entry : root.value("furniture", json::array())) {
            Int2 position;
            if (!read_position(entry, position) || !entry.contains("id") || !entry.at("id").is_string()
                || !in_bounds(blueprint, position)) {
                result.errors.push_back("invalid furniture entry");
                continue;
            }
            blueprint.furniture.push_back({position, entry.at("id").get<std::string>()});
        }
        for (const json& entry : root.value("loot_zones", json::array())) {
            BlueprintLootZone zone;
            if (!entry.is_object() || !entry.contains("id") || !entry.at("id").is_string()
                || !entry.contains("min") || !entry.contains("max")
                || !read_position(entry.at("min"), zone.min) || !read_position(entry.at("max"), zone.max)) {
                result.errors.push_back("invalid loot zone entry");
                continue;
            }
            zone.id = entry.at("id").get<std::string>();
            if (!in_bounds(blueprint, zone.min) || !in_bounds(blueprint, zone.max)) {
                result.errors.push_back("loot zone is out of bounds");
                continue;
            }
            blueprint.loot_zones.push_back(std::move(zone));
        }
        result.blueprint = std::move(blueprint);
    } catch (const json::exception& error) {
        result.errors.push_back("json error: " + std::string(error.what()));
    }
    return result;
}

bool save_building_blueprint(
    const BuildingBlueprint& blueprint,
    const std::filesystem::path& path,
    std::vector<std::string>& errors) {
    if (blueprint.id.empty() || blueprint.width < 1 || blueprint.height < 1) {
        errors.push_back("blueprint requires a non-empty id and positive dimensions");
        return false;
    }
    json root{
        {"id", blueprint.id}, {"name", blueprint.name},
        {"width", blueprint.width}, {"height", blueprint.height},
        {"terrain", json::array()}, {"edges", json::array()},
        {"furniture", json::array()}, {"loot_zones", json::array()},
    };
    for (const BlueprintTerrain& entry : blueprint.terrain) {
        root["terrain"].push_back({{"x", entry.position.x}, {"y", entry.position.y}, {"type", terrain_name(entry.terrain)}});
    }
    for (const BlueprintEdge& entry : blueprint.edges) {
        root["edges"].push_back({
            {"x", entry.position.x}, {"y", entry.position.y},
            {"direction", direction_name(entry.direction)}, {"type", edge_type_name(entry.edge.type)},
            {"open", entry.edge.door_open},
        });
    }
    for (const BlueprintFurniture& entry : blueprint.furniture) {
        root["furniture"].push_back({{"x", entry.position.x}, {"y", entry.position.y}, {"id", entry.id}});
    }
    for (const BlueprintLootZone& zone : blueprint.loot_zones) {
        root["loot_zones"].push_back({{"id", zone.id}, {"min", position_json(zone.min)}, {"max", position_json(zone.max)}});
    }

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) {
        errors.push_back("failed to open output file");
        return false;
    }
    output << root.dump(2) << '\n';
    return true;
}

bool place_building_blueprint(World& world, const BuildingBlueprint& blueprint, Int2 origin) {
    if (blueprint.id.empty() || blueprint.width < 1 || blueprint.height < 1) {
        return false;
    }
    for (const BlueprintTerrain& entry : blueprint.terrain) {
        const Int2 position{origin.x + entry.position.x, origin.y + entry.position.y};
        world.ensure_tile_loaded(position);
        Tile tile = *world.get_tile(position);
        apply_terrain_rules(tile, entry.terrain);
        world.set_tile(position, std::move(tile));
    }
    for (const BlueprintFurniture& entry : blueprint.furniture) {
        const Int2 position{origin.x + entry.position.x, origin.y + entry.position.y};
        world.ensure_tile_loaded(position);
        Tile tile = *world.get_tile(position);
        tile.furniture_id = entry.id;
        world.set_tile(position, std::move(tile));
    }
    for (const BlueprintLootZone& zone : blueprint.loot_zones) {
        const int min_x = std::min(zone.min.x, zone.max.x);
        const int max_x = std::max(zone.min.x, zone.max.x);
        const int min_y = std::min(zone.min.y, zone.max.y);
        const int max_y = std::max(zone.min.y, zone.max.y);
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const Int2 position{origin.x + x, origin.y + y};
                world.ensure_tile_loaded(position);
                Tile tile = *world.get_tile(position);
                if (std::ranges::find(tile.loot_zones, zone.id) == tile.loot_zones.end()) {
                    tile.loot_zones.push_back(zone.id);
                }
                world.set_tile(position, std::move(tile));
            }
        }
    }
    for (const BlueprintEdge& entry : blueprint.edges) {
        world.set_edge(
            {origin.x + entry.position.x, origin.y + entry.position.y},
            entry.direction,
            entry.edge);
    }
    return true;
}

bool place_building_blueprint(
    World& world,
    const BuildingBlueprintLibrary& blueprints,
    std::string_view blueprint_id,
    int origin_x,
    int origin_y) {
    const auto found = blueprints.find(std::string(blueprint_id));
    return found != blueprints.end() && place_building_blueprint(world, found->second, {origin_x, origin_y});
}

const char* direction_name(Direction direction) {
    switch (direction) {
    case Direction::north: return "north";
    case Direction::east: return "east";
    case Direction::south: return "south";
    case Direction::west: return "west";
    }
    return "north";
}
