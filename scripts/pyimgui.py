"""
Ergonomic Python facade for PyImGui.

Design:
- `PyImGui` remains the raw parity layer.
- This module adds a small ad-hoc interface for the patterns Python users
  actually need: scope management, lightweight state binding, and argument
  normalization.
- Anything not wrapped here falls through to the raw module via `__getattr__`.
"""

from dataclasses import dataclass
from typing import Any, MutableMapping, Optional

import PyImGui as _raw

raw = _raw

__all__ = [
    "raw",
    "SortDirection",
    "WindowFlags",
    "ChildFlags",
    "InputTextFlags",
    "TreeNodeFlags",
    "PopupFlags",
    "SelectableFlags",
    "ComboFlags",
    "TabBarFlags",
    "TabItemFlags",
    "FocusedFlags",
    "HoveredFlags",
    "DockNodeFlags",
    "Dir",
    "DragDropFlags",
    "SliderFlags",
    "ButtonFlags",
    "TableFlags",
    "TableColumnFlags",
    "TableRowFlags",
    "DrawFlags",
    "ColorEditFlags",
    "ImGuiCond",
    "MouseButton",
    "MouseCursor",
    "ImGuiCol",
    "ImGuiStyleVar",
    "TableColumnSortSpecs",
    "TableSortSpecs",
    "ImGuiStyle",
    "ImGuiIO",
    "StyleConfig",
    "Vec2",
    "Vec4",
    "ScopeToken",
    "State",
    "bind",
    "begin",
    "begin_with_close",
    "end",
    "begin_child",
    "end_child",
    "text",
    "bullet_text",
    "separator_text",
    "same_line",
    "checkbox",
    "radio_button",
    "button",
    "text_colored",
    "progress_bar",
    "selectable",
    "menu_item",
    "color_button",
    "color_picker4",
    "get_io",
    "get_content_region_avail",
    "open_popup",
    "set_next_window_pos",
    "set_next_window_size",
    "set_next_window_content_size",
    "set_window_pos",
    "set_window_size",
    "dummy",
    "v_slider_int",
    "push_clip_rect",
    "show_tooltip",
    "window",
    "child",
    "group",
    "disabled",
    "tab_bar",
    "tab_item",
    "combo_box",
    "table",
    "menu_bar",
    "menu",
    "popup",
    "popup_modal",
    "popup_context_window",
    "tooltip",
    "id_scope",
    "style_color",
    "style_var",
    "item_width",
    "text_wrap_pos",
    "font",
    "style_font",
    "font_size",
    "clip_rect",
]

# Re-export the public enum/type surface that scripts commonly consume.
SortDirection = _raw.SortDirection
WindowFlags = _raw.WindowFlags
ChildFlags = _raw.ChildFlags
InputTextFlags = _raw.InputTextFlags
TreeNodeFlags = _raw.TreeNodeFlags
PopupFlags = _raw.PopupFlags
SelectableFlags = _raw.SelectableFlags
ComboFlags = _raw.ComboFlags
TabBarFlags = _raw.TabBarFlags
TabItemFlags = _raw.TabItemFlags
FocusedFlags = _raw.FocusedFlags
HoveredFlags = _raw.HoveredFlags
DockNodeFlags = _raw.DockNodeFlags
Dir = _raw.Dir
DragDropFlags = _raw.DragDropFlags
SliderFlags = _raw.SliderFlags
ButtonFlags = _raw.ButtonFlags
TableFlags = _raw.TableFlags
TableColumnFlags = _raw.TableColumnFlags
TableRowFlags = _raw.TableRowFlags
DrawFlags = _raw.DrawFlags
ColorEditFlags = _raw.ColorEditFlags
ImGuiCond = _raw.ImGuiCond
MouseButton = _raw.MouseButton
MouseCursor = _raw.MouseCursor
ImGuiCol = _raw.ImGuiCol
ImGuiStyleVar = _raw.ImGuiStyleVar
TableColumnSortSpecs = _raw.TableColumnSortSpecs
TableSortSpecs = _raw.TableSortSpecs
ImGuiStyle = _raw.ImGuiStyle
ImGuiIO = _raw.ImGuiIO
StyleConfig = _raw.StyleConfig
Vec2 = _raw.Vec2
Vec4 = _raw.Vec4


def _vec2(value: Any = (0.0, 0.0), y: Optional[float] = None) -> Vec2:
    if isinstance(value, Vec2):
        return value
    if y is not None:
        return Vec2(float(value), float(y))
    if isinstance(value, (tuple, list)):
        return Vec2(float(value[0]), float(value[1]))
    return Vec2(float(value), float(value))


def _vec4(value: Any) -> Vec4:
    if isinstance(value, Vec4):
        return value
    return Vec4(float(value[0]), float(value[1]), float(value[2]), float(value[3]))


