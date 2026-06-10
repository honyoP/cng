#pragma once

#include "core/Types.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int chunk_size = 32;
constexpr float TILE_SIZE = 1.0F;
constexpr float TILE_HEIGHT = 1.0F;
constexpr int MAX_STEP_HEIGHT = 1;
constexpr int MIN_TERRAIN_ELEVATION = -1;
constexpr int MAX_TERRAIN_ELEVATION = 8;

enum class TerrainType : std::uint8_t {
    deep_water,
    shallow_water,
    sand,
    grass,
    forest,
    rock,
    mountain,
    floor,
};

enum class EdgeType : std::uint8_t {
    empty,
    wall,
    door,
    window,
};

struct EdgeObject {
    EdgeType type = EdgeType::empty;
    bool door_open = false;

    friend bool operator==(const EdgeObject&, const EdgeObject&) = default;
};

enum class Direction : std::uint8_t {
    north,
    east,
    south,
    west,
};

struct Tile {
    TerrainType terrain = TerrainType::grass;
    int elevation = 0;
    bool walkable = true;
    float movement_cost = 1.0F;
    bool blocking = false;
    std::array<EdgeObject, 4> edges{};
    // TODO: Replace this single-cell placeholder with furniture definitions
    // and explicit multi-tile occupancy when furniture gameplay begins.
    std::string furniture_id;
    // TODO: Loot zones are metadata only until loot tables/spawning exist.
    std::vector<std::string> loot_zones;
};

struct Chunk {
    Int2 coordinate{};
    std::array<Tile, chunk_size * chunk_size> tiles{};
};

class World {
public:
    explicit World(std::uint64_t seed, int loading_radius = 2);

    [[nodiscard]] const Tile* get_tile(Int2 position) const;
    [[nodiscard]] Tile* get_tile(Int2 position);
    [[nodiscard]] bool is_walkable(Int2 position) const;
    void set_tile(Int2 position, Tile tile);
    void restore_generated_terrain(Int2 position);
    void set_edge(Int2 position, Direction direction, EdgeObject edge);
    [[nodiscard]] EdgeObject get_edge(Int2 position, Direction direction) const;
    void ensure_tile_loaded(Int2 position);

    void update_loaded_chunks(Int2 player_position);
    void regenerate(std::uint64_t seed, Int2 player_position);
    void set_loading_radius(int loading_radius);
    [[nodiscard]] Int2 find_walkable_spawn();

    [[nodiscard]] std::uint64_t seed() const { return seed_; }
    [[nodiscard]] int loading_radius() const { return loading_radius_; }
    [[nodiscard]] std::size_t loaded_chunk_count() const { return chunks_.size(); }
    [[nodiscard]] const std::unordered_map<Int2, Chunk, Int2Hash>& loaded_chunks() const { return chunks_; }

private:
    void load_chunk(Int2 chunk_coordinate);

    std::uint64_t seed_;
    int loading_radius_;
    std::unordered_map<Int2, Chunk, Int2Hash> chunks_;
    // Session-time edits are separate from deterministic base terrain. Future
    // save support can serialize this map without changing world generation.
    std::unordered_map<Int2, Tile, Int2Hash> tile_overrides_;
};

[[nodiscard]] Int2 world_to_chunk(Int2 world_position);
[[nodiscard]] Int2 world_to_local(Int2 world_position);
[[nodiscard]] Vec3 tile_to_world_position(int tile_x, int tile_y, int elevation);
[[nodiscard]] Vec3 tile_to_world_position(const World& world, Int2 tile_position);
[[nodiscard]] float get_surface_world_y(const World& world, int tile_x, int tile_y);
[[nodiscard]] bool can_move_between_tiles(
    const World& world,
    Int2 from,
    Int2 to,
    int max_step_height = MAX_STEP_HEIGHT);
[[nodiscard]] const char* terrain_name(TerrainType terrain);
[[nodiscard]] const char* edge_type_name(EdgeType edge);
[[nodiscard]] Int2 direction_offset(Direction direction);
[[nodiscard]] Direction opposite_direction(Direction direction);
