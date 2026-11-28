#pragma once
#include <QString>
#include <QStringList>
namespace Completion {
// Only this many characters either side of the word being typed are scanned.
constexpr int window = 128 * 1024;
// Words of 3+ characters from `text` outside [from, to) plus the language's keywords,
// de-duplicated and sorted case-insensitively. [from, to) is the word being typed.
QStringList candidates(const QString &text, int from, int to, const QString &language);
} // namespace Completion