@dataclass
class ScopeToken:
    visible: bool = True
    open: Optional[bool] = None

    def __bool__(self) -> bool:
        return self.visible


class _AlwaysEndScope:
    def __init__(self, begin_fn, end_fn):
        self._begin_fn = begin_fn
        self._end_fn = end_fn
        self._token = ScopeToken()

    def __enter__(self) -> ScopeToken:
        self._token = self._begin_fn()
        return self._token

    def __exit__(self, exc_type, exc, tb) -> bool:
        self._end_fn()
        return False


class _ConditionalEndScope:
    def __init__(self, begin_fn, end_fn):
        self._begin_fn = begin_fn
        self._end_fn = end_fn
        self._token = ScopeToken(False)

    def __enter__(self) -> ScopeToken:
        self._token = self._begin_fn()
        return self._token

    def __exit__(self, exc_type, exc, tb) -> bool:
        if self._token.visible:
            self._end_fn()
        return False


class State:
    """Dictionary-backed widget state binder."""

    def __init__(self, store: MutableMapping[str, Any]):
        self.store = store

    def get(self, key: str, default: Any = None) -> Any:
        return self.store.get(key, default)

    def set(self, key: str, value: Any) -> Any:
        self.store[key] = value
        return value

    def __getitem__(self, key: str) -> Any:
        return self.store[key]

    def __setitem__(self, key: str, value: Any) -> None:
        self.store[key] = value

    def checkbox(self, key: str, label: str) -> bool:
        return self.set(key, checkbox(label, bool(self.store.get(key, False))))

    def radio(self, key: str, label: str, button_index: int) -> int:
        value = int(self.store.get(key, 0))
        return self.set(key, radio_button(label, value, button_index))

    def slider_float(self, key: str, label: str, v_min: float, v_max: float, fmt: str = "%.3f", flags: int = 0) -> float:
        value = float(self.store.get(key, 0.0))
        return self.set(key, _raw.slider_float(label, value, v_min, v_max, fmt, flags))

    def slider_int(self, key: str, label: str, v_min: int, v_max: int, fmt: str = "%d", flags: int = 0) -> int:
        value = int(self.store.get(key, 0))
        return self.set(key, _raw.slider_int(label, value, v_min, v_max, fmt, flags))

    def drag_float(self, key: str, label: str, speed: float = 1.0, v_min: float = 0.0, v_max: float = 0.0, fmt: str = "%.3f", flags: int = 0) -> float:
        value = float(self.store.get(key, 0.0))
        return self.set(key, _raw.drag_float(label, value, speed, v_min, v_max, fmt, flags))

    def drag_int(self, key: str, label: str, speed: float = 1.0, v_min: int = 0, v_max: int = 0, fmt: str = "%d", flags: int = 0) -> int:
        value = int(self.store.get(key, 0))
        return self.set(key, _raw.drag_int(label, value, speed, v_min, v_max, fmt, flags))

    def input_text(self, key: str, label: str, flags: int = 0) -> str:
        return self.set(key, _raw.input_text(label, str(self.store.get(key, "")), flags))

    def input_text_with_hint(self, key: str, label: str, hint: str, flags: int = 0) -> str:
        return self.set(key, _raw.input_text_with_hint(label, hint, str(self.store.get(key, "")), flags))

    def input_text_multiline(self, key: str, label: str, size=(0.0, 0.0), flags: int = 0) -> str:
        return self.set(key, _raw.input_text_multiline(label, str(self.store.get(key, "")), _vec2(size), flags))

    def input_int(self, key: str, label: str, step: int = 1, step_fast: int = 100, flags: int = 0) -> int:
        value = int(self.store.get(key, 0))
        return self.set(key, _raw.input_int(label, value, step, step_fast, flags))

    def input_float(self, key: str, label: str, step: float = 0.0, step_fast: float = 0.0, fmt: str = "%.3f", flags: int = 0) -> float:
        value = float(self.store.get(key, 0.0))
        return self.set(key, _raw.input_float(label, value, step, step_fast, fmt, flags))

    def combo(self, key: str, label: str, items) -> int:
        value = int(self.store.get(key, 0))
        return self.set(key, _raw.combo(label, value, list(items)))

    def selectable(self, key: str, label: str, flags: int = 0, size=(0.0, 0.0)) -> bool:
        value = bool(self.store.get(key, False))
        return self.set(key, _raw.selectable(label, value, flags, _vec2(size)))

    def color_edit4(self, key: str, label: str, flags: int = 0):
        value = self.store.get(key, [1.0, 1.0, 1.0, 1.0])
        return self.set(key, _raw.color_edit4(label, value, flags))

    def menu_item(self, key: str, label: str, shortcut: str = "", enabled: bool = True) -> bool:
        value = bool(self.store.get(key, False))
        return self.set(key, _raw.menu_item(label, shortcut, value, enabled))


