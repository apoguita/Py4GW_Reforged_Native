#include "base/py_bindings.h"

#include "GW/dialog/dialog.h"

namespace py = pybind11;

using GW::dialog::ActiveDialogInfo;
using GW::dialog::DialogButtonInfo;
using GW::dialog::DialogCallbackJournalEntry;
using GW::dialog::DialogEventLog;
using GW::dialog::DialogInfo;
using GW::dialog::DialogTextDecodedInfo;

void BindDialogInfo(py::module_& m) {
    py::class_<DialogInfo>(m, "DialogInfo")
        .def(py::init<>())
        .def_readwrite("dialog_id", &DialogInfo::dialog_id)
        .def_readwrite("flags", &DialogInfo::flags)
        .def_readwrite("frame_type", &DialogInfo::frame_type)
        .def_readwrite("event_handler", &DialogInfo::event_handler)
        .def_readwrite("content_id", &DialogInfo::content_id)
        .def_readwrite("property_id", &DialogInfo::property_id)
        .def_readwrite("content", &DialogInfo::content)
        .def_readwrite("agent_id", &DialogInfo::agent_id);
}

void BindActiveDialogInfo(py::module_& m) {
    py::class_<ActiveDialogInfo>(m, "ActiveDialogInfo")
        .def(py::init<>())
        .def_readwrite("dialog_id", &ActiveDialogInfo::dialog_id)
        .def_readwrite("context_dialog_id", &ActiveDialogInfo::context_dialog_id)
        .def_readwrite("agent_id", &ActiveDialogInfo::agent_id)
        .def_readwrite("dialog_id_authoritative", &ActiveDialogInfo::dialog_id_authoritative)
        .def_readwrite("message", &ActiveDialogInfo::message);
}

void BindDialogButtonInfo(py::module_& m) {
    py::class_<DialogButtonInfo>(m, "DialogButtonInfo")
        .def(py::init<>())
        .def_readwrite("dialog_id", &DialogButtonInfo::dialog_id)
        .def_readwrite("button_icon", &DialogButtonInfo::button_icon)
        .def_readwrite("message", &DialogButtonInfo::message)
        .def_readwrite("message_decoded", &DialogButtonInfo::message_decoded)
        .def_readwrite("message_decode_pending", &DialogButtonInfo::message_decode_pending);
}

void BindDialogTextDecodedInfo(py::module_& m) {
    py::class_<DialogTextDecodedInfo>(m, "DialogTextDecodedInfo")
        .def(py::init<>())
        .def_readwrite("dialog_id", &DialogTextDecodedInfo::dialog_id)
        .def_readwrite("text", &DialogTextDecodedInfo::text)
        .def_readwrite("pending", &DialogTextDecodedInfo::pending);
}

void BindDialogEventLog(py::module_& m) {
    py::class_<DialogEventLog>(m, "DialogEventLog")
        .def(py::init<>())
        .def_readwrite("tick", &DialogEventLog::tick)
        .def_readwrite("message_id", &DialogEventLog::message_id)
        .def_readwrite("incoming", &DialogEventLog::incoming)
        .def_readwrite("is_frame_message", &DialogEventLog::is_frame_message)
        .def_readwrite("frame_id", &DialogEventLog::frame_id)
        .def_readwrite("w_bytes", &DialogEventLog::w_bytes)
        .def_readwrite("l_bytes", &DialogEventLog::l_bytes);
}

void BindDialogCallbackJournalEntry(py::module_& m) {
    py::class_<DialogCallbackJournalEntry>(m, "DialogCallbackJournalEntry")
        .def(py::init<>())
        .def_readwrite("tick", &DialogCallbackJournalEntry::tick)
        .def_readwrite("message_id", &DialogCallbackJournalEntry::message_id)
        .def_readwrite("incoming", &DialogCallbackJournalEntry::incoming)
        .def_readwrite("dialog_id", &DialogCallbackJournalEntry::dialog_id)
        .def_readwrite("context_dialog_id", &DialogCallbackJournalEntry::context_dialog_id)
        .def_readwrite("agent_id", &DialogCallbackJournalEntry::agent_id)
        .def_readwrite("map_id", &DialogCallbackJournalEntry::map_id)
        .def_readwrite("model_id", &DialogCallbackJournalEntry::model_id)
        .def_readwrite("dialog_id_authoritative", &DialogCallbackJournalEntry::dialog_id_authoritative)
        .def_readwrite("context_dialog_id_inferred", &DialogCallbackJournalEntry::context_dialog_id_inferred)
        .def_readwrite("npc_uid", &DialogCallbackJournalEntry::npc_uid)
        .def_readwrite("event_type", &DialogCallbackJournalEntry::event_type)
        .def_readwrite("text", &DialogCallbackJournalEntry::text);
}

