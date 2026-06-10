#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

struct Int2 {
    int x = 0;
    int y = 0;

    friend bool operator==(Int2, Int2) = default;
};

struct Int2Hash {
    std::size_t operator()(Int2 value) const noexcept {
        const auto x = static_cast<std::uint32_t>(value.x);
        const auto y = static_cast<std::uint32_t>(value.y);
        return static_cast<std::size_t>((static_cast<std::uint64_t>(x) << 32U) ^ y);
    }
};

struct Vec3 {
    // Render-space convention: X east/west, Y vertical/up, Z north/south.
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

inline Vec3 move_towards(Vec3 current, Vec3 target, float max_distance) {
    const float dx = target.x - current.x;
    const float dy = target.y - current.y;
    const float dz = target.z - current.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (distance <= max_distance || distance == 0.0F) {
        return target;
    }

    const float scale = max_distance / distance;
    return {current.x + dx * scale, current.y + dy * scale, current.z + dz * scale};
}
