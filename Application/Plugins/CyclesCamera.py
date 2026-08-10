import sys
import re
import win32com.client
from win32com.client import constants as c
log_message = Application.LogMessage

null = None
false = 0
true = 1

OSLPARAM_PATTERN = "oslparam_"
OSLPARAM_NUMBER = "number_"
OSLPARAM_VECTOR = "vector_"
camera_types = ["Native XSI Camera", 0, "Panorama", 1, "OSL", 2]
panorama_types = ["Equirectangular", 0, "Fisheye Equidistant", 1, "Fisheye Equisolid", 2, "Mirrorball", 3, "Fisheye Lens Polynomial", 4, "Equiangular Cubemap Face", 5]

# -------------------------------------------------------------------
# -------------------------------------------------------------------
# ---OSL parsing functions (deepseek, 2026-08-09)--------------------

def remove_comments(text):
    text = re.sub(r'//.*?$', '', text, flags=re.MULTILINE)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    return text


def find_matching_paren(text, pos):
    stack = 1
    i = pos + 1
    while i < len(text) and stack > 0:
        ch = text[i]
        if ch == '(':
            stack += 1
        elif ch == ')':
            stack -= 1
        i += 1
    return i - 1 if stack == 0 else -1


def split_by_comma(s):
    parts = []
    current = []
    in_string = False
    escape = False
    paren_level = 0

    for ch in s:
        if in_string:
            if escape:
                escape = False
            elif ch == '\\':
                escape = True
            elif ch == '"':
                in_string = False
            current.append(ch)
            continue

        if ch == '"':
            in_string = True
            current.append(ch)
            continue

        if ch in '([{':
            paren_level += 1
            current.append(ch)
            continue

        if ch in ')]}':
            paren_level -= 1
            current.append(ch)
            continue

        if ch == ',' and paren_level == 0:
            parts.append(''.join(current).strip())
            current = []
            continue

        current.append(ch)

    if current:
        parts.append(''.join(current).strip())
    return parts


def extract_meta(meta_str, key):
    pattern = r'\b' + key + r'\s*=\s*([^,}\]]+)'
    m = re.search(pattern, meta_str)
    return m.group(1).strip() if m else None

def parse_declaration(decl):
    decl = decl.strip()
    if not decl:
        return None

    keywords = ['output', 'uniform', 'varying']
    for kw in keywords:
        if decl.startswith(kw):
            decl = decl[len(kw):].strip()

    tokens = decl.split()
    if len(tokens) < 2:
        return None

    type_str = tokens[0]
    name = tokens[1]

    rest = ' '.join(tokens[2:])

    default = None
    min_val = None
    max_val = None

    eq_pos = rest.find('=')
    if eq_pos != -1:
        default_part = rest[eq_pos+1:].strip()
        meta_pos = default_part.find('[[')
        if meta_pos != -1:
            default = default_part[:meta_pos].strip()
            meta = default_part[meta_pos+2:].strip()
            if meta.endswith(']]'):
                meta = meta[:-2].strip()
            min_val = extract_meta(meta, 'min')
            max_val = extract_meta(meta, 'max')
        else:
            default = default_part
    else:
        meta_pos = rest.find('[[')
        if meta_pos != -1:
            meta = rest[meta_pos+2:].strip()
            if meta.endswith(']]'):
                meta = meta[:-2].strip()
            min_val = extract_meta(meta, 'min')
            max_val = extract_meta(meta, 'max')

    return {
        'type': type_str,
        'name': name,
        'default': default,
        'min': min_val,
        'max': max_val
    }

def parse_params(params_str):
    parts = split_by_comma(params_str)
    params = []
    for part in parts:
        if not part:
            continue
        if part.lstrip().startswith('output'):
            continue
        param = parse_declaration(part)
        if param:
            params.append(param)
    return params


def fullmatch(pattern, string, flags=0):
    return re.match(r'^' + pattern + r'$', string, flags)

# ---OSL parsing functions-------------------------------------------
# -------------------------------------------------------------------
# -------------------------------------------------------------------



def XSILoadPlugin(in_reg):
    in_reg.Author = "Shekn Itrch"
    in_reg.Name = "CyclesCameraPlugin"
    in_reg.Major = 1
    in_reg.Minor = 0

    in_reg.RegisterProperty("CyclesCamera")
    in_reg.RegisterCommand("AddCyclesCamera","AddCyclesCamera")
    # RegistrationInsertionPoint - do not remove this line

    return true


def XSIUnloadPlugin(in_reg):
    strPluginName = in_reg.Name
    Application.LogMessage(str(strPluginName) + str("has been unloaded."), c.siVerbose)
    return true


