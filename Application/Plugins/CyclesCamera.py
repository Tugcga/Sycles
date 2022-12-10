import sys
import win32com.client
from win32com.client import constants as c
log_message = Application.LogMessage

null = None
false = 0
true = 1

camera_types = ["Native XSI Camera", 0, "Panorama", 1]
panorama_types = ["Equirectangular", 0, "Fisheye Equidistant", 1, "Fisheye Equisolid", 2, "Mirrorball", 3, "Fisheye Lens Polynomial", 4]


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

    prop.AddParameter3("sensor_width", c.siFloat, 32.0)
    prop.AddParameter3("sensor_height", c.siFloat, 18.0)

    prop.AddParameter3("blades", c.siInt2, 0)
    prop.AddParameter3("blades_rotation", c.siFloat, 0.0, 0.0, 360.0)

    return True


def set_readonly(prop, array, value):
    for k in array:
        prop.Parameters(k).ReadOnly = value


def update(prop):
    camera_type = prop.Parameters("camera_type").Value
    panorama_type = prop.Parameters("panorama_type").Value
    panorama_general = ["panorama_type", "fisheye_fov"]
    panorama_eq = ["equ_latitude_min", "equ_latitude_max", "equ_longitude_min", "equ_longitude_max"]
    panorama_fisheye = ["fisheye_lens"]
    panorama_polynomial = ["polynomial_k0", "polynomial_k1", "polynomial_k2", "polynomial_k3", "polynomial_k4"]
    if camera_type == 0:
        set_readonly(prop, panorama_general, True)
        set_readonly(prop, panorama_eq, True)
        set_readonly(prop, panorama_fisheye, True)
        set_readonly(prop, panorama_polynomial, True)
    else:
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
        elif panorama_type == 3:
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


def build_camera_ui():
    props = PPG.Inspected(0)
    layout = PPG.PPGLayout
    layout.Clear()

    layout.AddTab("Camera")

    layout.AddGroup("Sensor")
    layout.AddItem("sensor_width", "Sensor Width")
    layout.AddItem("sensor_height", "Sensor Height")
    layout.EndGroup()

    layout.AddGroup("Camera Type")
    layout.AddEnumControl("camera_type", camera_types, "Type")
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

    PPG.Refresh()


def CyclesCamera_OnInit():
    build_camera_ui()
    update(PPG.Inspected(0))
    return True


def CyclesCamera_camera_type_OnChanged():
    update(PPG.Inspected(0))
    return True


def CyclesCamera_panorama_type_OnChanged():
    update(PPG.Inspected(0))
    return True