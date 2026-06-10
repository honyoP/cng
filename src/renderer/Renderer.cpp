#include "renderer/Renderer.h"

#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {
void color(float red, float green, float blue) {
    glColor3f(red, green, blue);
}

void draw_box(float x, float base_y, float z, float width, float depth, float height);

bool is_chunk_visible(
    Int2 coordinate,
    Vec3 camera_target,
    float aspect,
    float half_view_height,
    float render_margin_chunks) {
    const Vec3 first = tile_to_world_position(coordinate.x * chunk_size, coordinate.y * chunk_size, 0);
    const float half_chunk = static_cast<float>(chunk_size) * TILE_SIZE * 0.5F;
    const float center_x = first.x + half_chunk;
    const float center_z = first.z + half_chunk;
    const float dx = center_x - camera_target.x;
    const float dz = center_z - camera_target.z;
    constexpr float camera_pitch_degrees = 35.0F;
    constexpr float degrees_to_radians = 3.1415926535F / 180.0F;
    const float half_view_width = half_view_height * std::max(aspect, 1.0F);
    const float ground_depth = half_view_height / std::sin(camera_pitch_degrees * degrees_to_radians);
    const float camera_ground_diagonal = std::sqrt(
        half_view_width * half_view_width + ground_depth * ground_depth);
    const float margin = std::max(render_margin_chunks, 0.0F) * static_cast<float>(chunk_size) * TILE_SIZE;
    const float view_radius = camera_ground_diagonal + half_chunk + margin;
    return dx * dx + dz * dz <= view_radius * view_radius;
}

void draw_chunk_border(Int2 coordinate) {
    const Vec3 first_center = tile_to_world_position(coordinate.x * chunk_size, coordinate.y * chunk_size, 0);
    const float half_tile = TILE_SIZE * 0.5F;
    const float left = first_center.x - half_tile;
    const float front = first_center.z - half_tile;
    const float right = left + static_cast<float>(chunk_size) * TILE_SIZE;
    const float back = front + static_cast<float>(chunk_size) * TILE_SIZE;
    constexpr float marker_y = static_cast<float>(MAX_TERRAIN_ELEVATION + 2) * TILE_HEIGHT;

    color(1.0F, 0.15F, 0.85F);
    glBegin(GL_LINE_LOOP);
    glVertex3f(left, marker_y, front);
    glVertex3f(left, marker_y, back);
    glVertex3f(right, marker_y, back);
    glVertex3f(right, marker_y, front);
    glEnd();
}

void draw_axis_marker(const Vec3& origin) {
    constexpr float axis_length = 2.5F;
    const float y = origin.y + 0.08F;

    glLineWidth(3.0F);
    glBegin(GL_LINES);
    color(1.0F, 0.15F, 0.15F);
    glVertex3f(origin.x, y, origin.z);
    glVertex3f(origin.x + axis_length, y, origin.z);
    color(0.15F, 1.0F, 0.15F);
    glVertex3f(origin.x, y, origin.z);
    glVertex3f(origin.x, y + axis_length, origin.z);
    color(0.15F, 0.35F, 1.0F);
    glVertex3f(origin.x, y, origin.z);
    glVertex3f(origin.x, y, origin.z + axis_length);
    glEnd();
    glLineWidth(1.0F);
}

void draw_edge_object(const World& world, Int2 position, Direction direction, EdgeObject edge) {
    if (edge.type == EdgeType::empty) {
        return;
    }
    const Int2 offset = direction_offset(direction);
    const float current_y = get_surface_world_y(world, position.x, position.y);
    const float neighbor_y = get_surface_world_y(world, position.x + offset.x, position.y + offset.y);
    const float base_y = std::max(current_y, neighbor_y);
    const Vec3 center = tile_to_world_position(position.x, position.y, 0);
    const bool horizontal = direction == Direction::north || direction == Direction::south;
    const float x = center.x + static_cast<float>(offset.x) * TILE_SIZE * 0.5F;
    const float z = center.z + static_cast<float>(offset.y) * TILE_SIZE * 0.5F;

    switch (edge.type) {
    case EdgeType::wall: color(0.58F, 0.42F, 0.25F); break;
    case EdgeType::door: color(edge.door_open ? 0.30F : 0.72F, edge.door_open ? 0.75F : 0.45F, 0.18F); break;
    case EdgeType::window: color(0.22F, 0.70F, 0.92F); break;
    case EdgeType::empty: return;
    }
    const float height = edge.type == EdgeType::window ? 1.5F : 2.3F;
    draw_box(
        x,
        base_y,
        z,
        horizontal ? TILE_SIZE : 0.12F,
        horizontal ? 0.12F : TILE_SIZE,
        height);
}

