#pragma once

struct InputState {
    bool move_up = false;
    bool move_down = false;
    bool move_left = false;
    bool move_right = false;
    float mouse_wheel_y = 0.0F;
    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    float mouse_delta_x = 0.0F;
    bool left_mouse_pressed = false;
    bool left_mouse_down = false;
    bool left_mouse_released = false;
    bool right_mouse_pressed = false;
    bool middle_mouse_down = false;
    bool rotate_tool_pressed = false;
    int building_tool_hotkey = 0;

    // These are one-frame actions, unlike the held movement fields above.
    bool regenerate_pressed = false;
    bool new_seed_pressed = false;
    bool toggle_chunk_borders_pressed = false;
    bool toggle_debug_window_pressed = false;
    bool toggle_editor_pressed = false;
    bool quit_requested = false;
};
