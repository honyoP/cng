#pragma once

#include "entities/Entity.h"
#include "editor/BuildingEditor.h"
#include "renderer/ChunkMesh.h"
#include "world/World.h"

#include <cstddef>
#include <unordered_map>

struct RenderOptions {
    int width = 1;
    int height = 1;
    float frame_dt = 0.0F;
    float camera_half_view_height = 20.0F;
    float camera_pitch_degrees = 35.0F;
    float camera_yaw_degrees = -45.0F;
    Vec3 camera_target{};
    float render_margin_chunks = 0.5F;
    bool show_chunk_borders = false;
    bool show_axis_marker = true;
    const BuildingEditorState* building_editor = nullptr;
    const BuildingBlueprint* ghost_blueprint = nullptr;
};

struct RendererStats {
    double fps = 0.0;
    std::size_t loaded_chunks = 0;
    std::size_t visible_chunks = 0;
    std::size_t terrain_draw_calls = 0;
    std::size_t terrain_triangles = 0;
    std::size_t generated_mesh_vertices = 0;
};

class Renderer {
public:
    void render(const World& world, const EntityStore& entities, const Entity& player, RenderOptions options);
    [[nodiscard]] const RendererStats& stats() const { return stats_; }

private:
    void sync_meshes(const World& world);

    std::unordered_map<Int2, ChunkMeshData, Int2Hash> chunk_meshes_;
    RendererStats stats_;
    double fps_timer_ = 0.0;
    int fps_frames_ = 0;
};
