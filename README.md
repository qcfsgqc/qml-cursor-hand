# Cursor.Hand

Minimal Qt **6.10+** QML module that turns the mouse cursor into a hand (or any `Qt::CursorShape`) on **any** `Item` / Control — without stealing clicks.

URI: `Cursor.Hand`

## Why this works on every QML component

Two complementary APIs, both non-invasive:

| API | Use on | Clicks stolen? |
|---|---|---|
| **Attached** `CursorHand` | Any `Item`, Control, `Window` | No |
| **Child** `HandCursor` | Drop inside any component | No (`HoverHandler`) |

Attached mode:

1. Calls `QQuickItem::setCursor()` (window walks parents, so it still applies when children fill the item).
2. Installs a `HoverHandler` on the same item so Qt Quick Controls / nested mouse areas still show the hand.

`HoverHandler` does not grab the pointer, so `Button`, `MouseArea`, `TapHandler`, `Flickable`, etc. keep working.

## Usage

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
    HandCursor {}                     // child handler, same default
}

Rectangle {
    CursorHand.shape: Qt.OpenHandCursor
    // CursorHand.hovered is true while the pointer is over this item
}
```

Global busy / custom image cursor (C++ singleton, same type name):

```qml
CursorHand.setOverride(Qt.WaitCursor)
CursorHand.setPixmapOverride(Qt.resolvedUrl("cursor.png"), 8, 8)
CursorHand.restoreOverride()
```

### Attached properties

| Property | Type | Default |
|---|---|---|
| `enabled` | bool | `true` |
| `shape` | `Qt.CursorShape` | `Qt.PointingHandCursor` |
| `hovered` | bool (read-only) | `false` |

## Prebuilt modules (GitHub Actions)

Every successful build on `main` (and manual **Run workflow**) publishes zips to the **[Latest release](https://github.com/qcfsgqc/qml-cursor-hand/releases/latest)**. Pushing a version tag creates a named release instead.

| Artifact | Runner |
|---|---|
| `cursor-hand-linux-x64.zip` | Ubuntu 24.04 |
| `cursor-hand-linux-arm64.zip` | Ubuntu 24.04 ARM |
| `cursor-hand-windows-x64.zip` | Windows 2022 (MSVC) |
| `cursor-hand-macos-arm64.zip` | macOS 14 |
| `cursor-hand-macos-x64.zip` | macOS 13 |

Each zip contains `qml/Cursor/Hand/` (plugin + backing lib + `qmldir`). Point the engine at the `qml` folder:

```cpp
engine.addImportPath("/path/to/extract/qml");
```

```bash
# or
export QML_IMPORT_PATH=/path/to/extract/qml
```

## Build from source

Requires Qt 6.10 and CMake 3.16+.

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.x/gcc_64
cmake --build build
cmake --install build --prefix dist
```

Example app target: `cursorhand-example` (`-DCURSORHAND_BUILD_EXAMPLE=ON`, default).

### Use from another project

```cmake
add_subdirectory(qml-cursor-hand)

qt_add_qml_module(myapp
    URI MyApp
    QML_FILES Main.qml
    DEPENDENCIES Cursor.Hand
)

target_link_libraries(myapp PRIVATE Qt6::Quick cursorhand)
```

```qml
import Cursor.Hand
```

If you consume the CI zip instead of linking the backing lib, install/copy `qml/Cursor/Hand` onto `QML_IMPORT_PATH` as above.

## Layout

```
src/cursorhand.{h,cpp}   C++ attached + singleton
src/HandCursor.qml       HoverHandler drop-in
example/                 small demo window
.github/workflows/       multi-platform module builds
```

## License

MIT
