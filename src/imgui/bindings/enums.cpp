#include "base/error_handling.h"

#include "imgui/bindings.h"

#include <imgui.h>

// Uniform flag-enum surface. Every flag enum gets bitwise operators that keep
// the enum type (so composition stays typed and chainable):
//   WindowFlags.NoTitleBar | WindowFlags.NoResize
#define BIND_FLAGS_ENUM(NAME, TYPE) py::enum_<TYPE>(m, NAME)
#define BIND_FLAGS_END(TYPE) \
    .export_values() \
    .def("__or__",     [](TYPE a, TYPE b) -> TYPE { return static_cast<TYPE>(static_cast<int>(a) | static_cast<int>(b)); }) \
    .def("__and__",    [](TYPE a, TYPE b) -> TYPE { return static_cast<TYPE>(static_cast<int>(a) & static_cast<int>(b)); }) \
    .def("__xor__",    [](TYPE a, TYPE b) -> TYPE { return static_cast<TYPE>(static_cast<int>(a) ^ static_cast<int>(b)); }) \
    .def("__invert__", [](TYPE a)         -> TYPE { return static_cast<TYPE>(~static_cast<int>(a)); })

namespace PY4GW::imgui_bindings {

void register_enums(py::module_& m) {
    py::enum_<ImGuiSortDirection>(m, "SortDirection")
        .value("None", ImGuiSortDirection_None)
        .value("Ascending", ImGuiSortDirection_Ascending)
        .value("Descending", ImGuiSortDirection_Descending)
        .export_values();

    BIND_FLAGS_ENUM("WindowFlags", ImGuiWindowFlags_)
        .value("NoFlag", ImGuiWindowFlags_None).value("NoTitleBar", ImGuiWindowFlags_NoTitleBar)
        .value("NoResize", ImGuiWindowFlags_NoResize).value("NoMove", ImGuiWindowFlags_NoMove)
        .value("NoScrollbar", ImGuiWindowFlags_NoScrollbar).value("NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse)
        .value("NoCollapse", ImGuiWindowFlags_NoCollapse).value("AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize)
        .value("NoBackground", ImGuiWindowFlags_NoBackground).value("NoSavedSettings", ImGuiWindowFlags_NoSavedSettings)
        .value("NoMouseInputs", ImGuiWindowFlags_NoMouseInputs).value("MenuBar", ImGuiWindowFlags_MenuBar)
        .value("HorizontalScrollbar", ImGuiWindowFlags_HorizontalScrollbar).value("NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing)
        .value("NoBringToFrontOnFocus", ImGuiWindowFlags_NoBringToFrontOnFocus)
        .value("AlwaysVerticalScrollbar", ImGuiWindowFlags_AlwaysVerticalScrollbar).value("AlwaysHorizontalScrollbar", ImGuiWindowFlags_AlwaysHorizontalScrollbar)
        .value("NoNavInputs", ImGuiWindowFlags_NoNavInputs).value("NoNavFocus", ImGuiWindowFlags_NoNavFocus)
        .value("UnsavedDocument", ImGuiWindowFlags_UnsavedDocument).value("NoDocking", ImGuiWindowFlags_NoDocking)
        .value("NoInputs", ImGuiWindowFlags_NoInputs)
    BIND_FLAGS_END(ImGuiWindowFlags_);

    BIND_FLAGS_ENUM("ChildFlags", ImGuiChildFlags_)
        .value("NoFlag", ImGuiChildFlags_None).value("Borders", ImGuiChildFlags_Borders)
        .value("AlwaysUseWindowPadding", ImGuiChildFlags_AlwaysUseWindowPadding).value("ResizeX", ImGuiChildFlags_ResizeX)
        .value("ResizeY", ImGuiChildFlags_ResizeY).value("AutoResizeX", ImGuiChildFlags_AutoResizeX)
        .value("AutoResizeY", ImGuiChildFlags_AutoResizeY).value("AlwaysAutoResize", ImGuiChildFlags_AlwaysAutoResize)
        .value("FrameStyle", ImGuiChildFlags_FrameStyle)
    BIND_FLAGS_END(ImGuiChildFlags_);

    BIND_FLAGS_ENUM("InputTextFlags", ImGuiInputTextFlags_)
        .value("NoFlag", ImGuiInputTextFlags_None).value("CharsDecimal", ImGuiInputTextFlags_CharsDecimal)
        .value("CharsHexadecimal", ImGuiInputTextFlags_CharsHexadecimal).value("CharsUppercase", ImGuiInputTextFlags_CharsUppercase)
        .value("CharsNoBlank", ImGuiInputTextFlags_CharsNoBlank).value("AutoSelectAll", ImGuiInputTextFlags_AutoSelectAll)
        .value("EnterReturnsTrue", ImGuiInputTextFlags_EnterReturnsTrue).value("CallbackCompletion", ImGuiInputTextFlags_CallbackCompletion)
        .value("CallbackHistory", ImGuiInputTextFlags_CallbackHistory).value("CallbackAlways", ImGuiInputTextFlags_CallbackAlways)
        .value("CallbackCharFilter", ImGuiInputTextFlags_CallbackCharFilter).value("AllowTabInput", ImGuiInputTextFlags_AllowTabInput)
        .value("CtrlEnterForNewLine", ImGuiInputTextFlags_CtrlEnterForNewLine).value("NoHorizontalScroll", ImGuiInputTextFlags_NoHorizontalScroll)
        .value("ReadOnly", ImGuiInputTextFlags_ReadOnly).value("Password", ImGuiInputTextFlags_Password)
        .value("NoUndoRedo", ImGuiInputTextFlags_NoUndoRedo).value("CharsScientific", ImGuiInputTextFlags_CharsScientific)
        .value("CallbackResize", ImGuiInputTextFlags_CallbackResize).value("CallbackEdit", ImGuiInputTextFlags_CallbackEdit)
        .value("WordWrap", ImGuiInputTextFlags_WordWrap)
    BIND_FLAGS_END(ImGuiInputTextFlags_);

    BIND_FLAGS_ENUM("TreeNodeFlags", ImGuiTreeNodeFlags_)
        .value("NoFlag", ImGuiTreeNodeFlags_None).value("Selected", ImGuiTreeNodeFlags_Selected)
        .value("Framed", ImGuiTreeNodeFlags_Framed).value("NoTreePushOnOpen", ImGuiTreeNodeFlags_NoTreePushOnOpen)
        .value("NoAutoOpenOnLog", ImGuiTreeNodeFlags_NoAutoOpenOnLog).value("DefaultOpen", ImGuiTreeNodeFlags_DefaultOpen)
        .value("OpenOnDoubleClick", ImGuiTreeNodeFlags_OpenOnDoubleClick).value("OpenOnArrow", ImGuiTreeNodeFlags_OpenOnArrow)
        .value("Leaf", ImGuiTreeNodeFlags_Leaf).value("Bullet", ImGuiTreeNodeFlags_Bullet)
        .value("FramePadding", ImGuiTreeNodeFlags_FramePadding).value("SpanAvailWidth", ImGuiTreeNodeFlags_SpanAvailWidth)
        .value("SpanFullWidth", ImGuiTreeNodeFlags_SpanFullWidth).value("NavLeftJumpsBackHere", ImGuiTreeNodeFlags_NavLeftJumpsBackHere)
        .value("CollapsingHeader", ImGuiTreeNodeFlags_CollapsingHeader)
    BIND_FLAGS_END(ImGuiTreeNodeFlags_);

    BIND_FLAGS_ENUM("PopupFlags", ImGuiPopupFlags_)
        .value("NoFlag", ImGuiPopupFlags_None).value("MouseButtonLeft", ImGuiPopupFlags_MouseButtonLeft)
        .value("MouseButtonRight", ImGuiPopupFlags_MouseButtonRight).value("MouseButtonMiddle", ImGuiPopupFlags_MouseButtonMiddle)
        .value("NoOpenOverExistingPopup", ImGuiPopupFlags_NoOpenOverExistingPopup).value("NoOpenOverItems", ImGuiPopupFlags_NoOpenOverItems)
        .value("AnyPopupId", ImGuiPopupFlags_AnyPopupId).value("AnyPopupLevel", ImGuiPopupFlags_AnyPopupLevel)
    BIND_FLAGS_END(ImGuiPopupFlags_);

    BIND_FLAGS_ENUM("SelectableFlags", ImGuiSelectableFlags_)
        .value("NoFlag", ImGuiSelectableFlags_None).value("DontClosePopups", ImGuiSelectableFlags_DontClosePopups)
        .value("SpanAllColumns", ImGuiSelectableFlags_SpanAllColumns).value("AllowDoubleClick", ImGuiSelectableFlags_AllowDoubleClick)
        .value("Disabled", ImGuiSelectableFlags_Disabled)
    BIND_FLAGS_END(ImGuiSelectableFlags_);

    BIND_FLAGS_ENUM("ComboFlags", ImGuiComboFlags_)
        .value("NoFlag", ImGuiComboFlags_None).value("PopupAlignLeft", ImGuiComboFlags_PopupAlignLeft)
        .value("HeightSmall", ImGuiComboFlags_HeightSmall).value("HeightRegular", ImGuiComboFlags_HeightRegular)
        .value("HeightLarge", ImGuiComboFlags_HeightLarge).value("HeightLargest", ImGuiComboFlags_HeightLargest)
        .value("NoArrowButton", ImGuiComboFlags_NoArrowButton).value("NoPreview", ImGuiComboFlags_NoPreview)
    BIND_FLAGS_END(ImGuiComboFlags_);

    BIND_FLAGS_ENUM("TabBarFlags", ImGuiTabBarFlags_)
        .value("NoFlag", ImGuiTabBarFlags_None).value("Reorderable", ImGuiTabBarFlags_Reorderable)
        .value("AutoSelectNewTabs", ImGuiTabBarFlags_AutoSelectNewTabs).value("TabListPopupButton", ImGuiTabBarFlags_TabListPopupButton)
        .value("NoCloseWithMiddleMouseButton", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)
        .value("NoTabListScrollingButtons", ImGuiTabBarFlags_NoTabListScrollingButtons).value("NoTooltip", ImGuiTabBarFlags_NoTooltip)
        .value("DrawSelectedOverline", ImGuiTabBarFlags_DrawSelectedOverline).value("FittingPolicyMixed", ImGuiTabBarFlags_FittingPolicyMixed)
        .value("FittingPolicyShrink", ImGuiTabBarFlags_FittingPolicyShrink).value("FittingPolicyScroll", ImGuiTabBarFlags_FittingPolicyScroll)
        .value("FittingPolicyMask_", ImGuiTabBarFlags_FittingPolicyMask_).value("FittingPolicyDefault_", ImGuiTabBarFlags_FittingPolicyDefault_)
    BIND_FLAGS_END(ImGuiTabBarFlags_);

    BIND_FLAGS_ENUM("TabItemFlags", ImGuiTabItemFlags_)
        .value("NoFlag", ImGuiTabItemFlags_None).value("UnsavedDocument", ImGuiTabItemFlags_UnsavedDocument)
        .value("SetSelected", ImGuiTabItemFlags_SetSelected).value("NoCloseWithMiddleMouseButton", ImGuiTabItemFlags_NoCloseWithMiddleMouseButton)
        .value("NoPushId", ImGuiTabItemFlags_NoPushId).value("NoTooltip", ImGuiTabItemFlags_NoTooltip)
        .value("NoReorder", ImGuiTabItemFlags_NoReorder).value("Leading", ImGuiTabItemFlags_Leading)
        .value("Trailing", ImGuiTabItemFlags_Trailing).value("NoAssumedClosure", ImGuiTabItemFlags_NoAssumedClosure)
    BIND_FLAGS_END(ImGuiTabItemFlags_);

    BIND_FLAGS_ENUM("FocusedFlags", ImGuiFocusedFlags_)
        .value("NoFlag", ImGuiFocusedFlags_None).value("ChildWindows", ImGuiFocusedFlags_ChildWindows)
        .value("RootWindow", ImGuiFocusedFlags_RootWindow).value("AnyWindow", ImGuiFocusedFlags_AnyWindow)
        .value("RootAndChildWindows", ImGuiFocusedFlags_RootAndChildWindows)
    BIND_FLAGS_END(ImGuiFocusedFlags_);

    BIND_FLAGS_ENUM("HoveredFlags", ImGuiHoveredFlags_)
        .value("NoFlag", ImGuiHoveredFlags_None).value("ChildWindows", ImGuiHoveredFlags_ChildWindows)
        .value("RootWindow", ImGuiHoveredFlags_RootWindow).value("AnyWindow", ImGuiHoveredFlags_AnyWindow)
        .value("AllowWhenBlockedByPopup", ImGuiHoveredFlags_AllowWhenBlockedByPopup)
        .value("AllowWhenBlockedByActiveItem", ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
        .value("AllowWhenOverlapped", ImGuiHoveredFlags_AllowWhenOverlapped)
        .value("AllowWhenDisabled", ImGuiHoveredFlags_AllowWhenDisabled).value("ForTooltip", ImGuiHoveredFlags_ForTooltip)
    BIND_FLAGS_END(ImGuiHoveredFlags_);

    BIND_FLAGS_ENUM("DockNodeFlags", ImGuiDockNodeFlags_)
        .value("NoFlag", ImGuiDockNodeFlags_None).value("KeepAliveOnly", ImGuiDockNodeFlags_KeepAliveOnly)
        .value("NoDockingOverCentralNode", ImGuiDockNodeFlags_NoDockingOverCentralNode).value("PassthruCentralNode", ImGuiDockNodeFlags_PassthruCentralNode)
        .value("NoDockingSplit", ImGuiDockNodeFlags_NoDockingSplit).value("NoResize", ImGuiDockNodeFlags_NoResize)
        .value("AutoHideTabBar", ImGuiDockNodeFlags_AutoHideTabBar).value("NoUndocking", ImGuiDockNodeFlags_NoUndocking)
    BIND_FLAGS_END(ImGuiDockNodeFlags_);

    BIND_FLAGS_ENUM("DragDropFlags", ImGuiDragDropFlags_)
        .value("NoFlag", ImGuiDragDropFlags_None).value("SourceNoPreviewTooltip", ImGuiDragDropFlags_SourceNoPreviewTooltip)
        .value("SourceNoDisableHover", ImGuiDragDropFlags_SourceNoDisableHover).value("SourceNoHoldToOpenOthers", ImGuiDragDropFlags_SourceNoHoldToOpenOthers)
        .value("SourceAllowNullID", ImGuiDragDropFlags_SourceAllowNullID).value("SourceExtern", ImGuiDragDropFlags_SourceExtern)
        .value("PayloadAutoExpire", ImGuiDragDropFlags_PayloadAutoExpire).value("PayloadNoCrossContext", ImGuiDragDropFlags_PayloadNoCrossContext)
        .value("PayloadNoCrossProcess", ImGuiDragDropFlags_PayloadNoCrossProcess).value("AcceptBeforeDelivery", ImGuiDragDropFlags_AcceptBeforeDelivery)
        .value("AcceptNoDrawDefaultRect", ImGuiDragDropFlags_AcceptNoDrawDefaultRect).value("AcceptNoPreviewTooltip", ImGuiDragDropFlags_AcceptNoPreviewTooltip)
        .value("AcceptPeekOnly", ImGuiDragDropFlags_AcceptPeekOnly)
    BIND_FLAGS_END(ImGuiDragDropFlags_);

    BIND_FLAGS_ENUM("SliderFlags", ImGuiSliderFlags_)
        .value("NoFlag", ImGuiSliderFlags_None).value("AlwaysClamp", ImGuiSliderFlags_AlwaysClamp)
        .value("Logarithmic", ImGuiSliderFlags_Logarithmic).value("NoRoundToFormat", ImGuiSliderFlags_NoRoundToFormat)
        .value("NoInput", ImGuiSliderFlags_NoInput).value("InvalidMask_", ImGuiSliderFlags_InvalidMask_)
    BIND_FLAGS_END(ImGuiSliderFlags_);

    BIND_FLAGS_ENUM("ButtonFlags", ImGuiButtonFlags_)
        .value("NoFlag", ImGuiButtonFlags_None).value("MouseButtonLeft", ImGuiButtonFlags_MouseButtonLeft)
        .value("MouseButtonRight", ImGuiButtonFlags_MouseButtonRight).value("MouseButtonMiddle", ImGuiButtonFlags_MouseButtonMiddle)
        .value("EnableNav", ImGuiButtonFlags_EnableNav)
    BIND_FLAGS_END(ImGuiButtonFlags_);

    BIND_FLAGS_ENUM("TableFlags", ImGuiTableFlags_)
        .value("NoFlag", ImGuiTableFlags_None).value("Resizable", ImGuiTableFlags_Resizable).value("Reorderable", ImGuiTableFlags_Reorderable)
        .value("Hideable", ImGuiTableFlags_Hideable).value("Sortable", ImGuiTableFlags_Sortable)
        .value("NoSavedSettings", ImGuiTableFlags_NoSavedSettings).value("ContextMenuInBody", ImGuiTableFlags_ContextMenuInBody)
        .value("RowBg", ImGuiTableFlags_RowBg).value("BordersInnerH", ImGuiTableFlags_BordersInnerH)
        .value("BordersOuterH", ImGuiTableFlags_BordersOuterH).value("BordersInnerV", ImGuiTableFlags_BordersInnerV)
        .value("BordersOuterV", ImGuiTableFlags_BordersOuterV).value("BordersH", ImGuiTableFlags_BordersH)
        .value("BordersV", ImGuiTableFlags_BordersV).value("Borders", ImGuiTableFlags_Borders)
        .value("NoBordersInBody", ImGuiTableFlags_NoBordersInBody).value("NoBordersInBodyUntilResize", ImGuiTableFlags_NoBordersInBodyUntilResize)
        .value("SizingFixedFit", ImGuiTableFlags_SizingFixedFit).value("SizingFixedSame", ImGuiTableFlags_SizingFixedSame)
        .value("SizingStretchProp", ImGuiTableFlags_SizingStretchProp).value("SizingStretchSame", ImGuiTableFlags_SizingStretchSame)
        .value("NoHostExtendX", ImGuiTableFlags_NoHostExtendX).value("NoHostExtendY", ImGuiTableFlags_NoHostExtendY)
        .value("NoKeepColumnsVisible", ImGuiTableFlags_NoKeepColumnsVisible).value("PreciseWidths", ImGuiTableFlags_PreciseWidths)
        .value("NoClip", ImGuiTableFlags_NoClip).value("PadOuterX", ImGuiTableFlags_PadOuterX)
        .value("NoPadOuterX", ImGuiTableFlags_NoPadOuterX).value("NoPadInnerX", ImGuiTableFlags_NoPadInnerX)
        .value("ScrollX", ImGuiTableFlags_ScrollX).value("ScrollY", ImGuiTableFlags_ScrollY)
        .value("SortMulti", ImGuiTableFlags_SortMulti).value("SortTristate", ImGuiTableFlags_SortTristate)
    BIND_FLAGS_END(ImGuiTableFlags_);

    BIND_FLAGS_ENUM("TableColumnFlags", ImGuiTableColumnFlags_)
        .value("NoFlag", ImGuiTableColumnFlags_None).value("DefaultHide", ImGuiTableColumnFlags_DefaultHide)
        .value("DefaultSort", ImGuiTableColumnFlags_DefaultSort).value("WidthStretch", ImGuiTableColumnFlags_WidthStretch)
        .value("WidthFixed", ImGuiTableColumnFlags_WidthFixed).value("NoResize", ImGuiTableColumnFlags_NoResize)
        .value("NoReorder", ImGuiTableColumnFlags_NoReorder).value("NoHide", ImGuiTableColumnFlags_NoHide)
        .value("NoClip", ImGuiTableColumnFlags_NoClip).value("NoSort", ImGuiTableColumnFlags_NoSort)
        .value("NoSortAscending", ImGuiTableColumnFlags_NoSortAscending).value("NoSortDescending", ImGuiTableColumnFlags_NoSortDescending)
        .value("IndentEnable", ImGuiTableColumnFlags_IndentEnable).value("IndentDisable", ImGuiTableColumnFlags_IndentDisable)
        .value("IsEnabled", ImGuiTableColumnFlags_IsEnabled).value("IsVisible", ImGuiTableColumnFlags_IsVisible)
        .value("IsSorted", ImGuiTableColumnFlags_IsSorted).value("IsHovered", ImGuiTableColumnFlags_IsHovered)
    BIND_FLAGS_END(ImGuiTableColumnFlags_);

    BIND_FLAGS_ENUM("TableRowFlags", ImGuiTableRowFlags_)
        .value("NoFlag", ImGuiTableRowFlags_None).value("Headers", ImGuiTableRowFlags_Headers)
    BIND_FLAGS_END(ImGuiTableRowFlags_);

    BIND_FLAGS_ENUM("DrawFlags", ImDrawFlags_)
        .value("NoFlag", ImDrawFlags_None).value("Closed", ImDrawFlags_Closed)
        .value("RoundCornersTopLeft", ImDrawFlags_RoundCornersTopLeft).value("RoundCornersTopRight", ImDrawFlags_RoundCornersTopRight)
        .value("RoundCornersBottomLeft", ImDrawFlags_RoundCornersBottomLeft).value("RoundCornersBottomRight", ImDrawFlags_RoundCornersBottomRight)
        .value("RoundCornersNone", ImDrawFlags_RoundCornersNone).value("RoundCornersTop", ImDrawFlags_RoundCornersTop)
        .value("RoundCornersBottom", ImDrawFlags_RoundCornersBottom).value("RoundCornersLeft", ImDrawFlags_RoundCornersLeft)
        .value("RoundCornersRight", ImDrawFlags_RoundCornersRight).value("RoundCornersAll", ImDrawFlags_RoundCornersAll)
        .value("RoundCornersDefault", ImDrawFlags_RoundCornersAll)
    BIND_FLAGS_END(ImDrawFlags_);

    BIND_FLAGS_ENUM("ColorEditFlags", ImGuiColorEditFlags_)
        .value("NoFlag", ImGuiColorEditFlags_None).value("NoAlpha", ImGuiColorEditFlags_NoAlpha)
        .value("NoPicker", ImGuiColorEditFlags_NoPicker).value("NoOptions", ImGuiColorEditFlags_NoOptions)
        .value("NoSmallPreview", ImGuiColorEditFlags_NoSmallPreview).value("NoInputs", ImGuiColorEditFlags_NoInputs)
        .value("NoTooltip", ImGuiColorEditFlags_NoTooltip).value("NoLabel", ImGuiColorEditFlags_NoLabel)
        .value("NoSidePreview", ImGuiColorEditFlags_NoSidePreview).value("NoDragDrop", ImGuiColorEditFlags_NoDragDrop)
        .value("NoBorder", ImGuiColorEditFlags_NoBorder).value("AlphaBar", ImGuiColorEditFlags_AlphaBar)
        .value("AlphaPreview", ImGuiColorEditFlags_AlphaPreview).value("AlphaPreviewHalf", ImGuiColorEditFlags_AlphaPreviewHalf)
        .value("HDR", ImGuiColorEditFlags_HDR).value("DisplayRGB", ImGuiColorEditFlags_DisplayRGB)
        .value("DisplayHSV", ImGuiColorEditFlags_DisplayHSV).value("DisplayHex", ImGuiColorEditFlags_DisplayHex)
        .value("Uint8", ImGuiColorEditFlags_Uint8).value("Float", ImGuiColorEditFlags_Float)
        .value("PickerHueBar", ImGuiColorEditFlags_PickerHueBar).value("PickerHueWheel", ImGuiColorEditFlags_PickerHueWheel)
        .value("InputRGB", ImGuiColorEditFlags_InputRGB).value("InputHSV", ImGuiColorEditFlags_InputHSV)
    BIND_FLAGS_END(ImGuiColorEditFlags_);

    BIND_FLAGS_ENUM("ImGuiCond", ImGuiCond_)
        .value("None", ImGuiCond_None).value("Always", ImGuiCond_Always).value("Once", ImGuiCond_Once)
        .value("FirstUseEver", ImGuiCond_FirstUseEver).value("Appearing", ImGuiCond_Appearing)
    BIND_FLAGS_END(ImGuiCond_);

    py::enum_<ImGuiMouseButton_>(m, "MouseButton")
        .value("Left", ImGuiMouseButton_Left).value("Right", ImGuiMouseButton_Right)
        .value("Middle", ImGuiMouseButton_Middle).value("Count", ImGuiMouseButton_COUNT).export_values();

    py::enum_<ImGuiMouseCursor_>(m, "MouseCursor")
        .value("None", ImGuiMouseCursor_None).value("Arrow", ImGuiMouseCursor_Arrow)
        .value("TextInput", ImGuiMouseCursor_TextInput).value("ResizeAll", ImGuiMouseCursor_ResizeAll)
        .value("ResizeNS", ImGuiMouseCursor_ResizeNS).value("ResizeEW", ImGuiMouseCursor_ResizeEW)
        .value("ResizeNESW", ImGuiMouseCursor_ResizeNESW).value("ResizeNWSE", ImGuiMouseCursor_ResizeNWSE)
        .value("Hand", ImGuiMouseCursor_Hand).value("NotAllowed", ImGuiMouseCursor_NotAllowed)
        .value("Count", ImGuiMouseCursor_COUNT).export_values();

    py::enum_<ImGuiCol_>(m, "ImGuiCol")
        .value("Text", ImGuiCol_Text).value("TextDisabled", ImGuiCol_TextDisabled)
        .value("WindowBg", ImGuiCol_WindowBg).value("ChildBg", ImGuiCol_ChildBg).value("PopupBg", ImGuiCol_PopupBg)
        .value("Border", ImGuiCol_Border).value("BorderShadow", ImGuiCol_BorderShadow)
        .value("FrameBg", ImGuiCol_FrameBg).value("FrameBgHovered", ImGuiCol_FrameBgHovered).value("FrameBgActive", ImGuiCol_FrameBgActive)
        .value("TitleBg", ImGuiCol_TitleBg).value("TitleBgActive", ImGuiCol_TitleBgActive).value("TitleBgCollapsed", ImGuiCol_TitleBgCollapsed)
        .value("MenuBarBg", ImGuiCol_MenuBarBg).value("ScrollbarBg", ImGuiCol_ScrollbarBg)
        .value("ScrollbarGrab", ImGuiCol_ScrollbarGrab).value("ScrollbarGrabHovered", ImGuiCol_ScrollbarGrabHovered)
        .value("ScrollbarGrabActive", ImGuiCol_ScrollbarGrabActive).value("CheckMark", ImGuiCol_CheckMark)
        .value("SliderGrab", ImGuiCol_SliderGrab).value("SliderGrabActive", ImGuiCol_SliderGrabActive)
        .value("Button", ImGuiCol_Button).value("ButtonHovered", ImGuiCol_ButtonHovered).value("ButtonActive", ImGuiCol_ButtonActive)
        .value("Header", ImGuiCol_Header).value("HeaderHovered", ImGuiCol_HeaderHovered).value("HeaderActive", ImGuiCol_HeaderActive)
        .value("Separator", ImGuiCol_Separator).value("SeparatorHovered", ImGuiCol_SeparatorHovered).value("SeparatorActive", ImGuiCol_SeparatorActive)
        .value("ResizeGrip", ImGuiCol_ResizeGrip).value("ResizeGripHovered", ImGuiCol_ResizeGripHovered).value("ResizeGripActive", ImGuiCol_ResizeGripActive)
        .value("Tab", ImGuiCol_Tab).value("TabHovered", ImGuiCol_TabHovered).value("TabActive", ImGuiCol_TabActive)
        .value("TabUnfocused", ImGuiCol_TabUnfocused).value("TabUnfocusedActive", ImGuiCol_TabUnfocusedActive)
        .value("PlotLines", ImGuiCol_PlotLines).value("PlotLinesHovered", ImGuiCol_PlotLinesHovered)
        .value("PlotHistogram", ImGuiCol_PlotHistogram).value("PlotHistogramHovered", ImGuiCol_PlotHistogramHovered)
        .value("TableHeaderBg", ImGuiCol_TableHeaderBg).value("TableBorderStrong", ImGuiCol_TableBorderStrong)
        .value("TableBorderLight", ImGuiCol_TableBorderLight).value("TableRowBg", ImGuiCol_TableRowBg)
        .value("TableRowBgAlt", ImGuiCol_TableRowBgAlt).value("TextLink", ImGuiCol_TextLink)
        .value("TextSelectedBg", ImGuiCol_TextSelectedBg).value("DragDropTarget", ImGuiCol_DragDropTarget)
        .value("NavHighlight", ImGuiCol_NavHighlight).value("NavWindowingHighlight", ImGuiCol_NavWindowingHighlight)
        .value("NavWindowingDimBg", ImGuiCol_NavWindowingDimBg).value("ModalWindowDimBg", ImGuiCol_ModalWindowDimBg)
        .export_values();

    py::enum_<ImGuiStyleVar_>(m, "ImGuiStyleVar")
        .value("Alpha", ImGuiStyleVar_Alpha).value("DisabledAlpha", ImGuiStyleVar_DisabledAlpha)
        .value("WindowPadding", ImGuiStyleVar_WindowPadding).value("WindowRounding", ImGuiStyleVar_WindowRounding)
        .value("WindowBorderSize", ImGuiStyleVar_WindowBorderSize).value("WindowMinSize", ImGuiStyleVar_WindowMinSize)
        .value("WindowTitleAlign", ImGuiStyleVar_WindowTitleAlign).value("ChildRounding", ImGuiStyleVar_ChildRounding)
        .value("ChildBorderSize", ImGuiStyleVar_ChildBorderSize).value("PopupRounding", ImGuiStyleVar_PopupRounding)
        .value("PopupBorderSize", ImGuiStyleVar_PopupBorderSize).value("FramePadding", ImGuiStyleVar_FramePadding)
        .value("FrameRounding", ImGuiStyleVar_FrameRounding).value("FrameBorderSize", ImGuiStyleVar_FrameBorderSize)
        .value("ItemSpacing", ImGuiStyleVar_ItemSpacing).value("ItemInnerSpacing", ImGuiStyleVar_ItemInnerSpacing)
        .value("IndentSpacing", ImGuiStyleVar_IndentSpacing).value("CellPadding", ImGuiStyleVar_CellPadding)
        .value("ScrollbarSize", ImGuiStyleVar_ScrollbarSize).value("ScrollbarRounding", ImGuiStyleVar_ScrollbarRounding)
        .value("GrabMinSize", ImGuiStyleVar_GrabMinSize).value("GrabRounding", ImGuiStyleVar_GrabRounding)
        .value("ImageRounding", ImGuiStyleVar_ImageRounding).value("ImageBorderSize", ImGuiStyleVar_ImageBorderSize)
        .value("TabRounding", ImGuiStyleVar_TabRounding).value("TabBorderSize", ImGuiStyleVar_TabBorderSize)
        .value("TabBarBorderSize", ImGuiStyleVar_TabBarBorderSize).value("TabBarOverlineSize", ImGuiStyleVar_TabBarOverlineSize)
        .value("TableAngledHeadersAngle", ImGuiStyleVar_TableAngledHeadersAngle).value("TableAngledHeadersTextAlign", ImGuiStyleVar_TableAngledHeadersTextAlign)
        .value("ButtonTextAlign", ImGuiStyleVar_ButtonTextAlign).value("SelectableTextAlign", ImGuiStyleVar_SelectableTextAlign)
        .value("SeparatorTextBorderSize", ImGuiStyleVar_SeparatorTextBorderSize).value("SeparatorTextAlign", ImGuiStyleVar_SeparatorTextAlign)
        .value("SeparatorTextPadding", ImGuiStyleVar_SeparatorTextPadding).export_values();

    // Legacy-name aliases: the migrated Py4GWCoreLib references the legacy
    // PyImGui spellings for these; keep both names pointing at the same types.
    m.attr("ImGuiComboFlags") = m.attr("ComboFlags");
    m.attr("ImGuiWindowFlags_AlwaysAutoResize") = m.attr("WindowFlags").attr("AlwaysAutoResize");
}

}  // namespace PY4GW::imgui_bindings
