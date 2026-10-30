#pragma once

#include <QString>

// Two interface languages. Texts are written in Russian in the code and looked up in an English
// table; the SDK has no Qt translation tools, and two languages do not need them.
namespace I18n {

void setLanguage(const QString &code); // "ru" or "en"; anything else counts as Russian
bool english();
// The firmware's language code ("ru", "en", "de", …) to ours: Russian stays Russian, the rest
// gets English; no code at all keeps Russian.
QString fromFirmware(const QString &firmwareCode);
// English for a Russian text, empty when the table has none.
QString english(const QString &russian);

} // namespace I18n

// The text in the current language (the Russian original when there is no translation).
QString tx(const char *russian);
// Russian plural forms (1 статья, 3 статьи, 5 статей); English uses the table entries for
// `one` and `many`.
QString txn(int n, const char *one, const char *few, const char *many);
