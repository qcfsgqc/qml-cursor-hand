qt_add_qml_module(cursorhand
    URI Cursor.Hand
    VERSION 1.0
    SOURCES
        cursorhand.h
        cursorhand.cpp
    QML_FILES
        HandCursor.qml
)

target_link_libraries(cursorhand
    PRIVATE
        Qt6::Quick
)

target_include_directories(cursorhand
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
