#include "app/Application.h"

#include "renderer/Renderer.h"

#include <SDL3/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <ranges>
#include <stdexcept>
#include <vector>

namespace {
constexpr double fixed_dt = 1.0 / 60.0;
constexpr std::uint64_t default_world_seed = 0xC0FFEE1234ULL;
constexpr int default_loading_radius = 2;
#ifndef CNG_DATA_ROOT
#define CNG_DATA_ROOT "data"
#endif

std::uint64_t random_seed() {
    std::random_device device;
    const auto time = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return (static_cast<std::uint64_t>(device()) << 32U) ^ device() ^ time;
}

const BuildingBlueprint* selected_blueprint(const BuildingEditorState& editor, const DataLoadResult& data) {
    std::vector<const BuildingBlueprint*> blueprints;
    for (const auto& [id, blueprint] : data.data.building_blueprints) {
        (void)id;
        blueprints.push_back(&blueprint);
    }
    std::ranges::sort(blueprints, {}, &BuildingBlueprint::id);
    if (blueprints.empty()) return nullptr;
    return blueprints[static_cast<std::size_t>(
        std::clamp(editor.selected_blueprint, 0, static_cast<int>(blueprints.size()) - 1))];
}
} // namespace

Application::Application()
    : world_(default_world_seed, default_loading_radius),
      data_root_(CNG_DATA_ROOT) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window_ = SDL_CreateWindow("CNG - Worldgen v0.1", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error(SDL_GetError());
    }
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL3_InitForOpenGL(window_, gl_context_) || !ImGui_ImplOpenGL3_Init("#version 120")) {
        throw std::runtime_error("Could not initialize Dear ImGui.");
    }

    player_id_ = entities_.spawn(EntityType::player, {}, "player").id;
    place_player_at_spawn();
    seed_editor_ = world_.seed();
    reload_game_data();
}

