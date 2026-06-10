#pragma once

#include "buildings/BuildingBlueprint.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct WeaponDef {
    int damage_bash = 0;
    int damage_cut = 0;
    int attack_time = 100;
    int stamina_cost = 0;
};

struct ItemDef {
    std::string id;
    std::string name;
    std::string sprite;
    float weight_kg = 0.0F;
    float volume_l = 0.0F;
    std::optional<WeaponDef> weapon;
};

struct GameData {
    std::unordered_map<std::string, ItemDef> items;
    BuildingBlueprintLibrary building_blueprints;
};

struct DataError {
    std::filesystem::path file;
    std::string message;
};

struct DataLoadResult {
    GameData data;
    std::vector<DataError> errors;
    std::size_t files_scanned = 0;

    [[nodiscard]] bool ok() const { return errors.empty(); }
};

[[nodiscard]] DataLoadResult load_game_data(const std::filesystem::path& data_root);
[[nodiscard]] const ItemDef* find_item(const GameData& data, const std::string& id);