void draw_tile_outline(const World& world, Int2 tile, float red, float green, float blue) {
    const Vec3 center = tile_to_world_position(world, tile);
    const float half = TILE_SIZE * 0.5F;
    color(red, green, blue);
    glBegin(GL_LINE_LOOP);
    glVertex3f(center.x - half, center.y + 0.05F, center.z - half);
    glVertex3f(center.x - half, center.y + 0.05F, center.z + half);
    glVertex3f(center.x + half, center.y + 0.05F, center.z + half);
    glVertex3f(center.x + half, center.y + 0.05F, center.z - half);
    glEnd();
}

void draw_rectangle_outline(const World& world, Int2 a, Int2 b, float red, float green, float blue) {
    const int min_x = std::min(a.x, b.x);
    const int max_x = std::max(a.x, b.x);
    const int min_y = std::min(a.y, b.y);
    const int max_y = std::max(a.y, b.y);
    for (int x = min_x; x <= max_x; ++x) {
        draw_tile_outline(world, {x, min_y}, red, green, blue);
        if (min_y != max_y) draw_tile_outline(world, {x, max_y}, red, green, blue);
    }
    for (int y = min_y + 1; y < max_y; ++y) {
        draw_tile_outline(world, {min_x, y}, red, green, blue);
        if (min_x != max_x) draw_tile_outline(world, {max_x, y}, red, green, blue);
    }
}

void draw_editor_overlay(const World& world, const BuildingEditorState& editor, const BuildingBlueprint* ghost) {
    if (!editor.enabled) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glLineWidth(3.0F);
    if (editor.hovered.valid) {
        draw_tile_outline(world, editor.hovered.tile, 1.0F, 0.95F, 0.2F);
        draw_edge_object(world, editor.hovered.tile, editor.hovered.edge, {EdgeType::window, false});
    }
    if (editor.blueprint_min && editor.blueprint_max) {
        draw_rectangle_outline(world, *editor.blueprint_min, *editor.blueprint_max, 0.95F, 0.25F, 0.85F);
    }
    if (editor.drag_start && editor.hovered.valid) {
        draw_rectangle_outline(world, *editor.drag_start, editor.hovered.tile, 1.0F, 1.0F, 1.0F);
    }
    for (const BlueprintLootZone& zone : editor.authored_loot_zones) {
        draw_rectangle_outline(world, zone.min, zone.max, 0.25F, 0.95F, 0.35F);
    }
    if (ghost && editor.tool == BuildingTool::place_blueprint && editor.hovered.valid) {
        draw_rectangle_outline(
            world,
            editor.hovered.tile,
            {editor.hovered.tile.x + ghost->width - 1, editor.hovered.tile.y + ghost->height - 1},
            0.25F,
            0.75F,
            1.0F);
    }
    glLineWidth(1.0F);
    glEnable(GL_DEPTH_TEST);
}
} // namespace

void Renderer::sync_meshes(const World& world) {
    std::erase_if(chunk_meshes_, [&](const auto& entry) {
        return !world.loaded_chunks().contains(entry.first);
    });

    for (const auto& [coordinate, chunk] : world.loaded_chunks()) {
        (void)chunk;
        const std::uint64_t signature = chunk_mesh::source_signature(world, coordinate);
        const auto existing = chunk_meshes_.find(coordinate);
        if (existing == chunk_meshes_.end() || existing->second.source_signature != signature) {
            chunk_meshes_.insert_or_assign(coordinate, chunk_mesh::build(world, coordinate));
        }
    }

    stats_.generated_mesh_vertices = 0;
    for (const auto& [coordinate, mesh] : chunk_meshes_) {
        (void)coordinate;
        stats_.generated_mesh_vertices += mesh.vertices.size();
    }
}