Application::~Application() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (gl_context_) {
        SDL_GL_DestroyContext(gl_context_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

int Application::run() {
    std::uint64_t previous_ticks = SDL_GetTicksNS();
    double accumulator = 0.0;

    while (running_) {
        const std::uint64_t now = SDL_GetTicksNS();
        const double frame_dt = std::min(static_cast<double>(now - previous_ticks) / 1'000'000'000.0, 0.25);
        previous_ticks = now;
        accumulator += frame_dt;

        poll_input();
        if (!running_) {
            break;
        }
        while (accumulator >= fixed_dt) {
            fixed_update(fixed_dt);
            accumulator -= fixed_dt;
        }
        handle_actions();

        // Visual positions are presentation state, so they follow the variable render timestep.
        movement::update_visual_positions(entities_, world_, static_cast<float>(frame_dt));

        int width = 1;
        int height = 1;
        SDL_GetWindowSizeInPixels(window_, &width, &height);
        const Entity& player = *entities_.find(player_id_);
        const Vec3 camera_target = building_editor_.enabled ? editor_camera_target_ : player.visual_position;
        renderer_.render(
            world_,
            entities_,
            player,
            {.width = width,
             .height = height,
             .frame_dt = static_cast<float>(frame_dt),
             .camera_half_view_height = camera_.half_view_height,
             .camera_pitch_degrees = camera_.pitch_degrees,
             .camera_yaw_degrees = camera_.yaw_degrees,
             .camera_target = camera_target,
             .render_margin_chunks = render_margin_chunks_,
             .show_chunk_borders = show_chunk_borders_,
             .show_axis_marker = show_axis_marker_,
             .building_editor = &building_editor_,
             .ghost_blueprint = selected_blueprint(building_editor_, data_load_)});

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        draw_debug_ui();
        if (show_imgui_demo_) {
            ImGui::ShowDemoWindow(&show_imgui_demo_);
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window_);
    }
    return 0;
}

void Application::poll_input() {
    input_.regenerate_pressed = false;
    input_.new_seed_pressed = false;
    input_.toggle_chunk_borders_pressed = false;
    input_.toggle_debug_window_pressed = false;
    input_.toggle_editor_pressed = false;
    input_.quit_requested = false;
    input_.mouse_wheel_y = 0.0F;
    input_.mouse_delta_x = 0.0F;
    input_.left_mouse_pressed = false;
    input_.left_mouse_released = false;
    input_.right_mouse_pressed = false;
    input_.rotate_tool_pressed = false;
    input_.building_tool_hotkey = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
            input_.quit_requested = true;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            input_.quit_requested |= event.key.scancode == SDL_SCANCODE_ESCAPE;
            input_.toggle_debug_window_pressed |= event.key.scancode == SDL_SCANCODE_F10;
            if (!ImGui::GetIO().WantCaptureKeyboard) {
                input_.toggle_editor_pressed |= event.key.scancode == SDL_SCANCODE_F8;
                input_.regenerate_pressed |= event.key.scancode == SDL_SCANCODE_F5;
                input_.new_seed_pressed |= event.key.scancode == SDL_SCANCODE_F6;
                input_.toggle_chunk_borders_pressed |= event.key.scancode == SDL_SCANCODE_F9;
                input_.rotate_tool_pressed |= event.key.scancode == SDL_SCANCODE_R;
                if (event.key.scancode >= SDL_SCANCODE_1 && event.key.scancode <= SDL_SCANCODE_9) {
                    input_.building_tool_hotkey = static_cast<int>(event.key.scancode - SDL_SCANCODE_1) + 1;
                }
            }
        }
        if (!ImGui::GetIO().WantCaptureMouse && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            input_.left_mouse_pressed |= event.button.button == SDL_BUTTON_LEFT;
            input_.right_mouse_pressed |= event.button.button == SDL_BUTTON_RIGHT;
        }
        if (!ImGui::GetIO().WantCaptureMouse && event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            input_.left_mouse_released |= event.button.button == SDL_BUTTON_LEFT;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            input_.mouse_delta_x += event.motion.xrel;
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL && !ImGui::GetIO().WantCaptureMouse) {
            input_.mouse_wheel_y += event.wheel.y;
        }
    }

    const bool* keys = SDL_GetKeyboardState(nullptr);
    const bool keyboard_available = !ImGui::GetIO().WantCaptureKeyboard;
    input_.move_up = keyboard_available && keys[SDL_SCANCODE_W];
    input_.move_down = keyboard_available && keys[SDL_SCANCODE_S];
    input_.move_left = keyboard_available && keys[SDL_SCANCODE_A];
    input_.move_right = keyboard_available && keys[SDL_SCANCODE_D];
    const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&input_.mouse_x, &input_.mouse_y);
    const bool mouse_available = !ImGui::GetIO().WantCaptureMouse;
    input_.left_mouse_down = mouse_available && (mouse_buttons & SDL_BUTTON_LMASK) != 0;
    input_.middle_mouse_down = mouse_available && (mouse_buttons & SDL_BUTTON_MMASK) != 0;
    running_ = !input_.quit_requested;
}

void Application::fixed_update(double step) {
    Entity& player = *entities_.find(player_id_);
    if (building_editor_.enabled) {
        constexpr float editor_pan_speed = 14.0F;
        editor_camera_target_.x += (static_cast<int>(input_.move_right) - static_cast<int>(input_.move_left))
            * editor_pan_speed * static_cast<float>(step);
        editor_camera_target_.z += (static_cast<int>(input_.move_down) - static_cast<int>(input_.move_up))
            * editor_pan_speed * static_cast<float>(step);
        world_.update_loaded_chunks({
            static_cast<int>(std::round(editor_camera_target_.x / TILE_SIZE)),
            static_cast<int>(std::round(editor_camera_target_.z / TILE_SIZE)),
        });
        return;
    }
    world_.update_loaded_chunks(player.tile_position);
    movement::update_player(player, player_movement_, input_, world_, step);
    world_.update_loaded_chunks(player.tile_position);
}