// Single merged Python module: legacy PyDialog surface plus the catalog-only
// read_dialog_* accessors (legacy PyDialogCatalog module was retired; its
// remaining functions all had PyDialog equivalents).
namespace {

struct PyDialogShim {};

}  // namespace

PYBIND11_EMBEDDED_MODULE(PyDialog, m) {
    BindDialogInfo(m);
    BindActiveDialogInfo(m);
    BindDialogButtonInfo(m);
    BindDialogTextDecodedInfo(m);
    BindDialogEventLog(m);
    BindDialogCallbackJournalEntry(m);
    py::class_<PyDialogShim>(m, "PyDialog")
        .def(py::init<>())
        .def_static("is_dialog_available", &GW::dialog::IsDialogAvailable, py::arg("dialog_id"))
        .def_static("get_dialog_info", &GW::dialog::GetDialogInfo, py::arg("dialog_id"))
        .def_static("get_last_selected_dialog_id", &GW::dialog::GetLastSelectedDialogId)
        .def_static("get_active_dialog", &GW::dialog::GetActiveDialog)
        .def_static("get_active_dialog_buttons", &GW::dialog::GetActiveDialogButtons)
        .def_static("is_dialog_active", &GW::dialog::IsDialogActive)
        .def_static("is_dialog_displayed", &GW::dialog::IsDialogDisplayed, py::arg("dialog_id"))
        .def_static("enumerate_available_dialogs", &GW::dialog::EnumerateAvailableDialogs)
        .def_static("get_dialog_text_decoded", &GW::dialog::GetDialogTextDecoded, py::arg("dialog_id"))
        .def_static("is_dialog_text_decode_pending", &GW::dialog::IsDialogTextDecodePending, py::arg("dialog_id"))
        .def_static("get_dialog_text_decode_status", &GW::dialog::GetDecodedDialogTextStatus)
        .def_static("read_dialog_flags", &GW::dialog::ReadDialogFlags, py::arg("dialog_id"))
        .def_static("read_dialog_frame_type", &GW::dialog::ReadDialogFrameType, py::arg("dialog_id"))
        .def_static("read_dialog_event_handler", &GW::dialog::ReadDialogEventHandler, py::arg("dialog_id"))
        .def_static("read_dialog_content_id", &GW::dialog::ReadDialogContentId, py::arg("dialog_id"))
        .def_static("read_dialog_property_id", &GW::dialog::ReadDialogPropertyId, py::arg("dialog_id"))
        .def_static("get_dialog_event_logs", &GW::dialog::GetDialogEventLogs)
        .def_static("get_dialog_event_logs_received", &GW::dialog::GetDialogEventLogsReceived)
        .def_static("get_dialog_event_logs_sent", &GW::dialog::GetDialogEventLogsSent)
        .def_static("clear_dialog_event_logs", &GW::dialog::ClearDialogEventLogs)
        .def_static("clear_dialog_event_logs_received", &GW::dialog::ClearDialogEventLogsReceived)
        .def_static("clear_dialog_event_logs_sent", &GW::dialog::ClearDialogEventLogsSent)
        .def_static("get_dialog_callback_journal", &GW::dialog::GetDialogCallbackJournal)
        .def_static("get_dialog_callback_journal_received", &GW::dialog::GetDialogCallbackJournalReceived)
        .def_static("get_dialog_callback_journal_sent", &GW::dialog::GetDialogCallbackJournalSent)
        .def_static("clear_dialog_callback_journal", &GW::dialog::ClearDialogCallbackJournal)
        .def_static("clear_dialog_callback_journal_received", &GW::dialog::ClearDialogCallbackJournalReceived)
        .def_static("clear_dialog_callback_journal_sent", &GW::dialog::ClearDialogCallbackJournalSent)
        .def_static("clear_dialog_callback_journal_filtered", &GW::dialog::ClearDialogCallbackJournalFiltered,
            py::arg("npc_uid") = std::nullopt,
            py::arg("incoming") = std::nullopt,
            py::arg("message_id") = std::nullopt,
            py::arg("event_type") = std::nullopt)
        .def_static("clear_cache", &GW::dialog::ClearCache)
        .def_static("initialize", &GW::dialog::Initialize)
        .def_static("terminate", &GW::dialog::Shutdown);
}