def AddCyclesCamera_Init(in_ctxt):
    oCmd = in_ctxt.Source
    oCmd.Description = ""
    oCmd.Tooltip = ""
    oCmd.SetFlag(c.siSupportsKeyAssignment, False)
    oCmd.SetFlag(c.siCannotBeUsedInBatch, True)

    return true


def AddCyclesCamera_Execute():
    log_message("AddCyclesCamera_Execute called", c.siVerbose)

    selected_object = Application.Selection(0)
    if selected_object != None and selected_object.Type == "camera":
        if selected_object.GetPropertyFromName2("CyclesCamera"):
            prop = selected_object.GetPropertyFromName2("CyclesCamera")
        else:
            prop = selected_object.AddProperty("CyclesCamera")

        Application.InspectObj(prop)
    else:
        log_message("This property can be allpied only to camera object")

    return True


def CyclesCamera_Define(in_ctxt):
    prop = in_ctxt.Source
    prop.AddParameter3("aperture_size", c.siFloat, 0.0)
    prop.AddParameter3("aperture_ratio", c.siFloat, 1.0)

    prop.AddParameter3("camera_type", c.siInt2, 0)
    prop.AddParameter3("osl_path", c.siString, "", "", "", False, False)
    prop.AddParameter3("panorama_type", c.siInt2, 0)

    prop.AddParameter3("fisheye_fov", c.siFloat, 180.0, 10.0, 360.0)
    prop.AddParameter3("fisheye_lens", c.siFloat, 10.5, 0.01, 15.0)

    prop.AddParameter3("polynomial_k0", c.siFloat, -0.000672, -0.001, 0.001)
    prop.AddParameter3("polynomial_k1", c.siFloat, -1.14527, -4.0, 0.0)
    prop.AddParameter3("polynomial_k2", c.siFloat, -0.000192, -0.001, 0.001)
    prop.AddParameter3("polynomial_k3", c.siFloat, 0.000178, -0.001, 0.001)
    prop.AddParameter3("polynomial_k4", c.siFloat, -0.000001, -0.00001, 0.00001)

    # Equirectangular parameters
    prop.AddParameter3("equ_latitude_min", c.siFloat, -90.0, -90.0, 90.0)
    prop.AddParameter3("equ_latitude_max", c.siFloat, 90.0, -90.0, 90.0)
    prop.AddParameter3("equ_longitude_min", c.siFloat, -180.0, -180.0, 180.0)
    prop.AddParameter3("equ_longitude_max", c.siFloat, 180.0, -180.0, 180.0)

    prop.AddParameter3("sensor_size", c.siFloat, 32.0)

    prop.AddParameter3("blades", c.siInt2, 0)
    prop.AddParameter3("blades_rotation", c.siFloat, 0.0, 0.0, 360.0)
    return True


def set_readonly(prop, array, value):
    for k in array:
        prop.Parameters(k).ReadOnly = value


def clear_osl_parameters(prop):
    all_parameters = prop.Parameters
    for param in all_parameters:
        # find parameter with the pattern oslparam_xxx
        param_name = param.Name
        if param_name[:len(OSLPARAM_PATTERN)] == OSLPARAM_PATTERN:
            prop.RemoveParameter(param)


def extract_parms_str(osl_str):
    text = remove_comments(osl_str)
    m = re.search(r'shader\s+(\w+)\s*\(', text)
    open_paren = m.end() - 1
    close_paren = find_matching_paren(text, open_paren)
    params_str = text[open_paren+1:close_paren]
    return params_str.strip()


def get_oslparams_count(prop):
    all_parameters = prop.Parameters
    counter = 0
    for param in all_parameters:
        param_name = param.Name
        if param_name[:len(OSLPARAM_PATTERN)] == OSLPARAM_PATTERN:
            counter += 1
    return counter


def is_numeric_regex(s):
    return bool(fullmatch(r"^-?\d+(\.\d+)?$", s))


def is_integer_regex(s):
    return bool(fullmatch(r"^[+-]?\d+$", s))


def defaul_str_to_value(default, param_type):
    '''default is string, and type also is the string
    default can start fom something like vector(...), or it can be simple value
    '''
    if default is None:
        return None

    if param_type == "string":
        return default
    if is_numeric_regex(default):
        if param_type == "int":
            if is_integer_regex(default):
                return int(default)
            else:
                return None
        elif param_type == "float":
            return float(default)
        elif param_type == "color" or param_type == "vector":
            return [float(default) for _ in range(3)]
        else:
            return None
    else:
        # if value is not a number, then it should be a vector
        # something like color(r, g, b), point(a, b, c) and so on
        # so, it should contain parenthesis
        left_paren = default.find("(")
        if left_paren == -1:
            return None
        right_paren = find_matching_paren(default, left_paren)
        if right_paren == -1:
            return None
        default_parts = default[left_paren + 1:right_paren].split(", ")
        if len(default_parts) != 3 or not all(is_numeric_regex(part) for part in default_parts):
            return None
        return [float(v) for v in default_parts]


