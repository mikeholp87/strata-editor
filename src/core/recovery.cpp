#include "recovery.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUuid>

Persistence::ReadResult Recovery::load(const QString &path) {
    auto result = Persistence::readChecked(path);
    if (result.missing || !result.error.isEmpty())
        return result;
    bool valid = result.value["buffers"].isArray();
    for (const auto entry : result.value["buffers"].toArray()) {
        const auto o = entry.toObject();
        const auto mode = o["mode"].toInt(-1);
        static const QRegularExpression hash("^(?:[0-9a-fA-F]{64})?$");
        valid &=
            entry.isObject() && o["path"].isString() && o["text"].isString() && mode >= 0 && mode <= 2 &&
            QStringList{"UTF-8", "UTF-8 BOM", "UTF-16 LE", "UTF-16 BE"}.contains(o["encoding"].toString()) &&
            QStringList{"LF", "CRLF", "CR"}.contains(o["newline"].toString()) && o["digest"].isString() &&
            hash.match(o["digest"].toString()).hasMatch();
    }
    if (!valid) {
        result.value = {};
        result.error = QStringLiteral("Invalid recovery snapshot structure: %1").arg(path);
    }
    return result;
}
QStringList Recovery::backupPaths(const QString &path) {
    return {path + ".bak.1", path + ".bak.2", path + ".bak.3"};
}
bool Recovery::save(const QString &path, const QJsonObject &value, QString *error) {
    const auto current = load(path);
    if (!current.error.isEmpty()) {
        *error = current.error;
        return false;
    }
    auto next = value;
    next["version"] = 1;
    if (current.value == next)
        return true;
    QList<QJsonObject> history;
    if (!current.value["buffers"].toArray().isEmpty())
        history.append(current.value);
    const auto paths = backupPaths(path);
    for (const auto &backup : paths) {
        const auto old = load(backup);
        if (old.error.isEmpty() && !old.value["buffers"].toArray().isEmpty() && !history.contains(old.value))
            history.append(old.value);
    }
    // Write oldest first; never replace the primary if any backup write fails.
    for (int i = qMin(3, int(history.size())) - 1; i >= 0; --i)
        if (!Persistence::write(paths[i], history[i], error))
            return false;
    return Persistence::write(path, next, error);
}
QString Recovery::preserveInvalid(const QString &path, QString *error) {
    const QFileInfo info(path);
    if (!info.isFile() && !info.isSymLink()) {
        *error = QStringLiteral("Cannot preserve non-file recovery path: %1").arg(path);
        return {};
    }
    const auto preserved = path + ".corrupt." + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QFile file(path);
    if (!file.rename(preserved)) {
        *error = file.errorString();
        return {};
    }
    return preserved;
}
