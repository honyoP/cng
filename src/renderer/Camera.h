#pragma once

struct CameraView {
    // Orthographic zoom changes the visible world height without changing the
    // existing camera projection type, pitch, or yaw.
    float half_view_height = 20.0F;
    float pitch_degrees = 35.0F;
    float yaw_degrees = -45.0F;
};

constexpr float MIN_CAMERA_HALF_VIEW_HEIGHT = 8.0F;
constexpr float MAX_CAMERA_HALF_VIEW_HEIGHT = 48.0F;

void zoom_camera(CameraView& camera, float mouse_wheel_amount);
void reset_camera_zoom(CameraView& camera);
void rotate_camera(CameraView& camera, float mouse_delta_x);
