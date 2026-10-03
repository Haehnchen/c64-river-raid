#include "audio/voice_synth.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    using namespace river_raid;
    try {
        VoiceSynth synth(48000, 985248);
        std::vector<float> silence(2048, 1);
        synth.render(silence);
        require(std::ranges::all_of(silence, [](float v) { return v == 0; }),
                "Initial output must be silent");
        AudioControlOutput output;
        output.voices[0] = {0x2000, AudioVoiceControl::TriangleGate, 15, 0, false};
        synth.set_output(output);
        std::vector<float> tone(48000);
        synth.render(tone);
        require(std::ranges::any_of(tone, [](float v) { return v > .2f; }), "Missing tone");
        int crossings = 0;
        for (std::size_t i = 1; i < tone.size(); ++i) {
            if (tone[i - 1] <= 0 && tone[i] > 0) ++crossings;
        }
        const double frequency = 8192.0 * 985248.0 / 16777216.0;
        require(std::abs(crossings - frequency) < 2, "Triangle frequency mismatch");

        output.voices[1] = {0x4000, AudioVoiceControl::NoiseGate, 8, 3, true};
        output.voices[2] = {0x8000, AudioVoiceControl::TriangleGate, 12, 7, true};
        synth.reset();
        synth.set_output(output);
        std::vector<float> whole(48000), pieces(48000);
        synth.render(whole);
        VoiceSynth chunked(48000, 985248);
        chunked.set_output(output);
        for (std::size_t offset = 0; offset < pieces.size();) {
            const auto count = std::min<std::size_t>(137, pieces.size() - offset);
            chunked.render(std::span(pieces).subspan(offset, count));
            offset += count;
        }
        require(whole == pieces, "Audio depends on buffer partitioning");
        require(std::ranges::all_of(whole, [](float v) {
            return std::isfinite(v) && std::abs(v) <= .75f;
        }), "Invalid or clipped mix");
        synth.reset();
        synth.set_output(output);
        synth.render(pieces);
        require(whole == pieces, "Reset must reproduce audio");

        VoiceSynth retriggered(48000, 985248), held(48000, 985248);
        AudioControlOutput quiet;
        quiet.voices[0] = {0x2000, AudioVoiceControl::TriangleGate, 2, 0, false};
        retriggered.set_output(quiet);
        held.set_output(quiet);
        std::array<float, 2048> before{};
        retriggered.render(before);
        held.render(before);
        quiet.voices[0].gate_restarted = true;
        retriggered.set_output(quiet);
        std::array<float, 256> attack{}, sustained{};
        retriggered.render(attack);
        held.render(sustained);
        require(attack != sustained, "Gate retrigger ignored");

        synth.set_output({});
        synth.render(silence);
        require(std::ranges::all_of(silence, [](float v) { return v == 0; }),
                "Cleared waveform must be silent");
        synth.reset();
        output = {};
        output.voices[0] = {0x2000, AudioVoiceControl::NoiseGate, 15, 0, false};
        synth.set_output(output);
        synth.render(tone);
        require(std::ranges::min(tone) < -.1f && std::ranges::max(tone) > .1f,
                "Noise must vary across positive and negative amplitudes");
        VoiceSynth ntsc(48000, 1022730);
        ntsc.set_output(output);
        ntsc.render(pieces);
        require(tone != pieces, "Chip clock must affect synthesis");
        bool rejected = false;
        try { VoiceSynth invalid(0, 985248); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Zero sample rate accepted");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
