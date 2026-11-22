#include "projectsearch.h"
#include <QDirIterator>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QStringConverter>
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
        searchRevision = rootRevision;
        cancellation = std::make_shared<std::atomic_bool>(false);
        run->setEnabled(false);
        watcher.setFuture(QtConcurrent::run(&ProjectSearch::search, root, query->text(), cancellation));
    };
    connect(run, &QPushButton::clicked, this, start);
    connect(query, &QLineEdit::returnPressed, this, start);
    connect(&watcher, &QFutureWatcher<QList<SearchHit>>::finished, this, [this, run] {
        run->setEnabled(true);
        if (searchRevision != rootRevision)
            return;
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
void ProjectSearch::setRoot(const QString &path) {
    if (root == path)
        return;
    if (cancellation)
        *cancellation = true;
    root = path;
    ++rootRevision;
    results->clear();
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
    QStringList directories{root};
    while (!directories.isEmpty() && !*cancel && hits.size() < 1000) {
        QDirIterator it(directories.takeLast(), QDir::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        while (it.hasNext() && !*cancel && hits.size() < 1000) {
            const auto path = it.next();
            const auto info = it.fileInfo();
            if (info.isDir()) {
                const auto name = info.fileName();
                if (name != ".git" && name != "node_modules" && name != "build")
                    directories.append(path);
                continue;
            }
            if (!info.isFile())
                continue;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly) || f.size() > 2 * 1024 * 1024)
                continue;
            auto bytes = f.read(2 * 1024 * 1024 + 1);
            if (f.error() != QFileDevice::NoError || bytes.size() > 2 * 1024 * 1024)
                continue;
            QString text;
            if (bytes.startsWith("\xff\xfe") || bytes.startsWith("\xfe\xff")) {
                const bool le = bytes.startsWith("\xff\xfe");
                bytes.remove(0, 2);
                if (bytes.size() % 2)
                    continue;
                QStringDecoder decoder(le ? QStringConverter::Utf16LE : QStringConverter::Utf16BE,
                                       QStringConverter::Flag::Stateless |
                                           QStringConverter::Flag::ConvertInitialBom);
                text = decoder(bytes);
                if (decoder.hasError() || text.contains(QChar(0)))
                    continue;
            } else {
                if (bytes.contains(char(0)) || !bytes.isValidUtf8())
                    continue;
                text = QString::fromUtf8(bytes);
            }
            const auto lines = text.split('\n');
            for (int i = 0; i < lines.size() && hits.size() < 1000 && !*cancel; ++i)
                if (lines[i].contains(query, Qt::CaseInsensitive))
                    hits.append({path, lines[i].left(500), i + 1});
        }
    }
    return hits;
}
