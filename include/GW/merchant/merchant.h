#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/common/constants/constants.h"
#include "GW/context/item.h"
#include "GW/ui/ui.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace GW::merchant {

using MerchItemArray = Context::MerchItemArray;

enum class TransactionType : uint32_t {
    MerchantBuy = 0x1,
    CollectorBuy,
    CrafterBuy,
    WeaponsmithCustomize,

    MerchantSell = 0xB,
    TraderBuy,
    TraderSell,

    UnlockRunePriestOfBalth = 0xF
};

struct TransactionInfo {
    uint32_t item_count = 0;
    uint32_t* item_ids = nullptr;
    uint32_t* item_quantities = nullptr;
};

struct QuoteInfo {
    uint32_t unknown = 0;
    uint32_t item_count = 0;
    uint32_t* item_ids = nullptr;
};

struct TransactItemPacket {
    TransactionType type;
    uint32_t gold_give;
    TransactionInfo give;
    uint32_t gold_recv;
    TransactionInfo recv;
};

struct RequestQuotePacket {
    TransactionType type;
    uint32_t unknown;
    QuoteInfo give;
    QuoteInfo recv;
};

using TransactItemFn = void(__cdecl*)(TransactionType type, uint32_t gold_give, TransactionInfo give, uint32_t gold_recv, TransactionInfo recv);
using RequestQuoteFn = void(__cdecl*)(TransactionType type, uint32_t unknown, QuoteInfo give, QuoteInfo recv);

bool Initialize();
void Shutdown();

bool TransactItems(TransactionType type, uint32_t gold_give, TransactionInfo give, uint32_t gold_recv, TransactionInfo recv);
bool RequestQuote(TransactionType type, QuoteInfo give, QuoteInfo recv);
MerchItemArray* GetMerchantItemsArray();

extern TransactItemFn g_transact_item_func;
extern TransactItemFn g_transact_item_original;
extern RequestQuoteFn g_request_quote_func;
extern RequestQuoteFn g_request_quote_original;
extern PY4GW::HookEntry g_ui_message_entry;
extern const std::array<ui::UIMessage, 2> g_ui_messages_to_hook;
extern std::atomic<bool> g_initialized;

inline bool ResolveTransactItemFunction() {
    CrashContextScope context("startup", "merchant", "resolve_transact_item");
    const auto* pattern = PY4GW::Patterns::Get("merchant.transact_item_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: merchant.transact_item_target", "merchant");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("TransactItem_Scan", address, "merchant")) {
        return false;
    }
    g_transact_item_func = reinterpret_cast<TransactItemFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("TransactItem_Func", reinterpret_cast<uintptr_t>(g_transact_item_func), "merchant");
}

inline bool ResolveRequestQuoteFunction() {
    CrashContextScope context("startup", "merchant", "resolve_request_quote");
    const auto* pattern = PY4GW::Patterns::Get("merchant.request_quote_target");
    if (!pattern) {
        Logger::Instance().LogError("Missing or invalid pattern: merchant.request_quote_target", "merchant");
        return false;
    }
    const uintptr_t address = PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
    if (!Logger::AssertAddress("RequestQuote_Scan", address, "merchant")) {
        return false;
    }
    g_request_quote_func = reinterpret_cast<RequestQuoteFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("RequestQuote_Func", reinterpret_cast<uintptr_t>(g_request_quote_func), "merchant");
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();

void __cdecl OnTransactItem(TransactionType type, uint32_t gold_give, TransactionInfo give, uint32_t gold_recv, TransactionInfo recv);
void __cdecl OnRequestQuote(TransactionType type, uint32_t unknown, QuoteInfo give, QuoteInfo recv);
void OnUIMessage(PY4GW::HookStatus* status, ui::UIMessage message_id, void* wparam, void* lparam);

namespace packet {
struct kVendorWindow {
    TransactionType transaction_type;
    uint32_t unk;
    uint32_t merchant_agent_id;
    uint32_t is_pending;
};

struct kVendorQuote {
    uint32_t item_id;
    uint32_t price;
};

struct kVendorItems {
    TransactionType transaction_type;
    uint32_t item_ids_count;
    uint32_t* item_ids_buffer1;
    uint32_t* item_ids_buffer2;
};

struct kSendMerchantRequestQuote {
    TransactionType type;
    uint32_t gold_give;
    TransactionInfo give;
    uint32_t gold_recv;
    TransactionInfo recv;
};

struct kSendMerchantTransactItem {
    TransactionType type;
    uint32_t h0004;
    QuoteInfo give;
    uint32_t gold_recv;
    QuoteInfo recv;
};
}  // namespace packet

}  // namespace GW::merchant

namespace GW {
namespace Merchants = merchant;
}
