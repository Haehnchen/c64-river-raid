#pragma once

#include "render/character_scene.hpp"

namespace river_raid {

struct ClipRectangle {
    int left{}, top{}, width{}, height{};
};

// Later layers cover earlier layers; palette index 255 is transparent.
void draw_sprite_layers(Frame& destination, std::span<const ImageLayer> layers,
                        const Palette& palette, ClipRectangle clip);

} // namespace river_raid