void Renderer::render(const World& world, const EntityStore& entities, const Entity& player, RenderOptions options) {
    fps_timer_ += options.frame_dt;
    ++fps_frames_;
    if (fps_timer_ >= 1.0) {
        stats_.fps = static_cast<double>(fps_frames_) / fps_timer_;
        fps_timer_ = 0.0;
        fps_frames_ = 0;
    }

    sync_meshes(world);
    stats_.loaded_chunks = world.loaded_chunk_count();
    stats_.visible_chunks = 0;
    stats_.terrain_draw_calls = 0;
    stats_.terrain_triangles = 0;

    glViewport(0, 0, options.width, options.height);
    glClearColor(0.08F, 0.10F, 0.12F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    const float aspect = static_cast<float>(options.width) / static_cast<float>(std::max(options.height, 1));
    const float half_view_height = std::max(options.camera_half_view_height, 1.0F);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(
        -half_view_height * aspect,
        half_view_height * aspect,
        -half_view_height,
        half_view_height,
        -100.0,
        100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(options.camera_pitch_degrees, 1.0F, 0.0F, 0.0F);
    glRotatef(options.camera_yaw_degrees, 0.0F, 1.0F, 0.0F);
    glTranslatef(-options.camera_target.x, -options.camera_target.y, -options.camera_target.z);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    for (const auto& [coordinate, mesh] : chunk_meshes_) {
        if (mesh.vertices.empty() ||
            !is_chunk_visible(
                coordinate,
                options.camera_target,
                aspect,
                half_view_height,
                options.render_margin_chunks)) {
            continue;
        }

        const TerrainVertex* first = mesh.vertices.data();
        glVertexPointer(3, GL_FLOAT, sizeof(TerrainVertex), &first->position);
        glColorPointer(3, GL_FLOAT, sizeof(TerrainVertex), &first->red);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.vertices.size()));

        ++stats_.visible_chunks;
        ++stats_.terrain_draw_calls;
        stats_.terrain_triangles += mesh.vertices.size() / 3;

        if (options.show_chunk_borders) {
            draw_chunk_border(coordinate);
        }
    }
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    glEnable(GL_CULL_FACE);
    for (const auto& [coordinate, chunk] : world.loaded_chunks()) {
        (void)coordinate;
        for (int local_y = 0; local_y < chunk_size; ++local_y) {
            for (int local_x = 0; local_x < chunk_size; ++local_x) {
                const Int2 position{
                    chunk.coordinate.x * chunk_size + local_x,
                    chunk.coordinate.y * chunk_size + local_y,
                };
                draw_edge_object(world, position, Direction::north, world.get_edge(position, Direction::north));
                draw_edge_object(world, position, Direction::west, world.get_edge(position, Direction::west));
                if (!world.get_tile({position.x + 1, position.y})) {
                    draw_edge_object(world, position, Direction::east, world.get_edge(position, Direction::east));
                }
                if (!world.get_tile({position.x, position.y + 1})) {
                    draw_edge_object(world, position, Direction::south, world.get_edge(position, Direction::south));
                }
                const Tile* tile = world.get_tile(position);
                if (tile && !tile->furniture_id.empty()) {
                    color(0.65F, 0.35F, 0.72F);
                    const Vec3 center = tile_to_world_position(world, position);
                    draw_box(center.x, center.y + 0.02F, center.z, 0.62F, 0.62F, 0.65F);
                }
            }
        }
    }

    for (const Entity& entity : entities.all()) {
        if (!entity.active) {
            continue;
        }
        switch (entity.type) {
        case EntityType::player:
            color(0.20F, 0.55F, 0.95F);
            draw_box(entity.visual_position.x, entity.visual_position.y, entity.visual_position.z, 0.55F, 0.55F, 1.1F);
            break;
        case EntityType::zombie:
            color(0.35F, 0.72F, 0.30F);
            draw_box(entity.visual_position.x, entity.visual_position.y, entity.visual_position.z, 0.55F, 0.55F, 1.0F);
            break;
        case EntityType::item:
            color(0.95F, 0.72F, 0.18F);
            draw_box(entity.visual_position.x, entity.visual_position.y + 0.02F, entity.visual_position.z, 0.32F, 0.32F, 0.18F);
            break;
        }
    }

    if (options.show_axis_marker) {
        glDisable(GL_CULL_FACE);
        draw_axis_marker(player.visual_position);
    }
    if (options.building_editor) {
        draw_editor_overlay(world, *options.building_editor, options.ghost_blueprint);
    }
}

namespace {
void draw_box(float x, float base_y, float z, float width, float depth, float height) {
    const float left = x - width * 0.5F;
    const float right = x + width * 0.5F;
    const float front = z - depth * 0.5F;
    const float back = z + depth * 0.5F;
    const float top = base_y + height;

    glBegin(GL_QUADS);
    glVertex3f(left, top, front);
    glVertex3f(left, top, back);
    glVertex3f(right, top, back);
    glVertex3f(right, top, front);
    glVertex3f(left, base_y, back);
    glVertex3f(right, base_y, back);
    glVertex3f(right, top, back);
    glVertex3f(left, top, back);
    glVertex3f(right, base_y, front);
    glVertex3f(right, top, front);
    glVertex3f(right, top, back);
    glVertex3f(right, base_y, back);
    glVertex3f(left, base_y, front);
    glVertex3f(left, base_y, back);
    glVertex3f(left, top, back);
    glVertex3f(left, top, front);
    glVertex3f(left, base_y, front);
    glVertex3f(left, top, front);
    glVertex3f(right, top, front);
    glVertex3f(right, base_y, front);
    glEnd();
}
} // namespace
