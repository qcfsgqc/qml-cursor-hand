find_package(Qt6 6.10 REQUIRED COMPONENTS Quick QuickControls2)

qt_add_executable(cursorhand-example
    main.cpp
)

qt_add_qml_module(cursorhand-example
    URI CursorHandExample
    VERSION 1.0
    QML_FILES
        Main.qml
    DEPENDENCIES
        Cursor.Hand
)

target_link_libraries(cursorhand-example
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        cursorhand
)
