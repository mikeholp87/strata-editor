#pragma once
#include <QJsonObject>
#include <QString>
namespace Persistence {
QJsonObject read(const QString &path);
bool write(const QString &path, const QJsonObject &value);
QString dataPath(const QString &name);
} // namespace Persistence
