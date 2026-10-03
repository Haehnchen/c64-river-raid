cmake_minimum_required(VERSION 3.20)
project(river_raid LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)
include(CTest)

add_library(river_raid_game
    src/game/start_flow.cpp
    src/game/river_demo.cpp
    src/game/river_row.cpp
    src/game/river_viewport.cpp
    src/game/river_generator.cpp
    src/game/world_objects.cpp
    src/game/river_world.cpp
    src/game/demo_clock.cpp
    src/game/demo_simulation.cpp
    src/game/frame_pacer.cpp
    src/game/timed_demo.cpp
    src/game/player_steering.cpp
    src/game/player_shot.cpp
    src/game/shot_contact.cpp
    src/game/shot_contact_resolution.cpp
    src/game/contact_sampling.cpp
    src/game/bridge_sprites.cpp
    src/game/audio_control.cpp
    src/game/auxiliary_projectile.cpp
    src/game/gap_contact.cpp
    src/game/life_cycle.cpp
    src/game/session_idle.cpp
    src/game/title_delay.cpp
    src/game/idle_watchdog.cpp
)
target_include_directories(river_raid_game PUBLIC src)
target_compile_options(river_raid_game PRIVATE -Wall -Wextra -Wpedantic)

add_library(river_raid_audio
    src/audio/voice_synth.cpp
)
target_include_directories(river_raid_audio PUBLIC src)
target_compile_options(river_raid_audio PRIVATE -Wall -Wextra -Wpedantic)

add_library(river_raid_render
    src/render/glyph_atlas.cpp
    src/render/character_scene.cpp
    src/render/tile_scene.cpp
    src/render/river_scene.cpp
    src/render/options_scene.cpp
    src/render/sprite_scene.cpp
    src/render/object_image.cpp
    src/render/world_object_schedule.cpp
    src/render/world_scene.cpp
    src/render/contact_masks.cpp
    src/render/contact_producer.cpp
)
target_include_directories(river_raid_render PUBLIC src)
target_link_libraries(river_raid_render PUBLIC river_raid_game)
target_compile_options(river_raid_render PRIVATE -Wall -Wextra -Wpedantic)

add_library(river_raid_object_sheet
    src/app/object_sheet.cpp
)
target_include_directories(river_raid_object_sheet PRIVATE src/assets)
target_link_libraries(river_raid_object_sheet PUBLIC river_raid_render)
target_compile_options(river_raid_object_sheet PRIVATE -Wall -Wextra -Wpedantic)

add_library(river_raid_world_scene
    src/app/world_scene_setup.cpp
    src/app/player_overlay.cpp
    src/app/shot_overlay.cpp
    src/app/flight_hud.cpp
    src/app/bridge_overlay.cpp
    src/app/auxiliary_overlay.cpp
    src/app/contact_asset_setup.cpp
)
target_include_directories(river_raid_world_scene PRIVATE src/assets)
target_link_libraries(river_raid_world_scene PUBLIC river_raid_render)
target_compile_options(river_raid_world_scene PRIVATE -Wall -Wextra -Wpedantic)

add_library(river_raid_setup
    src/app/generator_setup.cpp
    src/app/world_setup.cpp
    src/app/demo_setup.cpp
    src/app/demo_timing_setup.cpp
    src/app/flight_preview.cpp
    src/app/flight_start.cpp
    src/app/audio_setup.cpp
    src/app/flight_audio.cpp
    src/app/flight_preview_contacts.cpp
    src/app/player_contact.cpp
    src/app/foreground_contacts.cpp
    src/app/flight_scene.cpp
    src/app/gap_contacts.cpp
    src/app/auxiliary_contacts.cpp
)
target_include_directories(river_raid_setup PRIVATE src/assets)
target_link_libraries(river_raid_setup PUBLIC river_raid_game)
target_link_libraries(river_raid_setup PUBLIC river_raid_audio)
target_link_libraries(river_raid_setup PUBLIC river_raid_world_scene)
target_compile_options(river_raid_setup PRIVATE -Wall -Wextra -Wpedantic)

add_library(river_raid_platform
    src/platform/sdl_preview.cpp
    src/platform/active_time.cpp
    src/platform/sdl_audio.cpp
    src/platform/sdl_joysticks.cpp
)
target_link_libraries(river_raid_platform PUBLIC river_raid_game river_raid_render SDL3::SDL3)
target_compile_options(river_raid_platform PRIVATE -Wall -Wextra -Wpedantic)

add_executable(river_raid
    src/main.cpp
)
if(WIN32)
    enable_language(RC)
    configure_file(src/platform/windows_icon.rc.in windows_icon.rc @ONLY)
    target_sources(river_raid PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/windows_icon.rc")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/windows_icon.rc" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/river_raid.ico")
endif()
target_include_directories(river_raid PRIVATE src/assets)
target_link_libraries(river_raid PRIVATE river_raid_setup river_raid_render river_raid_object_sheet river_raid_world_scene river_raid_platform)
target_compile_options(river_raid PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
