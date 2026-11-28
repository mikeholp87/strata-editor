#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>

// Hands file paths from a second launch to the running instance through small request files in a
// private directory, so no extra Qt module or socket permissions are needed.
class InstanceBridge : public QObject {
    Q_OBJECT
  public:
    explicit InstanceBridge(const QString &directory, QObject *parent = nullptr);
    // Queues `paths` (made absolute) for the running instance. An empty list just asks it to show itself.
    static bool send(const QString &directory, const QStringList &paths, QString *error);
  signals:
    void requested(const QStringList &paths);

  private:
    void scan();
    QString directory;
    QFileSystemWatcher watcher;
};
