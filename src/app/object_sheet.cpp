#include "app/object_sheet.hpp"
#include "render/object_image.hpp"
#include "render/sprite_scene.hpp"
#include "object_sprites.hpp"
#include "presentation_palette.hpp"

#include <array>
#include <cstddef>

namespace river_raid {

Frame make_object_sprite_sheet() {
    namespace data = assets::object_sprites;
    const auto& palette = assets::presentation_palette;
    constexpr int width = 512;
    constexpr int height = 224;
    Frame sheet{width, height, std::vector<std::uint32_t>(width * height, palette[0])};
    for (std::size_t kind = 0; kind < 16; ++kind) {
        // Explosion frames retain the base object's style; use the same specimen style.
        const auto base_kind = kind >= 2 && kind <= 6 ? 13 : kind;
        const auto& style = data::object_sprite_styles[base_kind];
        for (std::size_t direction = 0; direction < 2; ++direction) {
            const auto& mapping = data::object_sprites[direction * data::kind_count + kind];
            for (std::size_t alternative = 0; alternative < 2; ++alternative) {
                const auto index = kind * 4 + direction * 2 + alternative;
                const auto left = index % 8 * 64 + 8;
                const auto top = index / 8 * 28 + 3;
                for (std::size_t y = 0; y < 21; ++y) {
                    for (std::size_t x = 0; x < 48; ++x) {
                        sheet.pixels[(top + y) * width + left + x] = palette[5];
                    }
                }
                const auto& source = data::sprite_images[alternative == 0 ? mapping.primary_image : mapping.alternate_image];
                const auto image = colorize_object_image(source.width, source.height, source.roles,
                    {style.individual_color, style.multicolor_1, style.multicolor_2, style.expand_horizontal});
                const auto layer_left = static_cast<int>(left);
                const auto layer_top = static_cast<int>(top);
                const std::array layers{ImageLayer{image.pixels, image.width, image.height,
                                                  layer_left, layer_top}};
                draw_sprite_layers(sheet, layers, palette, {layer_left, layer_top, 48, 21});
            }
        }
    }
    return sheet;
}

} // namespace river_raid
