#pragma once
#include <QFutureWatcher>
#include <QWidget>
#include <atomic>
#include <memory>
class QLineEdit;
class QTreeWidget;
struct SearchHit {
    QString path, text;
    int line;
};
class ProjectSearch : public QWidget {
    Q_OBJECT
  public:
    explicit ProjectSearch(QWidget *parent = nullptr);
    ~ProjectSearch();
    QString root;
    static QList<SearchHit> search(const QString &root, const QString &query,
                                   const std::shared_ptr<std::atomic_bool> &cancel);
  signals:
    void openHit(QString path, int line);

  private:
    QLineEdit *query;
    QTreeWidget *results;
    QFutureWatcher<QList<SearchHit>> watcher;
    std::shared_ptr<std::atomic_bool> cancellation;
};
