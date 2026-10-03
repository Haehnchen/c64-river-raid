if(NOT DEFINED RIVER_RAID_EXECUTABLE OR "${RIVER_RAID_EXECUTABLE}" STREQUAL "")
    message(FATAL_ERROR "Missing RIVER_RAID_EXECUTABLE")
endif()
if(NOT EXISTS "${RIVER_RAID_EXECUTABLE}")
    message(FATAL_ERROR "River Raid executable does not exist: ${RIVER_RAID_EXECUTABLE}")
endif()

unset(ENV{WAYLAND_DISPLAY})
set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{SDL_RENDER_DRIVER} software)
set(ENV{SDL_AUDIODRIVER} dummy)

execute_process(
    COMMAND "${RIVER_RAID_EXECUTABLE}" --help
    RESULT_VARIABLE help_result
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error
    TIMEOUT 5)
if(NOT help_result EQUAL 0 OR NOT help_output MATCHES "--dump-flight" OR
   NOT help_error STREQUAL "")
    message(FATAL_ERROR "--help failed (${help_result})\nstdout: ${help_output}\nstderr: ${help_error}")
endif()

foreach(invalid_case IN ITEMS extra_help missing_dump_path unknown_mode)
    if(invalid_case STREQUAL "extra_help")
        set(arguments --help extra)
        set(expected_error "Unexpected help arguments")
    elseif(invalid_case STREQUAL "missing_dump_path")
        set(arguments --dump-title)
        set(expected_error "Unknown arguments; use --help")
    else()
        set(arguments --unknown)
        set(expected_error "Unknown arguments; use --help")
    endif()
    execute_process(
        COMMAND "${RIVER_RAID_EXECUTABLE}" ${arguments}
        RESULT_VARIABLE invalid_result
        OUTPUT_VARIABLE invalid_output
        ERROR_VARIABLE invalid_error
        TIMEOUT 5)
    if(invalid_result EQUAL 0 OR NOT invalid_output STREQUAL "" OR
       NOT invalid_error MATCHES "${expected_error}")
        message(FATAL_ERROR
            "${invalid_case} did not fail as expected (${invalid_result})\n"
            "stdout: ${invalid_output}\nstderr: ${invalid_error}")
    endif()
endforeach()
