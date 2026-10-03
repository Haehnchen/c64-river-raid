#include "app/audio_setup.hpp"
#include "audio_data.hpp"

namespace river_raid {

AudioController make_audio_controller() {
    namespace data = assets::audio;
    return AudioController({data::engine_voice1_selector,
                            data::effect_voice2_frequency_high,
                            data::bridge_voice3_frequency_high});
}

} // namespace river_raid
