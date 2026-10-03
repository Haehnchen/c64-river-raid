#include "app/flight_preview.hpp"
#include "support/flight_start_helpers.hpp"
#include "fixtures/flight_audio_cases.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require_field(bool matches, std::size_t boundary, std::size_t voice,
                   const char* field) {
    if (!matches) {
        throw std::runtime_error("Audio boundary " + std::to_string(boundary) +
                                 ", voice " + std::to_string(voice + 1) + ": " + field);
    }
}

struct PcmExportStats {
    std::size_t frames{};
    std::size_t clipped_samples{};
    double peak_float{};
    std::uint32_t peak_pcm16{};
};

void write_u16le(std::ostream& output, std::uint16_t value) {
    output.put(static_cast<char>(value & 0xffU));
    output.put(static_cast<char>((value >> 8U) & 0xffU));
}

void write_u32le(std::ostream& output, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        output.put(static_cast<char>((value >> shift) & 0xffU));
}

void write_fourcc(std::ostream& output, std::string_view code) {
    output.write(code.data(), static_cast<std::streamsize>(code.size()));
}

PcmExportStats write_wav(const std::filesystem::path& path,
                         const std::vector<float>& samples) {
    constexpr std::uint32_t sample_rate = 48000;
    constexpr std::uint32_t bytes_per_sample = 2;
    if (samples.size() > (std::numeric_limits<std::uint32_t>::max() - 36U) /
                             bytes_per_sample) {
        throw std::overflow_error("WAV output exceeds the RIFF size limit");
    }
    const auto data_bytes = static_cast<std::uint32_t>(samples.size() * bytes_per_sample);
    PcmExportStats stats{samples.size()};

    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(path, std::ios::binary);
    write_fourcc(output, "RIFF");
    write_u32le(output, 36U + data_bytes);
    write_fourcc(output, "WAVE");
    write_fourcc(output, "fmt ");
    write_u32le(output, 16);
    write_u16le(output, 1); // PCM
    write_u16le(output, 1); // mono
    write_u32le(output, sample_rate);
    write_u32le(output, sample_rate * bytes_per_sample);
    write_u16le(output, bytes_per_sample);
    write_u16le(output, 16);
    write_fourcc(output, "data");
    write_u32le(output, data_bytes);

    for (const float sample : samples) {
        if (!std::isfinite(sample)) throw std::runtime_error("Non-finite PCM sample");
        const auto raw = static_cast<double>(sample);
        stats.peak_float = std::max(stats.peak_float, std::abs(raw));
        if (raw < -1.0 || raw > 1.0) ++stats.clipped_samples;
        const auto bounded = std::clamp(raw, -1.0, 1.0);
        const auto scaled = bounded < 0.0 ? std::lround(bounded * 32768.0)
                                          : std::lround(bounded * 32767.0);
        const auto quantized = static_cast<std::int16_t>(scaled);
        stats.peak_pcm16 = std::max(stats.peak_pcm16,
            static_cast<std::uint32_t>(std::abs(static_cast<std::int32_t>(quantized))));
        write_u16le(output, static_cast<std::uint16_t>(quantized));
    }
    output.close();
    return stats;
}

} // namespace

int main(int argc, char** argv) {
    using namespace river_raid;
    using namespace std::chrono_literals;
    try {
        std::optional<std::filesystem::path> wav_path;
        if (argc == 3 && std::string_view(argv[1]) == "--dump-wav" &&
            std::string_view(argv[2]).size() != 0) {
            wav_path = argv[2];
            if (std::filesystem::exists(*wav_path) || std::filesystem::is_symlink(*wav_path))
                throw std::runtime_error("Refusing to overwrite WAV output");
        } else if (argc != 1) {
            throw std::runtime_error(
                "Usage: flight_audio_session_parity_test [--dump-wav FILE.wav]");
        }

        static_assert(test_data::flight_audio_cases.size() == 241);
        FlightPreview flight;
        flight.start();
        test::wait_for_launch(flight);
        flight.set_controls({false, false, false, false, true});
        std::vector<float> recorded_pcm;
        if (wav_path) recorded_pcm.reserve(241U * 960U);

        std::size_t timer_cases = 0;
        for (std::size_t index = 0; index < test_data::flight_audio_cases.size(); ++index) {
            const auto& original = test_data::flight_audio_cases[index];
            if (original.boundary_index != index)
                throw std::runtime_error("Audio fixture boundary index is not sequential");

            std::size_t admitted = 0;
            for (unsigned slice = 0; slice < 21 && admitted == 0; ++slice)
                admitted = flight.advance_elapsed(1ms);
            if (admitted != 1 || flight.status() != FlightStatus::Flying || flight.lives() != 3)
                throw std::runtime_error("Natural held-fire route diverged at audio boundary " +
                                         std::to_string(index));
            if (wav_path) {
                auto samples = flight.take_audio();
                recorded_pcm.insert(recorded_pcm.end(), samples.begin(), samples.end());
            }

            const auto& native = flight.audio_state().output.voices;
            for (std::size_t voice = 0; voice < native.size(); ++voice) {
                const auto& actual = native[voice];
                const auto& expected = original.voices[voice];
                const bool timer_frequency = voice == 1 && original.voice2_timer_read;
                if (timer_frequency) {
                    require_field(expected.frequency == 0x0100 || expected.frequency == 0x0200,
                                  index, voice, "original timer frequency outside 1/2");
                    require_field(actual.frequency == 0x0100 || actual.frequency == 0x0200,
                                  index, voice, "native timer frequency outside 1/2");
                    ++timer_cases;
                } else {
                    require_field(actual.frequency == expected.frequency, index, voice,
                                  "retained frequency");
                }
                require_field(actual.control == expected.control, index, voice, "control");
                require_field(actual.sustain == expected.sustain, index, voice, "sustain");
                require_field(actual.release == expected.release, index, voice, "release");
                require_field(actual.gate_restarted == expected.gate_restarted, index, voice,
                              "gate restart");
            }
        }
        if (timer_cases != 31)
            throw std::runtime_error("Natural audio route lost voice-2 timer coverage");
        std::cout << "241 natural SID boundaries match non-timer fields; " << timer_cases
                  << " voice-2 frequencies retain source range only\n";
        if (wav_path) {
            const auto stats = write_wav(*wav_path, recorded_pcm);
            constexpr double sample_rate = 48000.0;
            std::cout << std::fixed << std::setprecision(6)
                      << "native_wav=" << wav_path->string()
                      << " region=PAL game=1 option_index=0 input=centered-held-fire"
                      << " scope=active-boundaries-only original_pcm_parity=false"
                      << " boundaries=241 format=pcm_s16le channels=1 sample_rate_hz=48000"
                      << " frames=" << stats.frames
                      << " duration_seconds=" << stats.frames / sample_rate
                      << " peak_float=" << stats.peak_float
                      << " peak_pcm16_abs=" << stats.peak_pcm16
                      << " clipped_samples=" << stats.clipped_samples << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
