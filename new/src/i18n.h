#pragma once
#include "common.h"
#include <string>
#include <string_view>

// Mirrors injector.exe's nl::i18n module:
//   nl::i18n::T(const char*)   — returns the Russian translation when the
//                                 russian flag is set, otherwise the original.
//   nl::i18n::Ago(int, bool)   — "Today" / "Yesterday" / "N days ago" / ...
namespace nl::i18n {

void        SetLanguage(std::string_view lang);
bool        Russian();
const char* T(const char* s);

// `days` = full days between now and the timestamp.
std::string Ago(int days, bool russian);
// Convenience: ts==0 -> "Never"; else Ago(daysSince(ts)).
std::string AgoFromTs(int64_t unix_ts, bool russian);
// Subscription line: days==0 -> "Lifetime"; else "N days".
std::string Subscription(int days, bool russian);
}  // namespace nl::i18n