def bind(store: MutableMapping[str, Any]) -> State:
    return State(store)


def begin(name: str, flags: int = 0) -> bool:
    return _raw.begin(name, None, flags)


def begin_with_close(name: str, p_open: bool, flags: int = 0):
    return _raw.begin_with_close(name, p_open, flags)


def end():
    _raw.end()


def begin_child(id_: str, size=(0.0, 0.0), child_flags=0, window_flags=0) -> bool:
    return _raw.begin_child(id_, _vec2(size), child_flags, window_flags)


def end_child():
    _raw.end_child()


def text(value: str):
    _raw.text(value)


def bullet_text(value: str):
    _raw.bullet_text(value)


def separator_text(label: str):
    _raw.separator_text(label)


def same_line(offset_from_start_x: float = 0.0, spacing: float = -1.0):
    _raw.same_line(offset_from_start_x, spacing)


def checkbox(label: str, value: bool) -> bool:
    return _raw.checkbox(label, value)


def radio_button(label: str, value: int, button_index: int) -> int:
    return _raw.radio_button(label, value, button_index)


def button(label: str, width: float = 0.0, height: float = 0.0) -> bool:
    return _raw.button(label, width, height)


def text_colored(color, text: str):
    _raw.text_colored(_vec4(color), text)


def progress_bar(fraction: float, width: float = -1.0, height: float = 0.0, overlay: str = ""):
    _raw.progress_bar(fraction, _vec2(width, height), overlay)


def selectable(label: str, selected: bool, flags: int = 0, width: float = 0.0, height: float = 0.0) -> bool:
    return _raw.selectable(label, selected, flags, _vec2(width, height))


def menu_item(label: str, shortcut: str = "", selected: bool = False, enabled: bool = True) -> bool:
    return _raw.menu_item(label, shortcut, selected, enabled)


def color_button(label: str, color, flags: int = 0, width: float = 0.0, height: float = 0.0) -> bool:
    return _raw.color_button(label, _vec4(color), flags, _vec2(width, height))


def color_picker4(label: str, color, flags: int = 0):
    return _raw.color_picker4(label, color, flags)


def get_io():
    return _raw.get_io()


def get_content_region_avail():
    return _raw.get_content_region_avail()


def open_popup(str_id: str, popup_flags: int = 0):
    _raw.open_popup(str_id, popup_flags)


def set_next_window_pos(x: float, y: float, cond: int = 0):
    _raw.set_next_window_pos(_vec2(x, y), cond)


def set_next_window_size(width: float, height: float, cond: int = 0):
    _raw.set_next_window_size(_vec2(width, height), cond)


def set_next_window_content_size(width: float, height: float):
    _raw.set_next_window_content_size(_vec2(width, height))


def set_window_pos(x: float, y: float, cond: int = 0):
    _raw.set_window_pos(_vec2(x, y), cond)


def set_window_size(width: float, height: float, cond: int = 0):
    _raw.set_window_size(_vec2(width, height), cond)


def dummy(width: float, height: float):
    _raw.dummy(_vec2(width, height))


def v_slider_int(label: str, size, value: int, v_min: int, v_max: int, fmt: str = "%d", flags: int = 0) -> int:
    return _raw.v_slider_int(label, _vec2(size), value, v_min, v_max, fmt, flags)


def push_clip_rect(x: float, y: float, width: float, height: float, intersect: bool = True):
    _raw.push_clip_rect(_vec2(x, y), _vec2(x + width, y + height), intersect)


def show_tooltip(text: str):
    if _raw.is_item_hovered():
        _raw.begin_tooltip()
        _raw.text(text)
        _raw.end_tooltip()


def window(name: str, *, flags: int = 0, open: Optional[bool] = None):
    if open is None:
        return _AlwaysEndScope(lambda: ScopeToken(begin(name, flags)), end)
    return _AlwaysEndScope(lambda: ScopeToken(*begin_with_close(name, open, flags)), end)


def child(id_: str, *, size=(0.0, 0.0), child_flags: int = 0, window_flags: int = 0):
    return _AlwaysEndScope(lambda: ScopeToken(begin_child(id_, size, child_flags, window_flags)), end_child)


def group():
    return _AlwaysEndScope(lambda: (_raw.begin_group(), ScopeToken(True))[1], _raw.end_group)


def disabled(disabled_value: bool = True):
    return _AlwaysEndScope(lambda: (_raw.begin_disabled(disabled_value), ScopeToken(True))[1], _raw.end_disabled)