void Application::handle_actions() {
    Entity& player = *entities_.find(player_id_);
    zoom_camera(camera_, input_.mouse_wheel_y);
    if (input_.middle_mouse_down) {
        rotate_camera(camera_, input_.mouse_delta_x);
    }
    if (input_.toggle_editor_pressed) {
        building_editor_.enabled = !building_editor_.enabled;
        if (building_editor_.enabled) editor_camera_target_ = player.visual_position;
        building_editor_.drag_start.reset();
    }
    if (building_editor_.enabled && input_.building_tool_hotkey >= 1 && input_.building_tool_hotkey <= 9) {
        building_editor_.tool = static_cast<BuildingTool>(input_.building_tool_hotkey - 1);
    }
    if (building_editor_.enabled) {
        int width = 1;
        int height = 1;
        SDL_GetWindowSizeInPixels(window_, &width, &height);
        building_editor_.hovered = pick_world_tile(
            world_, camera_, editor_camera_target_, width, height, input_.mouse_x, input_.mouse_y);
        update_building_editor(
            building_editor_,
            {.mouse_x = input_.mouse_x,
             .mouse_y = input_.mouse_y,
             .left_pressed = input_.left_mouse_pressed,
             .left_down = input_.left_mouse_down,
             .left_released = input_.left_mouse_released,
             .right_pressed = input_.right_mouse_pressed,
             .rotate_pressed = input_.rotate_tool_pressed},
            world_,
            data_load_);
    }
    if (input_.regenerate_pressed) {
        world_.regenerate(world_.seed(), player.tile_position);
    }
    if (input_.new_seed_pressed) {
        world_.regenerate(random_seed(), {});
        place_player_at_spawn();
        seed_editor_ = world_.seed();
    }
    if (input_.toggle_chunk_borders_pressed) {
        show_chunk_borders_ = !show_chunk_borders_;
    }
    if (input_.toggle_debug_window_pressed) {
        show_debug_window_ = !show_debug_window_;
    }
}

void Application::place_player_at_spawn() {
    Entity& player = *entities_.find(player_id_);
    player.tile_position = world_.find_walkable_spawn();
    player.visual_position = tile_to_world_position(world_, player.tile_position);
    player_movement_ = {};
    world_.update_loaded_chunks(player.tile_position);
}

void Application::reload_game_data() {
    data_load_ = load_game_data(data_root_);
}