def add_one_osl_param(prop, param_data):
    '''param_data is the dictionary with keys:
    - type
    - name
    - default
    - min
    - max
    '''
    param_name = param_data["name"]
    param_type = param_data["type"]
    if param_type not in ["int", "float", "string", "vector", "color", "point", "normal"]:
        return None

    param_default = defaul_str_to_value(param_data["default"], param_type)
    if param_default is None or len(param_name) == 0:
        return None

    if param_type == "int":
        prop.AddParameter3(OSLPARAM_PATTERN + OSLPARAM_NUMBER + param_name, c.siInt4, param_default)
    elif param_type == "float":
        prop.AddParameter3(OSLPARAM_PATTERN + OSLPARAM_NUMBER + param_name, c.siFloat, param_default)
    elif param_type == "string":
        prop.AddParameter3(OSLPARAM_PATTERN + OSLPARAM_NUMBER + param_name, c.siString, param_default)
    else:
        # for all other supported types we should create three parameters, which ends x_, y_, z_
        # and add OSLPARAM_VECTOR at the name start
        # so, the name is oslparam_vector_x_name
        prop.AddParameter3(OSLPARAM_PATTERN + OSLPARAM_VECTOR + "x_" + param_name, c.siFloat, param_default[0])
        prop.AddParameter3(OSLPARAM_PATTERN + OSLPARAM_VECTOR + "y_" + param_name, c.siFloat, param_default[1])
        prop.AddParameter3(OSLPARAM_PATTERN + OSLPARAM_VECTOR + "z_" + param_name, c.siFloat, param_default[2])


def define_osl_parameters(prop):
    ols_path = prop.Parameters("osl_path").Value
    if len(ols_path) == 0:
        return None

    with open(ols_path, "rt") as file:
        osl_code = file.read()
        osl_code = remove_comments(osl_code)
        params_str = extract_parms_str(osl_code)
        params = parse_params(params_str)
        for param in params:
            add_one_osl_param(prop, param)


def build_camera_ui():
    props = PPG.Inspected(0)
    layout = PPG.PPGLayout
    layout.Clear()

    layout.AddTab("Camera")

    layout.AddGroup("Sensor")
    layout.AddItem("sensor_size", "Sensor Size")
    layout.EndGroup()

    layout.AddGroup("Camera Type")
    layout.AddEnumControl("camera_type", camera_types, "Type")
    item = layout.AddItem("osl_path", "OSL File", c.siControlFilePath)
    filterstring = "OSL files (*.osl)|*.osl|"
    item.SetAttribute(c.siUIOpenFile, True)
    item.SetAttribute(c.siUIFileMustExist, True)
    item.SetAttribute(c.siUIFileFilter, filterstring)
    layout.EndGroup()

    layout.AddGroup("Depth Of Field")
    layout.AddItem("aperture_size", "Aperture Size")
    layout.AddItem("blades", "Blades")
    layout.AddItem("blades_rotation", "Blades Rotation")
    layout.AddItem("aperture_ratio", "Aperture Ratio")
    layout.EndGroup()

    layout.AddTab("Panorama")
    layout.AddGroup("General")
    layout.AddEnumControl("panorama_type", panorama_types, "Panorama Type")
    layout.AddItem("fisheye_fov", "Field of View")
    layout.EndGroup()
    # Equirectangular
    layout.AddGroup("Equirectangular Parameters")
    layout.AddItem("equ_latitude_min", "Min Latitude")
    layout.AddItem("equ_latitude_max", "Max Latitude")
    layout.AddItem("equ_longitude_min", "Min Longitude")
    layout.AddItem("equ_longitude_max", "Max Longitude")
    layout.EndGroup()
    
    # Fisheye Equidistant
    layout.AddGroup("Fisheye Parameters")
    layout.EndGroup()
    
    # Fisheye Equisolid
    layout.AddGroup("Fisheye Parameters")
    layout.AddItem("fisheye_lens", "Fisheye Lense")
    layout.EndGroup()

    # Polynomial
    layout.AddGroup("Polynomial Parameters")
    layout.AddItem("polynomial_k0", "K0")
    layout.AddItem("polynomial_k1", "K1")
    layout.AddItem("polynomial_k2", "K2")
    layout.AddItem("polynomial_k3", "K3")
    layout.AddItem("polynomial_k4", "K4")
    layout.EndGroup()

    if get_oslparams_count(props):
        layout.AddTab("OSL")
        layout.AddGroup("Parameters")
        all_parameters = props.Parameters
        for param in all_parameters:
            param_name = param.Name
            if param_name.startswith(OSLPARAM_PATTERN):
                if param_name[len(OSLPARAM_PATTERN):].startswith(OSLPARAM_NUMBER):
                    layout.AddItem(param_name, param_name[len(OSLPARAM_PATTERN)+len(OSLPARAM_NUMBER):])
                elif param_name[len(OSLPARAM_PATTERN):].startswith(OSLPARAM_VECTOR):
                    # create item only for ..._x_name and skip others
                    start = len(OSLPARAM_PATTERN)+len(OSLPARAM_VECTOR)
                    key = param_name[start:start+2]
                    if key == "x_":
                        layout.AddGroup(param_name[start+2:])
                        layout.AddRow()
                        layout.AddItem(param_name, "X")
                        layout.AddItem(param_name[:start] + "y_" + param_name[start+2:], "Y")
                        layout.AddItem(param_name[:start] + "z_" + param_name[start+2:], "Z")
                        layout.EndRow()
                        layout.EndGroup()
        layout.AddButton("Reparse Parameters")
        layout.EndGroup()

    PPG.Refresh()