def tab_bar(str_id: str, flags: int = 0):
    return _ConditionalEndScope(lambda: ScopeToken(_raw.begin_tab_bar(str_id, flags)), _raw.end_tab_bar)


def tab_item(label: str, *, flags: int = 0, open: Optional[bool] = None):
    if open is None:
        return _ConditionalEndScope(lambda: ScopeToken(_raw.begin_tab_item(label, None, flags)), _raw.end_tab_item)
    return _ConditionalEndScope(lambda: ScopeToken(*_raw.begin_tab_item_closable(label, open, flags)), _raw.end_tab_item)


def combo_box(label: str, preview_value: str, flags: int = 0):
    return _ConditionalEndScope(lambda: ScopeToken(_raw.begin_combo(label, preview_value, flags)), _raw.end_combo)


def table(str_id: str, columns: int, flags: int = 0, outer_size=(0.0, 0.0), inner_width: float = 0.0):
    return _ConditionalEndScope(
        lambda: ScopeToken(_raw.begin_table(str_id, columns, flags, _vec2(outer_size), inner_width)),
        _raw.end_table,
    )


def menu_bar():
    return _ConditionalEndScope(lambda: ScopeToken(_raw.begin_menu_bar()), _raw.end_menu_bar)


def menu(label: str, enabled: bool = True):
    return _ConditionalEndScope(lambda: ScopeToken(_raw.begin_menu(label, enabled)), _raw.end_menu)


def popup(str_id: str, flags: int = 0):
    return _ConditionalEndScope(lambda: ScopeToken(_raw.begin_popup(str_id, flags)), _raw.end_popup)


def popup_modal(name: str, open: Optional[bool] = None, flags: int = 0):
    if open is None:
        return _ConditionalEndScope(lambda: ScopeToken(_raw.begin_popup_modal(name, None, flags)), _raw.end_popup)
    return _ConditionalEndScope(lambda: ScopeToken(*_raw.begin_popup_modal(name, open, flags)), _raw.end_popup)


def popup_context_window(str_id: Optional[str] = None, popup_flags: int = 1):
    return _ConditionalEndScope(lambda: ScopeToken(_raw.begin_popup_context_window(str_id, popup_flags)), _raw.end_popup)


def tooltip():
    return _ConditionalEndScope(lambda: ScopeToken(_raw.begin_tooltip() or True), _raw.end_tooltip)


def push_id(value):
    if isinstance(value, int):
        _raw.push_id_int(value)
    else:
        _raw.push_id(str(value))


class _PushPopScope:
    def __init__(self, push_fn, pop_fn):
        self._push_fn = push_fn
        self._pop_fn = pop_fn

    def __enter__(self):
        self._push_fn()
        return ScopeToken(True)

    def __exit__(self, exc_type, exc, tb) -> bool:
        self._pop_fn()
        return False


def id_scope(value):
    return _PushPopScope(lambda: push_id(value), _raw.pop_id)


def style_color(idx: int, color):
    return _PushPopScope(lambda: _raw.push_style_color(idx, _vec4(color)), _raw.pop_style_color)


def style_var(idx: int, value, y: Optional[float] = None):
    if y is None and isinstance(value, (tuple, list, Vec2)):
        return _PushPopScope(lambda: _raw.push_style_var_vec2(idx, _vec2(value)), _raw.pop_style_var)
    return _PushPopScope(lambda: _raw.push_style_var(idx, float(value)), _raw.pop_style_var)


def item_width(width: float):
    return _PushPopScope(lambda: _raw.push_item_width(width), _raw.pop_item_width)


def text_wrap_pos(wrap_local_pos_x: float = 0.0):
    return _PushPopScope(lambda: _raw.push_text_wrap_pos(wrap_local_pos_x), _raw.pop_text_wrap_pos)


def font(font_index: int = 0):
    return _PushPopScope(lambda: _raw.push_font(font_index), _raw.pop_font)


def style_font(style: int, size: float = 0.0):
    """Render a style (0=Regular,1=Bold,2=Italic,3=BoldItalic) at any pixel size
    under the ImGui 1.92 dynamic-font model. size <= 0 keeps the current size."""
    return _PushPopScope(lambda: _raw.push_style_font(style, size), _raw.pop_font)


def font_size(size: float):
    """Re-scale the current font to an absolute base size (before global factors)."""
    return _PushPopScope(lambda: _raw.push_font_size(size), _raw.pop_font)


def clip_rect(x: float, y: float, width: float, height: float, intersect: bool = True):
    return _PushPopScope(lambda: push_clip_rect(x, y, width, height, intersect), _raw.pop_clip_rect)


def __getattr__(name: str):
    return getattr(_raw, name)


def __dir__():
    return sorted(set(globals()) | set(dir(_raw)))
