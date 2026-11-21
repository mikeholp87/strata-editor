#include "mainwindow.h"
#include "core/persistence.h"
#include "core/shortcuts.h"
#include "documentview.h"
#include "editor/codeeditor.h"
#include "editor/highlighter.h"
#include "gitpanel.h"
#include "projectsearch.h"
#include <QJsonArray>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QtConcurrent>
#include <QtWidgets>

MainWindow::MainWindow(bool restore) : settings("StrataEditor", "StrataEditor") {
    setObjectName("mainWindow");
    setWindowTitle(STRATA_PRODUCT_NAME);
    resize(1180, 780);
    setMinimumSize(640, 420);
    setAcceptDrops(true);
    settings.setValue("schemaVersion", 1);
    tabs = new QTabWidget;
    tabs->setObjectName("documentTabs");
    tabs->setTabsClosable(true);
    tabs->setMovable(true);
    tabs->setDocumentMode(true);
    auto center = new QWidget;
    auto layout = new QVBoxLayout(center);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tabs);
    findBar = new QWidget;
    auto findLayout = new QHBoxLayout(findBar);
    findText = new QLineEdit;
    findText->setObjectName("findText");
    findText->setPlaceholderText(tr("Find"));
    replaceText = new QLineEdit;
    replaceText->setObjectName("replaceText");
    replaceText->setPlaceholderText(tr("Replace"));
    matchCase = new QCheckBox(tr("Aa"));
    wholeWord = new QCheckBox(tr("Word"));
    regex = new QCheckBox(tr("Regex"));
    for (auto w : QList<QWidget *>{findText, replaceText, matchCase, wholeWord, regex})
        findLayout->addWidget(w);
    auto findButton = [&](QString label, auto fn) {
        auto b = new QPushButton(label);
        findLayout->addWidget(b);
        connect(b, &QPushButton::clicked, this, fn);
    };
    findButton(tr("Next"), [this] { findNext(); });
    findButton(tr("Replace"), [this] { replace(); });
    findButton(tr("All"), [this] { replace(true); });
    findButton(tr("×"), [this] { findBar->hide(); });
    layout->addWidget(findBar);
    findBar->hide();
    setCentralWidget(center);
    connect(findText, &QLineEdit::returnPressed, this, [this] { findNext(); });
    explorerDock = new QDockWidget(tr("PROJECT"), this);
    explorerDock->setObjectName("explorerDock");
    files = new QFileSystemModel(this);
    files->setReadOnly(true);
    files->setRootPath(QDir::homePath());
    tree = new QTreeView;
    tree->setModel(files);
    tree->setHeaderHidden(true);
    for (int i = 1; i < 4; ++i)
        tree->hideColumn(i);
    tree->setRootIndex(files->index(QDir::homePath()));
    explorerDock->setWidget(tree);
    addDockWidget(Qt::LeftDockWidgetArea, explorerDock);
    connect(tree, &QTreeView::doubleClicked, this, [this](const QModelIndex &i) {
        if (!files->isDir(i))
            openFile(files->filePath(i));
    });
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree, &QTreeView::customContextMenuRequested, this, [this](QPoint pos) {
        auto index = tree->indexAt(pos);
        if (!index.isValid())
            return;
        auto path = files->filePath(index);
        QMenu m;
        m.addAction(tr("Open"), [this, path] {
            if (QFileInfo(path).isFile())
                openFile(path);
        });
        m.addAction(tr("Copy absolute path"), [path] { QApplication::clipboard()->setText(path); });
        m.addAction(tr("Reveal folder"), [path] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        });
        m.exec(tree->viewport()->mapToGlobal(pos));
    });
    projectSearch = new ProjectSearch;
    searchDock = new QDockWidget(tr("PROJECT SEARCH"), this);
    searchDock->setObjectName("searchDock");
    searchDock->setWidget(projectSearch);
    addDockWidget(Qt::BottomDockWidgetArea, searchDock);
    searchDock->hide();
    connect(projectSearch, &ProjectSearch::openHit, this, &MainWindow::openFile);
    git = new GitPanel;
    gitDock = new QDockWidget(tr("GIT"), this);
    gitDock->setObjectName("gitDock");
    gitDock->setWidget(git);
    addDockWidget(Qt::RightDockWidgetArea, gitDock);
    gitDock->hide();
    recent = settings.value("recentFiles").toStringList();
    buildCommands();
    buildFormatBar();
    QString shortcutError;
    if (!Shortcuts::load(settings, commands, &shortcutError)) {
        QTimer::singleShot(0, this, [this, shortcutError] {
            QMessageBox::warning(this, tr("Shortcut settings"),
                                 tr("Default shortcuts are active. %1").arg(shortcutError));
        });
    }
    updateTheme();
    connect(tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(tabs, &QTabWidget::currentChanged, this, &MainWindow::updateUi);
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        if (QFileInfo::exists(path))
            watcher.addPath(path);
        for (int i = 0; i < tabs->count(); ++i) {
            auto v = qobject_cast<DocumentView *>(tabs->widget(i));
            if (v->model->path() == path && v->model->changedOnDisk()) {
                tabs->setTabToolTip(i, tr("Changed externally. Reload or save a separate copy."));
                statusBar()->showMessage(
                    tr("File changed externally: %1 — reload or save a copy; your edits are preserved.")
                        .arg(path),
                    15000);
            }
        }
    });
    auto timer = new QTimer(this);
    timer->setInterval(15000);
    connect(timer, &QTimer::timeout, this, &MainWindow::recoverySnapshot);
    timer->start();
    if (restore) {
        restoreGeometry(settings.value("geometry").toByteArray());
        restoreState(settings.value("layout").toByteArray());
        restoreSession();
    }
    if (!tabs->count() && loading.isEmpty())
        newDocument();
    updateUi();
}
DocumentView *MainWindow::current() const {
    return qobject_cast<DocumentView *>(tabs->currentWidget());
}
QAction *MainWindow::command(const QString &id, const QString &title, const QKeySequence &key,
                             const std::function<void()> &fn, QMenu *menu) {
    auto a = new QAction(title, this);
    a->setObjectName(id);
    a->setShortcut(key);
    a->setProperty("defaultShortcut", QVariant::fromValue(key));
    connect(a, &QAction::triggered, this, fn);
    addAction(a);
    commands.append(a);
    if (menu)
        menu->addAction(a);
    return a;
}
DocumentView *MainWindow::attach(Document *doc) {
    auto view = new DocumentView(doc);
    tabs->addTab(view, doc->title());
    tabs->setCurrentWidget(view);
    auto syntax = new Highlighter(doc->text(), doc->mode() == Document::Mode::Markdown
                                                   ? "Markdown"
                                                   : Highlighter::languageForPath(doc->path()));
    Q_UNUSED(syntax);
    if (view->code)
        view->code->setProperty("language", Highlighter::languageForPath(doc->path()));
    connect(doc->text(), &QTextDocument::modificationChanged, this, &MainWindow::updateUi);
    connect(doc, &Document::metadataChanged, this, &MainWindow::updateUi);
    connect(view, &DocumentView::cursorChanged, this, &MainWindow::updateUi);
    auto stats = new QTimer(view);
    stats->setSingleShot(true);
    stats->setInterval(300);
    auto countWords = [this, doc] {
        auto text = doc->text()->toPlainText();
        doc->setProperty("words", text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size());
        updateUi();
    };
    connect(doc->text(), &QTextDocument::contentsChanged, stats, qOverload<>(&QTimer::start));
    connect(stats, &QTimer::timeout, view, countWords);
    stats->start();
    connect(doc, &Document::metadataChanged, view, [view, syntax, doc] {
        auto language = doc->mode() == Document::Mode::Markdown ? QString("Markdown")
                                                                : Highlighter::languageForPath(doc->path());
        syntax->setLanguage(language);
        for (auto e : view->findChildren<CodeEditor *>())
            e->setProperty("language", language);
    });
    configureViews();
    if (view->code)
        view->code->setFocus();
    else
        view->rich->setFocus();
    updateUi();
    return view;
}
DocumentView *MainWindow::newDocument(Document::Mode mode) {
    return attach(new Document(mode, this));
}
void MainWindow::openFile(const QString &path, int line) {
    auto canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty()) {
        QMessageBox::warning(this, tr("Open file"), tr("File does not exist: %1").arg(path));
        return;
    }
    for (int i = 0; i < tabs->count(); ++i) {
        auto v = qobject_cast<DocumentView *>(tabs->widget(i));
        if (v->model->path() == canonical) {
            tabs->setCurrentIndex(i);
            if (line > 0) {
                QTextCursor c(v->model->text()->findBlockByNumber(line - 1));
                v->setCursor(c);
            }
            return;
        }
    }
    if (loading.contains(canonical))
        return;
    loading.insert(canonical);
    statusBar()->showMessage(tr("Opening %1…").arg(canonical));
    auto worker = new QFutureWatcher<FileData>(this);
    connect(worker, &QFutureWatcher<FileData>::finished, this, [this, worker, canonical, line] {
        auto data = worker->result();
        worker->deleteLater();
        loading.remove(canonical);
        if (shuttingDown)
            return;
        if (!data.error.isEmpty()) {
            QMessageBox::warning(this, tr("Cannot open file"), data.error);
            return;
        }
        auto doc = new Document(Document::modeForPath(canonical), this);
        doc->apply(data);
        auto v = attach(doc);
        watcher.addPath(canonical);
        recent.removeAll(canonical);
        recent.prepend(canonical);
        while (recent.size() > 20)
            recent.removeLast();
        settings.setValue("recentFiles", recent);
        if (pendingViews.contains(canonical))
            v->restoreViewState(pendingViews.take(canonical));
        if (loading.isEmpty() && !pendingActive.isEmpty()) {
            for (int i = 0; i < tabs->count(); ++i)
                if (qobject_cast<DocumentView *>(tabs->widget(i))->model->path() == pendingActive)
                    tabs->setCurrentIndex(i);
            pendingActive.clear();
        }
        if (line > 0)
            v->setCursor(QTextCursor(doc->text()->findBlockByNumber(line - 1)));
        statusBar()->showMessage(tr("Opened %1").arg(canonical), 3000);
    });
    worker->setFuture(QtConcurrent::run(&Document::read, canonical));
}
bool MainWindow::saveCurrent(bool saveAs) {
    auto v = current();
    if (!v)
        return false;
    auto d = v->model;
    QString path = d->path();
    if (saveAs || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save document"), path.isEmpty() ? d->title() : path,
                                            d->mode() == Document::Mode::Html ? tr("HTML (*.html *.htm)")
                                            : d->mode() == Document::Mode::Markdown
                                                ? tr("Markdown (*.md *.markdown)")
                                                : tr("Text files (*)"));
        if (path.isEmpty())
            return false;
        if (d->mode() == Document::Mode::Html && Document::modeForPath(path) != Document::Mode::Html) {
            QMessageBox::warning(this, tr("HTML document"),
                                 tr("Use .html or .htm to preserve the rich-text document mode."));
            return false;
        }
    }
    for (int i = 0; i < tabs->count(); ++i) {
        auto other = qobject_cast<DocumentView *>(tabs->widget(i))->model;
        if (other != d && !other->path().isEmpty() && QFileInfo(path).canonicalFilePath() == other->path()) {
            QMessageBox::warning(this, tr("File already open"),
                                 tr("Choose another path or close the other document first."));
            return false;
        }
    }
    QString error;
    if (!d->save(path, &error)) {
        QMessageBox::warning(this, tr("Save failed"), error);
        return false;
    }
    watcher.addPath(d->path());
    recoverySnapshot();
    updateUi();
    return true;
}
bool MainWindow::canClose(DocumentView *v) {
    if (!v->model->text()->isModified())
        return true;
    tabs->setCurrentWidget(v);
    auto answer = QMessageBox::warning(
        this, tr("Unsaved document"), tr("Save changes to %1?").arg(v->model->title()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    return answer == QMessageBox::Discard || (answer == QMessageBox::Save && saveCurrent());
}
bool MainWindow::closeTab(int i) {
    auto v = qobject_cast<DocumentView *>(tabs->widget(i));
    if (!v || !canClose(v))
        return false;
    if (!v->model->path().isEmpty())
        closed.prepend(v->model->path());
    auto d = v->model;
    tabs->removeTab(i);
    delete v;
    delete d;
    recoverySnapshot();
    updateUi();
    return true;
}
void MainWindow::configureViews() {
    QFont font(settings.value("font", "monospace").toString(), settings.value("fontSize", 12).toInt());
    font.setStyleHint(QFont::Monospace);
    for (int i = 0; i < tabs->count(); ++i)
        qobject_cast<DocumentView *>(tabs->widget(i))
            ->configure(font, settings.value("wrap", false).toBool(), settings.value("tabWidth", 4).toInt(),
                        settings.value("spaces", true).toBool(),
                        settings.value("lineNumbers", true).toBool());
}
void MainWindow::updateUi() {
    for (int i = 0; i < tabs->count(); ++i) {
        auto v = qobject_cast<DocumentView *>(tabs->widget(i));
        tabs->setTabText(i, v->model->title() + (v->model->text()->isModified() ? " •" : ""));
        tabs->setTabToolTip(i, v->model->path());
    }
    auto v = current();
    if (formatBar)
        formatBar->setVisible(v && v->rich);
    for (auto action : commands)
        if (action->objectName().startsWith("format"))
            action->setEnabled(v && v->rich);
    if (!v)
        return;
    auto c = v->cursor();
    int chars = v->model->text()->characterCount() - 1;
    int words = v->model->property("words").toInt();
    statusBar()->showMessage(
        tr("Ln %1, Col %2   ·   %3 chars / %4 words / ~%5 min   ·   %6   ·   %7%8   ·   %9   ·   %10: %11")
            .arg(c.blockNumber() + 1)
            .arg(c.positionInBlock() + 1)
            .arg(chars)
            .arg(words)
            .arg(qMax(1, (words + 199) / 200))
            .arg(v->model->encoding(), v->model->newline(),
                 v->model->mixed() ? tr(" (mixed; normalizes on save)") : "",
                 Highlighter::languageForPath(v->model->path()),
                 settings.value("spaces", true).toBool() ? tr("Spaces") : tr("Tabs"))
            .arg(settings.value("tabWidth", 4).toInt()));
    setWindowTitle(v->model->title() + " — " + STRATA_PRODUCT_NAME);
}
void MainWindow::updateTheme() {
    bool light = settings.value("light", false).toBool();
    QPalette p;
    p.setColor(QPalette::Window, QColor(light ? "#E8EEF5" : "#10151D"));
    p.setColor(QPalette::WindowText, QColor(light ? "#18212D" : "#E8EEF5"));
    p.setColor(QPalette::Base, QColor(light ? "#FFFFFF" : "#141B25"));
    p.setColor(QPalette::AlternateBase, QColor(light ? "#E8EEF5" : "#202B39"));
    p.setColor(QPalette::Text, p.color(QPalette::WindowText));
    p.setColor(QPalette::Button, QColor(light ? "#DFE7EF" : "#18212D"));
    p.setColor(QPalette::ButtonText, p.color(QPalette::Text));
    p.setColor(QPalette::Highlight, QColor("#286FB0"));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(light ? "#536477" : "#93A3B5"));
    p.setColor(QPalette::ToolTipBase, p.color(QPalette::Base));
    p.setColor(QPalette::ToolTipText, p.color(QPalette::Text));
    qApp->setPalette(p);
    qApp->setStyleSheet(
        "QToolBar {spacing: 5px; padding: 4px; border: none;} QToolButton, QPushButton {padding: 5px 8px;} "
        "QTabBar::tab {padding: 9px 16px;} QTabBar::tab:selected {border-bottom: 2px solid #4DA8FF;} "
        "QDockWidget::title {padding: 8px;} QLineEdit, QSpinBox, QComboBox {padding: 4px;} QLineEdit:focus, "
        "QComboBox:focus {border: 1px solid #4DA8FF;} QTreeView {border:none;} QStatusBar {padding: 4px;}");
    configureViews();
}
void MainWindow::reloadCurrent() {
    auto v = current();
    if (!v || v->model->path().isEmpty())
        return;
    if (v->model->text()->isModified() &&
        QMessageBox::warning(this, tr("Reload"), tr("Discard unsaved edits and reload from disk?"),
                             QMessageBox::Discard | QMessageBox::Cancel,
                             QMessageBox::Cancel) != QMessageBox::Discard)
        return;
    auto data = Document::read(v->model->path());
    if (!data.error.isEmpty())
        QMessageBox::warning(this, tr("Reload failed"), data.error);
    else
        v->model->apply(data);
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (!loading.isEmpty()) {
        statusBar()->showMessage(tr("Please wait for files to finish opening."), 4000);
        event->ignore();
        return;
    }
    for (int i = 0; i < tabs->count(); ++i)
        if (!canClose(qobject_cast<DocumentView *>(tabs->widget(i)))) {
            event->ignore();
            return;
        }
    saveSession();
    Persistence::write(Persistence::dataPath("recovery.json"), {{"buffers", QJsonArray{}}});
    shuttingDown = true;
    event->accept();
}
void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}
void MainWindow::dropEvent(QDropEvent *event) {
    for (auto url : event->mimeData()->urls())
        if (url.isLocalFile()) {
            auto p = url.toLocalFile();
            if (QFileInfo(p).isDir())
                setProject(p);
            else
                openFile(p);
        }
    event->acceptProposedAction();
}
