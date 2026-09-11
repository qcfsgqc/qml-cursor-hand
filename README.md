# Cursor.Hand

Minimal Qt **6.10+** QML module that turns the mouse cursor into a hand (or any `Qt::CursorShape`) on **any** `Item` / Control — without stealing clicks.

Works on desktop **and Qt for WebAssembly** (static link). URI: `Cursor.Hand`.

## Why this works on every QML component

| API | Use on | Clicks stolen? |
|---|---|---|
| **Attached** `CursorHand` | Any `Item`, Control, `Window` | No |
| **Child** `HandCursor` | Drop inside any component | No (`HoverHandler`) |

Attached mode calls `QQuickItem::setCursor()` and installs a `HoverHandler` on the same item. `HoverHandler` does not grab the pointer, so `Button`, `MouseArea`, `TapHandler`, `Flickable`, etc. keep working.

A `MouseArea` that fills a parent and only accepts the right button still sits on top for **cursor picking**. So does a `MouseArea` with `enabled: false`: Qt keeps `Item.enabled` true, so a full-window click guard would otherwise lock the window cursor to Arrow. `QQuickMouseArea` also calls `setCursor(Arrow)` in its constructor, so `hasCursor` is true even when `hoverEnabled` is false; `QQuickWindow::updateCursor()` then stops on that overlay and never sees a `HoverHandler` underneath. Empty filler `Item`s (toasts, layout shells) are skipped the same way — returning them would `unsetCursor()` the window and let Qt's own pick land on a higher-z Arrow `MouseArea`. `CursorHand.ensureWatch()` (called automatically from attached mode and from `HandCursor`) watches the window, clears those pass-through overlays' item cursors, and looks through them, so buttons, text fields, and drag `MouseArea` cursors underneath still show. `HoverHandler` is a `QObject` child, not a `QQuickItem`; the watch reads it off the item's `children()`.

On WASM, Qt maps `Qt::PointingHandCursor` to CSS `cursor: pointer` (and the other shapes to `grab` / `wait` / …).

## Use as a git submodule

```bash
git submodule add https://github.com/qcfsgqc/qml-cursor-hand.git third_party/qml-cursor-hand
git submodule update --init --recursive
```

Parent `CMakeLists.txt` (desktop **and** WASM — use the kit's `qt-cmake` for WASM):

```cmake
add_subdirectory(third_party/qml-cursor-hand)

qt_add_executable(myapp main.cpp)

qt_add_qml_module(myapp
    URI MyApp
    VERSION 1.0
    QML_FILES Main.qml
    DEPENDENCIES
        QtQuick
        Cursor.Hand
)

target_link_libraries(myapp PRIVATE Qt6::Quick)

# links cursorhand; on WASM/static Qt also runs qt_import_qml_plugins()
cursorhand_link(myapp)
```

```qml
import Cursor.Hand

Button {
    text: "Click"
    CursorHand.enabled: true
}
```

WASM configure (from the **parent** project, same Emscripten as your Qt 6.10 kit, typically 4.0.7):

```bash
source /path/to/emsdk_env.sh
/path/to/Qt/6.10.x/wasm_singlethread/bin/qt-cmake -S . -B build-wasm
cmake --build build-wasm
```

Do not drop a shared plugin into the browser — Qt WASM is statically linked. `add_subdirectory` + `cursorhand_link()` is the supported path.

## QML API

```qml
import QtQuick
import QtQuick.Controls
import Cursor.Hand

Button {
    text: "Click"
    CursorHand.enabled: true          // PointingHandCursor
}

Image {
    source: "icon.png"
    HandCursor {}
}

Rectangle {
    CursorHand.shape: Qt.OpenHandCursor
    // CursorHand.hovered is true while the pointer is over this item
}
```

Global override (same type name, singleton):

```qml
CursorHand.setOverride(Qt.WaitCursor)
CursorHand.setPixmapOverride(Qt.resolvedUrl("cursor.png"), 8, 8)  // prefer qrc on WASM
CursorHand.restoreOverride()
```

### Attached properties

| Property | Type | Default |
|---|---|---|
| `enabled` | bool | `true` |
| `shape` | `Qt.CursorShape` | `Qt.PointingHandCursor` |
| `hovered` | bool (read-only) | `false` |

## Standalone build

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.x/<kit>
cmake --build build
```

Example target: `cursorhand-example` (`-DCURSORHAND_BUILD_EXAMPLE=ON`, default only when this repo is the top-level project).

Install the shared module (qmldir + plugin) to `<prefix>/qml/Cursor/Hand`:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.x/<kit> -DCMAKE_INSTALL_PREFIX=/path/to/prefix -DCURSORHAND_BUILD_EXAMPLE=OFF
cmake --build build --config Release
cmake --install build --config Release
```

## PySide6

Do not static-link into the Python process. Build the **shared** plugin (above), then before `engine.load`:

```python
from cursorhand_pyside import register

register(engine, "/path/to/prefix/qml")  # directory that contains Cursor/Hand
```

`register` calls `QQmlEngine.addImportPath`. On Windows it also puts the PySide6 Qt DLLs on the search path and preloads `cursorhand.dll`. QML is unchanged: `import Cursor.Hand`.

The calculator / C++ host still uses `add_subdirectory` + `CURSORHAND_FORCE_STATIC` + `cursorhand_link()`; that path does not load this plugin.

## License

MIT
