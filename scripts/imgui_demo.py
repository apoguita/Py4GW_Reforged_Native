import pyimgui as imgui

STATE = {
    'checkbox': False,
    'radio': 0,
    'slider_float': 0.5,
    'slider_int': 50,
    'drag_float': 0.0,
    'input_text': 'hello',
    'input_int': 42,
    'input_float': 3.14,
    'hint': '',
    'multiline': 'line 1\nline 2',
    'combo': 0,
    'selectable': False,
    'color': [0.3, 0.6, 0.9, 1.0],
    'progress': 0.0,
    'anim_on': False,
    'show_metrics': False,
    'overlay': True,
}
UI = imgui.bind(STATE)

_addons = {'browser': None, 'selected_file': '(none)', 'mem': None, 'mem_editor': None, 'hotkeys': None}


def draw():
    if hasattr(imgui, 'anim'):
        imgui.anim.update_begin_frame()

    if STATE['overlay']:
        _overlay()

    with imgui.window('PyImGui - Feature Showcase', flags=imgui.WindowFlags.MenuBar) as win:
        if not win:
            return

        _menu_bar()
        imgui.text_wrapped('Each section below maps to one feature area of the bindings. Expand a header to see it.')
        UI.checkbox('overlay', 'Draw foreground overlay')
        imgui.separator()

        if imgui.collapsing_header('13. Addons'):
            _section_addons()

    if STATE['show_metrics']:
        imgui.show_metrics_window()


def _menu_bar():
    with imgui.menu_bar() as bar:
        if not bar:
            return
        with imgui.menu('View') as menu:
            if menu:
                UI.menu_item('show_metrics', 'ImGui metrics window')
                UI.menu_item('overlay', 'Foreground overlay')


def _section_addons():
    if not hasattr(imgui, 'markdown'):
        imgui.text('Addon submodules are not present in this build.')
        return

    imgui.separator_text('markdown')
    imgui.markdown.render('**markdown** renders *inline* formatting and [links](https://github.com/ocornut/imgui).')

    imgui.separator_text('filebrowser')
    if _addons['browser'] is None:
        fb = imgui.filebrowser
        _addons['browser'] = fb.FileBrowser()
        _addons['browser'].set_use_modal(False)
        _addons['browser'].set_current_path('C:\\')
    browser = _addons['browser']
    if imgui.button('Open file browser'):
        imgui.open_popup('Pick a file')
    if browser.show_file_dialog('Pick a file', imgui.filebrowser.DialogMode.OPEN, (700.0, 450.0), '.py,.json,.txt'):
        _addons['selected_file'] = browser.selected_path
    imgui.text(f"selected: {_addons['selected_file']}")


def _overlay():
    fg = imgui.get_foreground_draw_list()
    io = imgui.get_io()
    white = imgui.color(1, 1, 1)
    accent = imgui.color(0.2, 1.0, 0.5)
    fg.add_rect((16, 16), (250, 64), white, 6.0, 1.5)
    fg.add_text((26, 22), white, 'PyImGui foreground overlay')
    fg.add_text((26, 42), accent, f'fps {io.framerate:.0f}  mouse {io.mouse_pos}')


def update():
    pass


def main():
    draw()

