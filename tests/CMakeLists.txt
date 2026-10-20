# Tests mirror engine/ and need no window or GPU context.
# Run with: ctest --test-dir build --output-on-failure

function(talos_add_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

talos_add_test(test_logger core/test_logger.cpp talos_core)
talos_add_test(test_input_map core/test_input_map.cpp talos_core)
talos_add_test(test_physics2d physics/test_physics2d.cpp talos_physics)
talos_add_test(test_audio_system audio/test_audio_system.cpp talos_audio)
talos_add_test(test_scene_serializer scene/test_scene_serializer.cpp talos_scene)
talos_add_test(test_project_serializer scene/test_project_serializer.cpp talos_scene)
talos_add_test(test_asset_manager scene/test_asset_manager.cpp talos_scene)
