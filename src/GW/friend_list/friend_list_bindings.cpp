#include "base/py_bindings.h"

#include "GW/friend_list/friend_list.h"
#include "GW/game_thread/game_thread.h"

#include <string>

namespace py = pybind11;

namespace {

std::wstring StrToWide(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

} // namespace

PYBIND11_EMBEDDED_MODULE(PyFriendList, m) {
    m.doc() = "Py4GW FriendList bindings";

    m.def("get_number_of_friends", [](uint32_t friend_type) -> uint32_t {
        return GW::friend_list::GetNumberOfFriends(
            static_cast<GW::Constants::FriendType>(friend_type));
    }, py::arg("friend_type") = 1);

    m.def("get_number_of_ignores", []() -> uint32_t {
        return GW::friend_list::GetNumberOfIgnores();
    });

    m.def("get_number_of_partners", []() -> uint32_t {
        return GW::friend_list::GetNumberOfPartners();
    });

    m.def("get_number_of_traders", []() -> uint32_t {
        return GW::friend_list::GetNumberOfTraders();
    });

    m.def("get_my_status", []() -> uint32_t {
        return static_cast<uint32_t>(GW::friend_list::GetMyStatus());
    });

    m.def("set_friend_list_status", [](uint32_t status) -> bool {
        GW::game_thread::Enqueue([status]() {
            GW::friend_list::SetFriendListStatus(
                static_cast<GW::Constants::FriendStatus>(status));
        });
        return true;
    }, py::arg("status"));

    m.def("add_friend", [](const std::string& name, const std::string& alias) -> bool {
        auto wname = StrToWide(name);
        auto walias = StrToWide(alias);
        GW::game_thread::Enqueue([wname, walias, alias]() {
            GW::friend_list::AddFriend(wname.c_str(), alias.empty() ? nullptr : walias.c_str());
        });
        return true;
    }, py::arg("name"), py::arg("alias") = "");

    m.def("add_ignore", [](const std::string& name, const std::string& alias) -> bool {
        auto wname = StrToWide(name);
        auto walias = StrToWide(alias);
        GW::game_thread::Enqueue([wname, walias, alias]() {
            GW::friend_list::AddIgnore(wname.c_str(), alias.empty() ? nullptr : walias.c_str());
        });
        return true;
    }, py::arg("name"), py::arg("alias") = "");
}
