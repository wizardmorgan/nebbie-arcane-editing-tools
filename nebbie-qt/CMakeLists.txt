find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(NEBBIE_APP_ICON_MAC "${CMAKE_CURRENT_SOURCE_DIR}/icons/nebbieedit.icns")
set(NEBBIE_APP_ICON_WIN "${CMAKE_CURRENT_SOURCE_DIR}/icons/nebbieedit.ico")

add_executable(nebbieedit
    main.cpp
    main_window.cpp
    main_window.hpp
    app_config.cpp
    app_config.hpp
    app_theme.cpp
    app_theme.hpp
    application_log.cpp
    application_log.hpp
    coordinator_client.cpp
    coordinator_client.hpp
    release_update_checker.cpp
    release_update_checker.hpp
    release_update_util.cpp
    release_update_util.hpp
    flag_group_widget.cpp
    flag_group_widget.hpp
    mob_editor_widget.cpp
    mob_editor_widget.hpp
    room_editor_widget.cpp
    room_editor_widget.hpp
    obj_editor_widget.cpp
    obj_editor_widget.hpp
    zone_editor_widget.cpp
    zone_editor_widget.hpp
    world_zone_map_widget.cpp
    world_zone_map_widget.hpp
    world_data_editor_widget.cpp
    world_data_editor_widget.hpp
    zone_map_widget.cpp
    zone_map_widget.hpp
    mud_color_widgets.cpp
    mud_color_widgets.hpp
    mud_color_list_delegate.cpp
    mud_color_list_delegate.hpp
    mud_color_dialogs.cpp
    mud_color_dialogs.hpp
    system_config_dialog.cpp
    system_config_dialog.hpp
    validation_report_ui.cpp
    validation_report_ui.hpp
    nebbieedit.qrc
)

if(APPLE AND EXISTS "${NEBBIE_APP_ICON_MAC}")
    set_source_files_properties("${NEBBIE_APP_ICON_MAC}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(nebbieedit PRIVATE "${NEBBIE_APP_ICON_MAC}")
endif()

if(WIN32 AND EXISTS "${NEBBIE_APP_ICON_WIN}")
    target_sources(nebbieedit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/nebbieedit.rc")
endif()

target_link_libraries(nebbieedit PRIVATE Qt6::Widgets Qt6::Network nebbie-core)
target_compile_features(nebbieedit PRIVATE cxx_std_17)
target_include_directories(nebbieedit PRIVATE "${CMAKE_BINARY_DIR}/generated")

if(MSVC)
    target_compile_options(nebbieedit PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/msvc_qt_compat.hpp")
endif()

if(WIN32)
    set_target_properties(nebbieedit PROPERTIES WIN32_EXECUTABLE TRUE OUTPUT_NAME izanagi)
endif()

if(APPLE AND NEBBIE_MACOS_BUNDLE)
    set_target_properties(nebbieedit PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.nebbie.editor"
        MACOSX_BUNDLE_BUNDLE_NAME "Izanagi"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
    if(EXISTS "${NEBBIE_APP_ICON_MAC}")
        set_target_properties(nebbieedit PROPERTIES MACOSX_BUNDLE_ICON_FILE nebbieedit)
    endif()
endif()

include(GNUInstallDirs)

if(NOT (APPLE AND NEBBIE_MACOS_BUNDLE))
    install(TARGETS nebbieedit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(NOT WIN32)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/nebbieedit.desktop"
                DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        foreach(_icon_size 16 22 24 32 48 64 128 256 512 1024)
            set(_icon_path "${CMAKE_CURRENT_SOURCE_DIR}/icons/hicolor/${_icon_size}x${_icon_size}/apps/nebbieedit.png")
            if(EXISTS "${_icon_path}")
                install(FILES "${_icon_path}"
                        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_icon_size}x${_icon_size}/apps")
                install(FILES "${_icon_path}"
                        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_icon_size}x${_icon_size}/apps"
                        RENAME izanagi.png)
            endif()
        endforeach()
    endif()
endif()

if(APPLE AND NEBBIE_MACOS_BUNDLE)
    install(TARGETS nebbieedit BUNDLE DESTINATION "/Applications")
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin" "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET nebbieedit POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:nebbieedit>"
            COMMENT "Deploying Qt runtime for nebbieedit"
        )
    endif()
endif()
