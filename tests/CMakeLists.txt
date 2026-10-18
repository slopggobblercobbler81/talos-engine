# Tests mirror engine/ and need no window or GPU context.
# Run with: ctest --test-dir build --output-on-failure

function(talos_add_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

talos_add_test(test_physics2d physics/test_physics2d.cpp talos_physics)
talos_add_test(test_scene_serializer scene/test_scene_serializer.cpp talos_scene)
