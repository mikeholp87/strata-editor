#include "instancebridge.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>

static constexpr qint64 maxRequestBytes = 256 * 1024;
static constexpr qint64 maxRequestAgeSeconds = 60;

InstanceBridge::InstanceBridge(const QString &dir, QObject *parent) : QObject(parent), directory(dir) {
    QDir().mkpath(directory);
    watcher.addPath(directory);
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [this] { scan(); });
    scan(); // a request may have landed between the lock being taken and the watch starting
}
bool InstanceBridge::send(const QString &directory, const QStringList &paths, QString *error) {
    if (!QDir().mkpath(directory)) {
        *error = QObject::tr("Cannot create %1").arg(directory);
        return false;
    }
    QJsonArray list;
    for (const auto &path : paths)
        list.append(QFileInfo(path).absoluteFilePath());
    QSaveFile file(directory + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".request");
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(list).toJson()) < 0 || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}
void InstanceBridge::scan() {
    const auto files = QDir(directory).entryInfoList({"*.request"}, QDir::Files, QDir::Time | QDir::Reversed);
    for (const auto &info : files) {
        QStringList paths;
        const bool fresh = info.lastModified().secsTo(QDateTime::currentDateTime()) <= maxRequestAgeSeconds;
        QFile file(info.absoluteFilePath());
        if (fresh && info.size() <= maxRequestBytes && file.open(QIODevice::ReadOnly))
            for (const auto &value : QJsonDocument::fromJson(file.readAll()).array())
                if (value.isString() && QFileInfo(value.toString()).isAbsolute())
                    paths.append(value.toString());
        file.close();
        if (!QFile::remove(info.absoluteFilePath()))
            continue; // do not act on a request that cannot be consumed; it would repeat on every scan
        if (fresh)
            emit requested(paths);
    }
}
