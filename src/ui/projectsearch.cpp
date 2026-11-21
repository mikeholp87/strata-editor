#include "projectsearch.h"
#include <QDirIterator>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>
ProjectSearch::ProjectSearch(QWidget *p) : QWidget(p) {
    auto l = new QVBoxLayout(this);
    query = new QLineEdit;
    query->setPlaceholderText(tr("Literal text • case insensitive"));
    l->addWidget(query);
    auto run = new QPushButton(tr("Search project"));
    l->addWidget(run);
    auto cancel = new QPushButton(tr("Cancel"));
    l->addWidget(cancel);
    results = new QTreeWidget;
    results->setHeaderLabels({tr("File / line"), tr("Match (first 1,000 results)")});
    l->addWidget(results);
    connect(cancel, &QPushButton::clicked, this, [this] {
        if (cancellation)
            *cancellation = true;
    });
    auto start = [this, run] {
        if (root.isEmpty() || query->text().isEmpty() || watcher.isRunning())
            return;
        results->clear();
        cancellation = std::make_shared<std::atomic_bool>(false);
        run->setEnabled(false);
        watcher.setFuture(QtConcurrent::run(&ProjectSearch::search, root, query->text(), cancellation));
    };
    connect(run, &QPushButton::clicked, this, start);
    connect(query, &QLineEdit::returnPressed, this, start);
    connect(&watcher, &QFutureWatcher<QList<SearchHit>>::finished, this, [this, run] {
        run->setEnabled(true);
        for (const auto &hit : watcher.result()) {
            auto item = new QTreeWidgetItem(results, {hit.path + ":" + QString::number(hit.line), hit.text});
            item->setData(0, Qt::UserRole, hit.path);
            item->setData(0, Qt::UserRole + 1, hit.line);
        }
    });
    connect(results, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *i, int) {
        emit openHit(i->data(0, Qt::UserRole).toString(), i->data(0, Qt::UserRole + 1).toInt());
    });
}
ProjectSearch::~ProjectSearch() {
    if (cancellation)
        *cancellation = true;
    watcher.waitForFinished();
}
QList<SearchHit> ProjectSearch::search(const QString &root, const QString &query,
                                       const std::shared_ptr<std::atomic_bool> &cancel) {
    QList<SearchHit> hits;
    if (query.isEmpty())
        return hits;
    QDirIterator it(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext() && !*cancel && hits.size() < 1000) {
        auto path = it.next();
        if (path.contains("/.git/") || path.contains("/node_modules/") || path.contains("/build/"))
            continue;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly) || f.size() > 2 * 1024 * 1024)
            continue;
        auto bytes = f.readAll();
        if (bytes.contains(char(0)))
            continue;
        const auto lines = QString::fromUtf8(bytes).split('\n');
        for (int i = 0; i < lines.size() && hits.size() < 1000 && !*cancel; ++i)
            if (lines[i].contains(query, Qt::CaseInsensitive))
                hits.append({path, lines[i].left(500), i + 1});
    }
    return hits;
}
