#include "i18n.h"
#include <cstring>
#include <cstddef>
#include <ctime>
#include <string>

// The EN->RU table (54 entries) extracted from injector.exe's nl::i18n::T::entries.
#include "i18n_table.inl"

namespace nl::i18n {

static bool g_russian = false;

void SetLanguage(std::string_view lang) {
    g_russian = (lang == "Russian" || lang == "russian" || lang == "ru" || lang == "RU");
}
bool Russian() { return g_russian; }

// Original did a linear scan: compare key length first, then the first bytes.
const char* T(const char* s) {
    if (!s || !g_russian) return s;
    const size_t len = std::strlen(s);
    for (size_t i = 0; i < kI18nEntryCount; ++i) {
        const I18nEntry& e = kI18nEntries[i];
        if (std::strlen(e.key) == len && std::memcmp(e.key, s, len) == 0)
            return e.val;
    }
    return s;
}

std::string Ago(int days, bool russian) {
    if (days <= 0) return T("Today");
    if (days == 1) return T("Yesterday");
    return russian ? (std::to_string(days) + " дн. назад")
                   : (std::to_string(days) + " days ago");
}

std::string AgoFromTs(int64_t unix_ts, bool russian) {
    if (unix_ts <= 0) return T("Never");
    const std::time_t now = std::time(nullptr);
    int days = (int)((now - (std::time_t)unix_ts) / 86400);
    if (days < 0) days = 0;
    return Ago(days, russian);
}

std::string Subscription(int days, bool russian) {
    if (days <= 0) return T("Lifetime");
    return russian ? (std::to_string(days) + " дн.")
                   : (std::to_string(days) + " days");
}

}  // namespace nl::i18n
