"""Load the Cursor.Hand shared QML plugin into a PySide6 QQmlEngine.

Build this repo as a *shared* QML module (do not set CURSORHAND_FORCE_STATIC),
install or copy the module so ``qml_root/Cursor/Hand/qmldir`` exists, then::

    from cursorhand_pyside import register
    register(engine, qml_root)

``qml_root`` is the import root (the directory that contains ``Cursor/Hand``),
not the module directory itself. Override the default with ``CURSORHAND_QML_ROOT``.

On Windows the plugin is typically linked against the official Qt SDK; at runtime
it must resolve Qt DLLs from the PySide6 install. This helper puts that directory
on the DLL search path and preloads ``cursorhand.dll`` next to the plugin.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

_DLL_DIR_HANDLES: list = []
_WIN_DLL_PREPARED = False


def module_dir(qml_root: str | os.PathLike[str] | None = None) -> Path:
    """Return ``<qml_root>/Cursor/Hand``."""
    return Path(resolve_qml_root(qml_root)) / "Cursor" / "Hand"


def resolve_qml_root(qml_root: str | os.PathLike[str] | None = None) -> Path:
    if qml_root is not None:
        return Path(qml_root)
    env = os.environ.get("CURSORHAND_QML_ROOT")
    if env:
        return Path(env)
    here = Path(__file__).resolve().parent
    for candidate in (here / "qml", here.parent / "qml"):
        if (candidate / "Cursor" / "Hand" / "qmldir").is_file():
            return candidate
    raise FileNotFoundError(
        "Cursor.Hand QML module not found. Pass qml_root (directory containing "
        "Cursor/Hand), set CURSORHAND_QML_ROOT, or install the shared plugin."
    )


def register(engine, qml_root: str | os.PathLike[str] | None = None, *, missing_ok: bool = False):
    """``engine.addImportPath(qml_root)`` so QML can ``import Cursor.Hand``.

    Returns the resolved import root, or ``None`` when ``missing_ok`` and the
    module is not installed.
    """
    try:
        root = resolve_qml_root(qml_root)
    except FileNotFoundError:
        if missing_ok:
            return None
        raise

    qmldir = root / "Cursor" / "Hand" / "qmldir"
    if not qmldir.is_file():
        if missing_ok:
            return None
        raise FileNotFoundError(
            f"Cursor.Hand QML module not found at {qmldir}. "
            "Build the shared plugin (not STATIC) and pass qml_root."
        )

    engine.addImportPath(str(root))
    if sys.platform == "win32":
        _prepare_windows_dlls(root / "Cursor" / "Hand")
    return root


def _prepare_windows_dlls(module_dir_path: Path) -> None:
    global _WIN_DLL_PREPARED
    import ctypes

    try:
        import PySide6
    except ImportError:
        return

    pyside_dir = Path(PySide6.__path__[0])
    if pyside_dir.is_dir():
        _DLL_DIR_HANDLES.append(os.add_dll_directory(str(pyside_dir)))
    if module_dir_path.is_dir():
        _DLL_DIR_HANDLES.append(os.add_dll_directory(str(module_dir_path)))

    if _WIN_DLL_PREPARED:
        return
    runtime = module_dir_path / "cursorhand.dll"
    plugin = module_dir_path / "cursorhandplugin.dll"
    if not runtime.is_file():
        return
    try:
        ctypes.CDLL(str(runtime))
    except OSError as exc:
        import warnings

        warnings.warn(
            f"Cursor.Hand: failed to preload {runtime}: {exc}",
            RuntimeWarning,
            stacklevel=2,
        )
        return
    if plugin.is_file():
        try:
            ctypes.CDLL(str(plugin))
        except OSError as exc:
            import warnings

            warnings.warn(
                f"Cursor.Hand: failed to preload {plugin}: {exc}",
                RuntimeWarning,
                stacklevel=2,
            )
    _WIN_DLL_PREPARED = True