def update(prop, update_trigger):
    '''
    update_trigger = 0 for OnInit
    update_trigger = 1 for camera_type_OnChanged
    update_trigger = 2 for panorama_type_OnChanged
    update_trigger = 3 for osl_path_OnChanged
    '''
    camera_type = prop.Parameters("camera_type").Value
    panorama_type = prop.Parameters("panorama_type").Value
    panorama_general = ["panorama_type", "fisheye_fov"]
    panorama_eq = ["equ_latitude_min", "equ_latitude_max", "equ_longitude_min", "equ_longitude_max"]
    panorama_fisheye = ["fisheye_lens"]
    panorama_polynomial = ["polynomial_k0", "polynomial_k1", "polynomial_k2", "polynomial_k3", "polynomial_k4"]
    if update_trigger == 1 or update_trigger == 3:
        # change camera type
        # become osl, or switch another type
        # in any case we should clear all parametrs
        # and then recreate it, if needed
        clear_osl_parameters(prop)
        if camera_type == 2:
            define_osl_parameters(prop)
            # after new parameters rebuild ui
            build_camera_ui()

    if camera_type == 0:
        set_readonly(prop, panorama_general, True)
        set_readonly(prop, panorama_eq, True)
        set_readonly(prop, panorama_fisheye, True)
        set_readonly(prop, panorama_polynomial, True)
        prop.Parameters("osl_path").ReadOnly = True
    elif camera_type == 1:
        if panorama_type == 0:
            set_readonly(prop, panorama_general, False)
            set_readonly(prop, ["fisheye_fov"], True)
            set_readonly(prop, panorama_eq, False)
            set_readonly(prop, panorama_fisheye, True)
            set_readonly(prop, panorama_polynomial, True)
        elif panorama_type == 1:
            set_readonly(prop, panorama_general, False)
            set_readonly(prop, panorama_eq, True)
            set_readonly(prop, panorama_fisheye, True)
            set_readonly(prop, panorama_polynomial, True)
        elif panorama_type == 2:
            set_readonly(prop, panorama_general, False)
            set_readonly(prop, panorama_eq, True)
            set_readonly(prop, panorama_fisheye, False)
            set_readonly(prop, panorama_polynomial, True)
        elif panorama_type == 3 or panorama_type == 5:
            set_readonly(prop, panorama_general, False)
            set_readonly(prop, ["fisheye_fov"], True)
            set_readonly(prop, panorama_eq, True)
            set_readonly(prop, panorama_fisheye, True)
            set_readonly(prop, panorama_polynomial, True)
        elif panorama_type == 4:
            set_readonly(prop, panorama_general, False)
            set_readonly(prop, panorama_eq, True)
            set_readonly(prop, panorama_fisheye, True)
            set_readonly(prop, panorama_polynomial, False)
        prop.Parameters("osl_path").ReadOnly = True
    elif camera_type == 2:
        prop.Parameters("osl_path").ReadOnly = False


def CyclesCamera_OnInit():
    build_camera_ui()
    update(PPG.Inspected(0), 0)
    return True


def CyclesCamera_camera_type_OnChanged():
    update(PPG.Inspected(0), 1)
    return True


def CyclesCamera_panorama_type_OnChanged():
    update(PPG.Inspected(0), 2)
    return True


def CyclesCamera_osl_path_OnChanged():
    update(PPG.Inspected(0), 3)
    return True


def CyclesCamera_Reparse_Parameters_OnClicked():
    update(PPG.Inspected(0), 3)
    return True