void Application::draw_debug_ui() {
    if (!show_debug_window_) {
        return;
    }

    const Entity& player = *entities_.find(player_id_);
    const RendererStats& render_stats = renderer_.stats();

    ImGui::SetNextWindowSize(ImVec2(390.0F, 500.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("CNG Debug / Status", &show_debug_window_)) {
        ImGui::End();
        return;
    }

    ImGui::Text("FPS: %.1f", render_stats.fps);
    ImGui::SeparatorText("Renderer");
    ImGui::Text("Loaded chunks: %zu", render_stats.loaded_chunks);
    ImGui::Text("Visible chunks: %zu", render_stats.visible_chunks);
    ImGui::Text("Terrain draw calls: %zu", render_stats.terrain_draw_calls);
    ImGui::Text("Terrain triangles: %zu", render_stats.terrain_triangles);
    ImGui::Text("Cached mesh vertices: %zu", render_stats.generated_mesh_vertices);
    ImGui::Text("Camera visible half-height: %.1f", camera_.half_view_height);
    if (ImGui::SliderFloat(
            "Camera zoom",
            &camera_.half_view_height,
            MIN_CAMERA_HALF_VIEW_HEIGHT,
            MAX_CAMERA_HALF_VIEW_HEIGHT,
            "%.1f")) {
        camera_.half_view_height = std::clamp(
            camera_.half_view_height,
            MIN_CAMERA_HALF_VIEW_HEIGHT,
            MAX_CAMERA_HALF_VIEW_HEIGHT);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset zoom")) {
        reset_camera_zoom(camera_);
    }
    ImGui::SliderFloat("Render margin (chunks)", &render_margin_chunks_, 0.0F, 2.0F, "%.1f");
    ImGui::TextDisabled("Render margin only uses chunks available inside the loading radius.");
    ImGui::Checkbox("Chunk borders (F9)", &show_chunk_borders_);
    ImGui::Checkbox("XYZ axis marker", &show_axis_marker_);

    ImGui::SeparatorText("World");
    ImGui::Text("Active seed: %llu", static_cast<unsigned long long>(world_.seed()));
    ImGui::InputScalar("Seed", ImGuiDataType_U64, &seed_editor_);
    if (ImGui::Button("Regenerate seed")) {
        world_.regenerate(seed_editor_, {});
        place_player_at_spawn();
    }
    ImGui::SameLine();
    if (ImGui::Button("New random seed")) {
        world_.regenerate(random_seed(), {});
        place_player_at_spawn();
        seed_editor_ = world_.seed();
    }
    int loading_radius = world_.loading_radius();
    if (ImGui::SliderInt("Loading radius", &loading_radius, 0, 8)) {
        world_.set_loading_radius(loading_radius);
        world_.update_loaded_chunks(player.tile_position);
    }

    const Int2 chunk = world_to_chunk(player.tile_position);
    const Tile* tile = world_.get_tile(player.tile_position);
    ImGui::SeparatorText("Player");
    ImGui::Text("Tile: %d, %d", player.tile_position.x, player.tile_position.y);
    ImGui::Text("Chunk: %d, %d", chunk.x, chunk.y);
    ImGui::Text("Visual XYZ: %.2f, %.2f, %.2f",
        player.visual_position.x, player.visual_position.y, player.visual_position.z);
    if (tile) {
        ImGui::Text("Terrain: %s", terrain_name(tile->terrain));
        ImGui::Text("Elevation: %d", tile->elevation);
        ImGui::Text("Surface world Y: %.2f",
            get_surface_world_y(world_, player.tile_position.x, player.tile_position.y));
    }
    if (player_movement_.blocked_by_height) {
        ImGui::TextColored(ImVec4(1.0F, 0.45F, 0.25F, 1.0F),
            "Height blocked: target (%d, %d), elevation %d from %d",
            player_movement_.blocked_target.x,
            player_movement_.blocked_target.y,
            player_movement_.blocked_target_elevation,
            player_movement_.blocked_from_elevation);
    }

    ImGui::SeparatorText("Tools");
    ImGui::Checkbox("ImGui demo window", &show_imgui_demo_);

    ImGui::SeparatorText("JSON Data");
    ImGui::Text("Data root: %s", data_root_.string().c_str());
    ImGui::Text("JSON files scanned: %zu", data_load_.files_scanned);
    ImGui::Text("Items loaded: %zu", data_load_.data.items.size());
    ImGui::Text("Building blueprints loaded: %zu", data_load_.data.building_blueprints.size());
    if (data_load_.ok()) {
        ImGui::TextColored(ImVec4(0.35F, 0.95F, 0.45F, 1.0F), "Data validation passed");
    } else {
        ImGui::TextColored(ImVec4(1.0F, 0.35F, 0.25F, 1.0F),
            "Data validation errors: %zu", data_load_.errors.size());
    }
    if (ImGui::Button("Reload JSON data")) {
        reload_game_data();
    }

    if (ImGui::CollapsingHeader("Loaded items")) {
        for (const auto& [id, item] : data_load_.data.items) {
            if (ImGui::TreeNode(id.c_str(), "%s (%s)", item.name.c_str(), id.c_str())) {
                ImGui::Text("Sprite: %s", item.sprite.c_str());
                ImGui::Text("Weight: %.2f kg", item.weight_kg);
                ImGui::Text("Volume: %.2f L", item.volume_l);
                if (item.weapon) {
                    ImGui::Text("Weapon: bash %d, cut %d, time %d, stamina %d",
                        item.weapon->damage_bash,
                        item.weapon->damage_cut,
                        item.weapon->attack_time,
                        item.weapon->stamina_cost);
                }
                ImGui::TreePop();
            }
        }
    }
    if (!data_load_.errors.empty() && ImGui::CollapsingHeader("Data errors", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const DataError& error : data_load_.errors) {
            ImGui::BulletText("%s: %s", error.file.string().c_str(), error.message.c_str());
        }
    }

    draw_building_editor_status(building_editor_, world_, data_load_, data_root_);

    ImGui::TextDisabled("F10 toggles this window. Escape quits.");
    ImGui::End();
}
