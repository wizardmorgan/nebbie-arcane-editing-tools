add_executable(nebbie-lib-extensions-tests test_lib_extensions.cpp)
target_link_libraries(nebbie-lib-extensions-tests PRIVATE nebbie-core)
add_test(NAME lib-extensions-fixtures
         COMMAND nebbie-lib-extensions-tests ${CMAKE_SOURCE_DIR}/tests/fixtures)

add_executable(nebbie-zone-file-eof-tests test_zone_file_eof.cpp)
target_link_libraries(nebbie-zone-file-eof-tests PRIVATE nebbie-core)
add_test(NAME zone-file-eof
         COMMAND nebbie-zone-file-eof-tests ${CMAKE_SOURCE_DIR}/tests/fixtures)

find_package(Qt6 QUIET COMPONENTS Core)
if(Qt6_FOUND)
    add_executable(nebbie-release-update-tests test_release_update.cpp
                   ${CMAKE_SOURCE_DIR}/nebbie-qt/release_update_util.cpp)
    target_include_directories(nebbie-release-update-tests PRIVATE
        "${CMAKE_SOURCE_DIR}/nebbie-qt"
        "${CMAKE_BINARY_DIR}/generated")
    target_link_libraries(nebbie-release-update-tests PRIVATE Qt6::Core)
    if(MSVC)
        target_compile_options(nebbie-release-update-tests PRIVATE
            "/FI${CMAKE_SOURCE_DIR}/nebbie-qt/msvc_qt_compat.hpp")
    endif()
    add_test(NAME release-update-tests COMMAND nebbie-release-update-tests)
endif()

add_executable(nebbie-core-tests test_io.cpp)
target_link_libraries(nebbie-core-tests PRIVATE nebbie-core)

add_executable(nebbie-text-lines-tests test_text_lines.cpp)
target_link_libraries(nebbie-text-lines-tests PRIVATE nebbie-core)
add_test(NAME text-lines-tests COMMAND nebbie-text-lines-tests)

add_executable(nebbie-wld-teleport-tests test_wld_teleport.cpp)
target_link_libraries(nebbie-wld-teleport-tests PRIVATE nebbie-core)
add_test(NAME wld-teleport-roundtrip COMMAND nebbie-wld-teleport-tests)

add_executable(nebbie-validate-tests test_validate.cpp)
target_link_libraries(nebbie-validate-tests PRIVATE nebbie-core)

add_executable(nebbie-phase4-tests test_phase4.cpp)
target_link_libraries(nebbie-phase4-tests PRIVATE nebbie-core)

add_executable(nebbie-mvp-tests test_mvp.cpp)
target_link_libraries(nebbie-mvp-tests PRIVATE nebbie-core)

add_executable(nebbie-create-tests test_create.cpp)
target_link_libraries(nebbie-create-tests PRIVATE nebbie-core)

add_test(NAME load-fixtures COMMAND nebbie-core-tests ${CMAKE_SOURCE_DIR}/tests/fixtures)
add_test(NAME validate-fixtures COMMAND nebbie-validate-tests ${CMAKE_SOURCE_DIR}/tests/fixtures)

add_test(NAME mvp-edit-workflow
         COMMAND nebbie-mvp-tests ${CMAKE_SOURCE_DIR}/tests/fixtures
                                  ${CMAKE_BINARY_DIR}/mvp-work-lib)

add_test(NAME create-edit-workflow
         COMMAND nebbie-create-tests ${CMAKE_SOURCE_DIR}/tests/fixtures
                                       ${CMAKE_BINARY_DIR}/create-work-lib)

add_executable(nebbie-reset-tests test_reset.cpp)
target_link_libraries(nebbie-reset-tests PRIVATE nebbie-core)

add_executable(nebbie-session-tests test_session.cpp)
target_link_libraries(nebbie-session-tests PRIVATE nebbie-core)

add_test(NAME session-workflow COMMAND nebbie-session-tests ${CMAKE_SOURCE_DIR}/tests/fixtures)

add_test(NAME zone-reset-workflow
         COMMAND nebbie-reset-tests ${CMAKE_SOURCE_DIR}/tests/fixtures
                                    ${CMAKE_BINARY_DIR}/reset-work-lib)

add_executable(nebbie-world-index-tests test_world_index.cpp)
target_link_libraries(nebbie-world-index-tests PRIVATE nebbie-core)
add_test(NAME world-index-fixtures
         COMMAND nebbie-world-index-tests ${CMAKE_SOURCE_DIR}/tests/fixtures)

add_executable(nebbie-overlay-tests test_overlay.cpp)
target_link_libraries(nebbie-overlay-tests PRIVATE nebbie-core)
add_test(NAME overlay-fixtures
         COMMAND nebbie-overlay-tests ${CMAKE_SOURCE_DIR}/tests/fixtures)

add_executable(nebbie-exit-sync-tests test_exit_sync.cpp)
target_link_libraries(nebbie-exit-sync-tests PRIVATE nebbie-core)
add_test(NAME exit-sync-workflow COMMAND nebbie-exit-sync-tests)

add_executable(nebbie-zone-partition-tests test_zone_partition.cpp)
target_link_libraries(nebbie-zone-partition-tests PRIVATE nebbie-core)
add_test(NAME zone-partition-fixtures
         COMMAND nebbie-zone-partition-tests ${CMAKE_SOURCE_DIR}/tests/fixtures)

add_executable(nebbie-system-field-config-tests test_system_field_config.cpp)
target_link_libraries(nebbie-system-field-config-tests PRIVATE nebbie-core)
add_test(NAME system-field-config-tests COMMAND nebbie-system-field-config-tests)

add_executable(nebbie-nebbietest-mob-tests test_nebbietest_mob.cpp)
target_link_libraries(nebbie-nebbietest-mob-tests PRIVATE nebbie-core)

if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/nebbietest/mudroot/lib/myst.mob")
    set(NEBBIETEST_MOB "${CMAKE_SOURCE_DIR}/vendor/nebbietest/mudroot/lib/myst.mob")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/vendor/nebbietest/myst.mob")
    set(NEBBIETEST_MOB "${CMAKE_SOURCE_DIR}/vendor/nebbietest/myst.mob")
endif()
if(NEBBIETEST_MOB)
    add_test(NAME nebbietest-mob-load COMMAND nebbie-nebbietest-mob-tests ${NEBBIETEST_MOB})
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/nebbietest/mudroot/lib/myst.mob")
    add_test(NAME nebbietest-lib-load COMMAND nebbie-nebbietest-mob-tests
             --lib ${CMAKE_SOURCE_DIR}/vendor/nebbietest/mudroot/lib)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/nebbietest/mudroot/lib/myst.act")
    add_test(NAME nebbietest-phase4 COMMAND nebbie-phase4-tests
             ${CMAKE_SOURCE_DIR}/vendor/nebbietest/mudroot/lib)
endif()
