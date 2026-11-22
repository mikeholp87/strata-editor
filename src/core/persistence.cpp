#include "persistence.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
Persistence::ReadResult Persistence::readChecked(const QString &path) {
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return {{}, {}, true};
    if (!info.isFile())
        return {{}, QStringLiteral("Not a regular JSON file: %1").arg(path)};
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {{}, f.errorString()};
    const auto bytes = f.read(64 * 1024 * 1024 + 1);
    if (f.error() != QFileDevice::NoError)
        return {{}, f.errorString()};
    if (bytes.size() > 64 * 1024 * 1024)
        return {{}, QStringLiteral("JSON file exceeds 64 MiB: %1").arg(path)};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError)
        return {{}, error.errorString()};
    if (!document.isObject() || document.object()["version"].toInt() != 1)
        return {{}, QStringLiteral("Unsupported JSON structure or version: %1").arg(path)};
    return {document.object(), {}};
}
QJsonObject Persistence::read(const QString &path) {
    return readChecked(path).value;
}
bool Persistence::write(const QString &path, const QJsonObject &value, QString *error) {
    auto o = value;
    o["version"] = 1;
    const auto bytes = QJsonDocument(o).toJson();
    if (bytes.size() > 64 * 1024 * 1024) {
        if (error)
            *error = QStringLiteral("Snapshot exceeds 64 MiB.");
        return false;
    }
    QSaveFile f(path);
    f.setDirectWriteFallback(false);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, f.errorString());
        return false;
    }
    return true;
}
QString Persistence::dataPath(const QString &name) {
    auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    return path + "/" + name;
}
