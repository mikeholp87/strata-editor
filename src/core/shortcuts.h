#pragma once
#include <QKeySequence>
#include <QList>
#include <QMap>
class QAction;
class QSettings;
namespace Shortcuts {
using Mapping = QMap<QString, QKeySequence>;
Mapping defaults(const QList<QAction *> &actions);
Mapping current(const QList<QAction *> &actions);
bool validate(const QList<QAction *> &actions, const Mapping &map, QString *error);
QByteArray encode(const Mapping &map);
bool decode(const QByteArray &bytes, const QList<QAction *> &actions, Mapping *map, QString *error);
bool importFile(const QString &path, const QList<QAction *> &actions, Mapping *map, QString *error);
bool exportFile(const QString &path, const QList<QAction *> &actions, const Mapping &map, QString *error);
bool apply(QSettings &settings, const QList<QAction *> &actions, const Mapping &map, QString *error);
bool load(QSettings &settings, const QList<QAction *> &actions, QString *error);
} // namespace Shortcuts
