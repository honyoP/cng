#pragma once

#include "app/InputState.h"
#include "data/GameData.h"
#include "editor/BuildingEditor.h"
#include "entities/Entity.h"
#include "renderer/Camera.h"
#include "renderer/Renderer.h"
#include "systems/Movement.h"
#include "world/World.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    void poll_input();
    void fixed_update(double fixed_dt);
    void handle_actions();
    void place_player_at_spawn();
    void reload_game_data();
    void draw_debug_ui();

    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    bool running_ = true;

    InputState input_;
    World world_;
    EntityStore entities_;
    EntityId player_id_ = 0;
    PlayerMovementState player_movement_;
    Renderer renderer_;
    CameraView camera_;
    Vec3 editor_camera_target_{};
    bool show_chunk_borders_ = false;
    bool show_axis_marker_ = true;
    bool show_debug_window_ = true;
    bool show_imgui_demo_ = false;
    float render_margin_chunks_ = 0.5F;
    std::uint64_t seed_editor_ = 0;
    std::filesystem::path data_root_;
    DataLoadResult data_load_;
    BuildingEditorState building_editor_;
};
