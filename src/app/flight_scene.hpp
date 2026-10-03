#pragma once

#include "app/flight_preview.hpp"
#include "render/glyph_atlas.hpp"

#include <cstddef>

namespace river_raid {

// Compose the current native flight presentation from one simulation snapshot.
[[nodiscard]] Frame make_flight_scene(const FlightPreview& flight);

} // namespace river_raid
