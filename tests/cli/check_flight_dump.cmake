foreach(required_variable IN ITEMS
        RIVER_RAID_EXECUTABLE
        FLIGHT_SCENE_TEST_EXECUTABLE
        OUTPUT_ROOT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "Missing required variable: ${required_variable}")
    endif()
endforeach()

if(NOT EXISTS "${RIVER_RAID_EXECUTABLE}")
    message(FATAL_ERROR "River Raid executable does not exist: ${RIVER_RAID_EXECUTABLE}")
endif()
if(NOT EXISTS "${FLIGHT_SCENE_TEST_EXECUTABLE}")
    message(FATAL_ERROR "Flight scene test executable does not exist: ${FLIGHT_SCENE_TEST_EXECUTABLE}")
endif()

file(MAKE_DIRECTORY "${OUTPUT_ROOT}")
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(generated_directory "${OUTPUT_ROOT}/flight_dump_regression_${suffix}")
if(EXISTS "${generated_directory}")
    message(FATAL_ERROR "Generated output directory already exists: ${generated_directory}")
endif()
file(MAKE_DIRECTORY "${generated_directory}")

set(actual_frame "${generated_directory}/actual.ppm")
set(expected_directory "${generated_directory}/expected")
execute_process(
    COMMAND "${RIVER_RAID_EXECUTABLE}" --dump-flight "${actual_frame}"
    RESULT_VARIABLE dump_result
    OUTPUT_VARIABLE dump_output
    ERROR_VARIABLE dump_error)
if(NOT dump_result EQUAL 0)
    message(FATAL_ERROR
        "--dump-flight failed (${dump_result})\nstdout: ${dump_output}\nstderr: ${dump_error}")
endif()

execute_process(
    COMMAND "${FLIGHT_SCENE_TEST_EXECUTABLE}" --capture-dir "${expected_directory}"
    RESULT_VARIABLE scene_result
    OUTPUT_VARIABLE scene_output
    ERROR_VARIABLE scene_error)
if(NOT scene_result EQUAL 0)
    message(FATAL_ERROR
        "flight_scene_test capture failed (${scene_result})\nstdout: ${scene_output}\nstderr: ${scene_error}")
endif()

set(expected_frame "${expected_directory}/startup_reveal.ppm")
if(NOT EXISTS "${actual_frame}")
    message(FATAL_ERROR "--dump-flight did not create ${actual_frame}")
endif()
if(NOT EXISTS "${expected_frame}")
    message(FATAL_ERROR "flight_scene_test did not create expected frame ${expected_frame}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${actual_frame}" "${expected_frame}"
    RESULT_VARIABLE compare_result
    OUTPUT_VARIABLE compare_output
    ERROR_VARIABLE compare_error)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR
        "--dump-flight differs from flight_scene_test startup reveal\n"
        "actual: ${actual_frame}\nexpected: ${expected_frame}\n"
        "stdout: ${compare_output}\nstderr: ${compare_error}")
endif()

message(STATUS "--dump-flight matches the startup reveal frame")
