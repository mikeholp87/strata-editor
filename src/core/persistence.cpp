#include "persistence.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
QJsonObject Persistence::read(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 64 * 1024 * 1024)
        return {};
    auto obj = QJsonDocument::fromJson(f.readAll()).object();
    return obj["version"].toInt() == 1 ? obj : QJsonObject{};
}
bool Persistence::write(const QString &path, const QJsonObject &value) {
    auto o = value;
    o["version"] = 1;
    auto bytes = QJsonDocument(o).toJson();
    if (bytes.size() > 64 * 1024 * 1024)
        return false;
    QSaveFile f(path);
    f.setDirectWriteFallback(false);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(bytes) == bytes.size() && f.commit();
}
QString Persistence::dataPath(const QString &name) {
    auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    return path + "/" + name;
}
