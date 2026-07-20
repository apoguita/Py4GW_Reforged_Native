#include "base/py_bindings.h"

#include "GW/chat/chat.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(PyChat, m) {
    m.doc() = "Py4GW Chat bindings";

    m.def("force_redraw_chat_log", []() {
        GW::chat::ForceRedrawChatLog();
    });

    m.def("get_is_typing", []() -> bool {
        return GW::chat::GetIsTyping();
    });

    m.def("send_chat", [](int channel, const std::string& msg) {
        return GW::chat::SendChat(static_cast<char>(channel), msg.c_str());
    }, py::arg("channel"), py::arg("msg"));

    m.def("send_chat_by_name", [](const std::string& from, const std::string& msg) {
        return GW::chat::SendChat(from.c_str(), msg.c_str());
    }, py::arg("from"), py::arg("msg"));

    m.def("write_chat", [](int channel, const std::string& message) {
        std::wstring wmsg(message.begin(), message.end());
        GW::chat::WriteChat(static_cast<GW::chat::Channel>(channel), wmsg.c_str());
    }, py::arg("channel"), py::arg("message"));

    m.def("write_chat_ex", [](int channel, const std::string& message, const std::string& sender) {
        std::wstring wmsg(message.begin(), message.end());
        std::wstring wsender(sender.begin(), sender.end());
        GW::chat::WriteChat(static_cast<GW::chat::Channel>(channel), wmsg.c_str(), wsender.c_str(), false);
    }, py::arg("channel"), py::arg("message"), py::arg("sender"));

    m.def("toggle_timestamps", [](bool enable) {
        GW::chat::ToggleTimestamps(enable);
    }, py::arg("enable"));

    m.def("set_timestamps_format", [](bool use_24h, bool show_seconds) {
        GW::chat::SetTimestampsFormat(use_24h, show_seconds);
    }, py::arg("use_24h"), py::arg("show_seconds") = false);

    m.def("set_timestamps_color", [](uint32_t r, uint32_t g, uint32_t b) {
        GW::chat::SetTimestampsColor(GW::chat::MakeColorRGB(
            static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)));
    }, py::arg("r"), py::arg("g"), py::arg("b"));

    m.def("set_sender_color", [](int channel, uint32_t r, uint32_t g, uint32_t b) {
        GW::chat::SetSenderColor(static_cast<GW::chat::Channel>(channel),
            GW::chat::MakeColorRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)));
    }, py::arg("channel"), py::arg("r"), py::arg("g"), py::arg("b"));

    m.def("set_message_color", [](int channel, uint32_t r, uint32_t g, uint32_t b) {
        GW::chat::SetMessageColor(static_cast<GW::chat::Channel>(channel),
            GW::chat::MakeColorRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)));
    }, py::arg("channel"), py::arg("r"), py::arg("g"), py::arg("b"));

    m.def("send_fake_chat", [](int channel, const std::string& message) {
        GW::chat::SendFakeChat(channel, message);
    }, py::arg("channel"), py::arg("message"));

    m.def("send_fake_chat_colored", [](int channel, const std::string& message, int r, int g, int b) {
        GW::chat::SendFakeChatColored(channel, message, r, g, b);
    }, py::arg("channel"), py::arg("message"), py::arg("r"), py::arg("g"), py::arg("b"));
}
