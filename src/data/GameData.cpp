#include "data/GameData.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <utility>

using json = nlohmann::json;

namespace {
void add_error(DataLoadResult& result, const std::filesystem::path& file, std::string message) {
    result.errors.push_back({file, std::move(message)});
}

bool require_string(
    const json& object,
    const char* key,
    std::string& output,
    DataLoadResult& result,
    const std::filesystem::path& file) {
    if (!object.contains(key) || !object.at(key).is_string()) {
        add_error(result, file, "missing or invalid string '" + std::string(key) + "'");
        return false;
    }
    output = object.at(key).get<std::string>();
    return true;
}

bool require_float(
    const json& object,
    const char* key,
    float& output,
    DataLoadResult& result,
    const std::filesystem::path& file) {
    if (!object.contains(key) || !object.at(key).is_number()) {
        add_error(result, file, "missing or invalid number '" + std::string(key) + "'");
        return false;
    }
    output = object.at(key).get<float>();
    return true;
}

bool require_int(
    const json& object,
    const char* key,
    int& output,
    DataLoadResult& result,
    const std::filesystem::path& file) {
    if (!object.contains(key)) {
        add_error(result, file, "missing or invalid integer '" + std::string(key) + "'");
        return false;
    }

    const json& value = object.at(key);
    if (value.is_number_unsigned()) {
        const std::uint64_t number = value.get<std::uint64_t>();
        if (number <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            output = static_cast<int>(number);
            return true;
        }
    } else if (value.is_number_integer()) {
        const std::int64_t number = value.get<std::int64_t>();
        if (number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max()) {
            output = static_cast<int>(number);
            return true;
        }
    } else {
        add_error(result, file, "missing or invalid integer '" + std::string(key) + "'");
        return false;
    }

    add_error(result, file, "integer '" + std::string(key) + "' is out of range");
    return false;
}

std::optional<WeaponDef> parse_weapon(
    const json& value,
    DataLoadResult& result,
    const std::filesystem::path& file) {
    if (!value.is_object()) {
        add_error(result, file, "weapon component must be an object");
        return std::nullopt;
    }

    WeaponDef weapon;
    bool valid = true;
    valid &= require_int(value, "damage_bash", weapon.damage_bash, result, file);
    valid &= require_int(value, "damage_cut", weapon.damage_cut, result, file);
    valid &= require_int(value, "attack_time", weapon.attack_time, result, file);
    valid &= require_int(value, "stamina_cost", weapon.stamina_cost, result, file);
    if (!valid) {
        return std::nullopt;
    }
    if (weapon.attack_time <= 0) {
        add_error(result, file, "weapon.attack_time must be greater than 0");
        return std::nullopt;
    }
    if (weapon.stamina_cost < 0) {
        add_error(result, file, "weapon.stamina_cost cannot be negative");
        return std::nullopt;
    }
    return weapon;
}

std::optional<ItemDef> parse_item(
    const json& value,
    DataLoadResult& result,
    const std::filesystem::path& file) {
    if (!value.is_object()) {
        add_error(result, file, "item entry must be an object");
        return std::nullopt;
    }

    ItemDef item;
    bool valid = true;
    valid &= require_string(value, "id", item.id, result, file);
    valid &= require_string(value, "name", item.name, result, file);
    valid &= require_string(value, "sprite", item.sprite, result, file);
    valid &= require_float(value, "weight_kg", item.weight_kg, result, file);
    valid &= require_float(value, "volume_l", item.volume_l, result, file);
    if (!valid) {
        return std::nullopt;
    }
    if (item.id.empty()) {
        add_error(result, file, "item id cannot be empty");
        return std::nullopt;
    }
    if (item.weight_kg < 0.0F || item.volume_l < 0.0F) {
        add_error(result, file, "item '" + item.id + "' cannot have negative weight or volume");
        return std::nullopt;
    }

    if (value.contains("components")) {
        const json& components = value.at("components");
        if (!components.is_object()) {
            add_error(result, file, "item '" + item.id + "' components must be an object");
            return std::nullopt;
        }
        if (components.contains("weapon")) {
            item.weapon = parse_weapon(components.at("weapon"), result, file);
            if (!item.weapon) {
                add_error(result, file, "item '" + item.id + "' has invalid weapon component");
                return std::nullopt;
            }
        }
    }
    return item;
}

void load_item_file(const std::filesystem::path& file, DataLoadResult& result) {
    ++result.files_scanned;
    std::ifstream input(file);
    if (!input) {
        add_error(result, file, "failed to open file");
        return;
    }

    try {
        const json root = json::parse(input);
        if (!root.is_array()) {
            add_error(result, file, "item file root must be an array");
            return;
        }
        for (const json& value : root) {
            std::optional<ItemDef> item = parse_item(value, result, file);
            if (!item) {
                continue;
            }
            if (result.data.items.contains(item->id)) {
                add_error(result, file, "duplicate item id '" + item->id + "'");
                continue;
            }
            result.data.items.emplace(item->id, std::move(*item));
        }
    } catch (const json::exception& error) {
        add_error(result, file, "json error: " + std::string(error.what()));
    }
}

void load_blueprint_file(const std::filesystem::path& file, DataLoadResult& result) {
    ++result.files_scanned;
    BlueprintFileResult loaded = load_building_blueprint(file);
    for (const std::string& error : loaded.errors) {
        add_error(result, file, error);
    }
    if (!loaded.blueprint) {
        return;
    }
    if (result.data.building_blueprints.contains(loaded.blueprint->id)) {
        add_error(result, file, "duplicate building blueprint id '" + loaded.blueprint->id + "'");
        return;
    }
    result.data.building_blueprints.emplace(loaded.blueprint->id, std::move(*loaded.blueprint));
}

void load_json_directory(
    const std::filesystem::path& root,
    DataLoadResult& result,
    void (*load_file)(const std::filesystem::path&, DataLoadResult&),
    bool required) {
    if (!std::filesystem::is_directory(root)) {
        if (required) {
            add_error(result, root, "directory does not exist");
        }
        return;
    }
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    for (const auto& file : files) {
        load_file(file, result);
    }
}
} // namespace

DataLoadResult load_game_data(const std::filesystem::path& data_root) {
    DataLoadResult result;
    load_json_directory(data_root / "items", result, load_item_file, true);
    load_json_directory(data_root / "blueprints" / "buildings", result, load_blueprint_file, false);
    return result;
}

const ItemDef* find_item(const GameData& data, const std::string& id) {
    const auto found = data.items.find(id);
    return found == data.items.end() ? nullptr : &found->second;
}
