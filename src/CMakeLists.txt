set(cursorhand_qml_args
    URI Cursor.Hand
    VERSION 1.0
    SOURCES
        cursorhand.h
        cursorhand.cpp
    QML_FILES
        HandCursor.qml
)

# Qt for WebAssembly (and static Qt) cannot dlopen a QML plugin.
if(EMSCRIPTEN OR (DEFINED QT6_IS_SHARED_LIBS_BUILD AND NOT QT6_IS_SHARED_LIBS_BUILD))
    list(APPEND cursorhand_qml_args STATIC)
endif()

qt_add_qml_module(cursorhand ${cursorhand_qml_args})

target_link_libraries(cursorhand
    PRIVATE
        Qt6::Quick
)

target_include_directories(cursorhand
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

qt_query_qml_module(cursorhand
    PLUGIN_TARGET module_plugin_target
    TARGET_PATH module_target_path
    QMLDIR module_qmldir
    TYPEINFO module_typeinfo
)

if(CURSORHAND_IS_TOPLEVEL)
    set(module_install_dir "qml/${module_target_path}")

    if(APPLE)
        set(_module_rpath "@loader_path")
    elseif(UNIX AND NOT EMSCRIPTEN)
        set(_module_rpath "$ORIGIN")
    endif()

    if(DEFINED _module_rpath)
        set_target_properties(cursorhand ${module_plugin_target} PROPERTIES
            INSTALL_RPATH "${_module_rpath}"
        )
    endif()

    install(TARGETS cursorhand ${module_plugin_target}
        LIBRARY DESTINATION "${module_install_dir}"
        RUNTIME DESTINATION "${module_install_dir}"
        ARCHIVE DESTINATION "${module_install_dir}"
    )

    install(FILES
        ${module_qmldir}
        ${module_typeinfo}
        DESTINATION "${module_install_dir}"
    )
endif()
