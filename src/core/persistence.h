#pragma once
#include <QJsonObject>
#include <QString>
namespace Persistence {
struct ReadResult {
    QJsonObject value;
    QString error;
    bool missing = false;
};
ReadResult readChecked(const QString &path);
QJsonObject read(const QString &path);
bool write(const QString &path, const QJsonObject &value, QString *error = nullptr);
QString dataPath(const QString &name);
} // namespace Persistence
