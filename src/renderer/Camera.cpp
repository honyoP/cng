#include "renderer/Camera.h"

#include <algorithm>

namespace {
constexpr float zoom_step = 2.0F;
constexpr float default_half_view_height = 20.0F;
} // namespace

void zoom_camera(CameraView& camera, float mouse_wheel_amount) {
    camera.half_view_height = std::clamp(
        camera.half_view_height - mouse_wheel_amount * zoom_step,
        MIN_CAMERA_HALF_VIEW_HEIGHT,
        MAX_CAMERA_HALF_VIEW_HEIGHT);
}

void reset_camera_zoom(CameraView& camera) {
    camera.half_view_height = default_half_view_height;
}

void rotate_camera(CameraView& camera, float mouse_delta_x) {
    camera.yaw_degrees += mouse_delta_x * 0.25F;
}
