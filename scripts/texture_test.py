import PyImGui
import PySkillbar
import PyTexture

STATE = {
    'skill_id': 330,
    'dat_file_id': 0,
    'file_path': '',
    'model_file_id': 0,
    'dye_tint': 0,
    'dye': [0, 0, 0, 0],
    'preview_px': 64,
    'browser': None,
}


def _preview(label, handle, size_px):
    if handle:
        PyImGui.text('%s  ->  0x%X (ready)' % (label, handle))
        PyImGui.image(handle, (float(size_px), float(size_px)), (0.0, 0.0), (1.0, 1.0))
    else:
        PyImGui.text('%s  ->  0 (loading / not found / map not ready)' % label)


def draw():
    PyImGui.begin('Texture Test Harness')

    STATE['preview_px'] = PyImGui.input_int('preview size (px)', STATE['preview_px'], 16, 64, 0)
    if STATE['preview_px'] < 16:
        STATE['preview_px'] = 16
    PyImGui.separator()

    PyImGui.text('Skill icon (GW.dat) - the way scripts load skill icons:')
    STATE['skill_id'] = PyImGui.input_int('skill id', STATE['skill_id'], 1, 10, 0)
    if STATE['skill_id'] < 0:
        STATE['skill_id'] = 0

    icon_id = PySkillbar.get_skill_icon_file_id(STATE['skill_id']) if STATE['skill_id'] > 0 else 0
    hi_id = PySkillbar.get_skill_icon_file_id_hi_res(STATE['skill_id']) if STATE['skill_id'] > 0 else 0
    PyImGui.text('icon_file_id=%d   hi_res=%d' % (icon_id, hi_id))
    _preview('skill icon', PyTexture.get_texture_by_file_id(icon_id) if icon_id else 0, STATE['preview_px'])
    _preview('skill icon (hi-res)', PyTexture.get_texture_by_file_id(hi_id) if hi_id else 0, STATE['preview_px'])
    PyImGui.separator()

    PyImGui.text('Raw GW.dat file id:')
    STATE['dat_file_id'] = PyImGui.input_int('dat file id', STATE['dat_file_id'], 1, 100, 0)
    if STATE['dat_file_id'] < 0:
        STATE['dat_file_id'] = 0
    dat_handle = PyTexture.get_texture_by_file_id(STATE['dat_file_id']) if STATE['dat_file_id'] > 0 else 0
    _preview('dat texture', dat_handle, STATE['preview_px'])
    PyImGui.separator()

    PyImGui.text('Dyed model texture (GW.dat) - base icon + dye mask:')
    STATE['model_file_id'] = PyImGui.input_int('model file id', STATE['model_file_id'], 1, 100, 0)
    if STATE['model_file_id'] < 0:
        STATE['model_file_id'] = 0
    STATE['dye_tint'] = PyImGui.input_int('dye tint (0..49)', STATE['dye_tint'], 1, 5, 0)
    for i in range(4):
        STATE['dye'][i] = PyImGui.input_int('dye %d (1..13)' % (i + 1), STATE['dye'][i], 1, 1, 0)
    colored_handle = 0
    if STATE['model_file_id'] > 0:
        colored_handle = PyTexture.get_colored_model_texture(
            STATE['model_file_id'], STATE['dye_tint'],
            STATE['dye'][0], STATE['dye'][1], STATE['dye'][2], STATE['dye'][3])
    _preview('colored model', colored_handle, STATE['preview_px'])
    PyImGui.separator()

    PyImGui.text('From file (WIC):')
    fb = PyImGui.filebrowser
    if STATE['browser'] is None:
        STATE['browser'] = fb.FileBrowser()
        STATE['browser'].set_use_modal(False)
        STATE['browser'].set_current_path('C:\\')
    browser = STATE['browser']
    if PyImGui.button('Browse for texture...'):
        PyImGui.open_popup('Select a texture')
    if browser.show_file_dialog('Select a texture', fb.DialogMode.OPEN, (700.0, 450.0), '.png,.jpg,.jpeg,.bmp,.gif,.dds'):
        STATE['file_path'] = browser.selected_path
    PyImGui.text('selected: ' + (STATE['file_path'] if STATE['file_path'] else '(none)'))
    file_handle = PyTexture.get_file_texture(STATE['file_path']) if STATE['file_path'] else 0
    _preview('file texture', file_handle, STATE['preview_px'])
    PyImGui.separator()

    if PyImGui.button('Cleanup unused textures now'):
        PyTexture.cleanup_old_textures(0)
    PyImGui.text('(DAT icons decode asynchronously - wait a frame or two after changing an id.)')

    PyImGui.end()
