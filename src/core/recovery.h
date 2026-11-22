#pragma once
#include "persistence.h"
#include <QStringList>
namespace Recovery {
Persistence::ReadResult load(const QString &path);
QStringList backupPaths(const QString &path);
bool save(const QString &path, const QJsonObject &value, QString *error);
QString preserveInvalid(const QString &path, QString *error);
} // namespace Recovery
