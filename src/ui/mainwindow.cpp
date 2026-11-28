#include "mainwindow.h"
#include "core/persistence.h"
#include "core/recovery.h"
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

MainWindow::MainWindow(bool restoreWorkspace, bool preserve) : settings("StrataEditor", "StrataEditor"), preserveRecovery(preserve) {
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
    tabs->setElideMode(Qt::ElideMiddle);
    tabs->setUsesScrollButtons(true);
    tabs->tabBar()->setObjectName("documentTabBar");
    tabs->tabBar()->setDrawBase(false);
    tabs->tabBar()->setExpanding(false);
    auto center = new QWidget;
    auto layout = new QVBoxLayout(center);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tabs);
    findBar = new QWidget;
    findBar->setObjectName("findBar");
    auto findLayout = new QVBoxLayout(findBar);
    findLayout->setContentsMargins(8, 6, 8, 6);
    auto findRow = new QHBoxLayout;
    findText = new QLineEdit;
    findText->setObjectName("findText");
    findText->setPlaceholderText(tr("Find"));
    findText->setAccessibleName(tr("Find text"));
    findRow->addWidget(findText, 1);
    auto next = new QPushButton(tr("Next"));
    findRow->addWidget(next);
    connect(next, &QPushButton::clicked, this, [this] { findNext(); });
    auto expandReplace = new QPushButton(tr("Replace…"));
    expandReplace->setObjectName("expandReplace");
    expandReplace->setCheckable(true);
    findRow->addWidget(expandReplace);
    auto closeFind = new QToolButton;
    closeFind->setText(tr("×"));
    closeFind->setAccessibleName(tr("Close find"));
    closeFind->setToolTip(tr("Close find (Esc)"));
    closeFind->setFocusPolicy(Qt::StrongFocus);
    findRow->addWidget(closeFind);
    findLayout->addLayout(findRow);
    auto options = new QHBoxLayout;
    matchCase = new QCheckBox(tr("Match case"));
    wholeWord = new QCheckBox(tr("Whole words"));
    wholeWord->setObjectName("wholeWord");
    regex = new QCheckBox(tr("Regular expression"));
    for (auto option : {matchCase, wholeWord, regex})
        options->addWidget(option);
    options->addStretch();
    findLayout->addLayout(options);
    replaceRow = new QWidget;
    replaceRow->setObjectName("replaceRow");
    auto replaceLayout = new QHBoxLayout(replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceText = new QLineEdit;
    replaceText->setObjectName("replaceText");
    replaceText->setPlaceholderText(tr("Replace with"));
    replaceText->setAccessibleName(tr("Replacement text"));
    replaceLayout->addWidget(replaceText, 1);
    auto replaceOne = new QPushButton(tr("Replace"));
    auto replaceAll = new QPushButton(tr("Replace all"));
    replaceLayout->addWidget(replaceOne);
    replaceLayout->addWidget(replaceAll);
    connect(replaceOne, &QPushButton::clicked, this, [this] { replace(); });
    connect(replaceAll, &QPushButton::clicked, this, [this] { replace(true); });
    connect(expandReplace, &QPushButton::toggled, replaceRow, &QWidget::setVisible);
    findLayout->addWidget(replaceRow);
    replaceRow->hide();
    auto hideFind = [this] {
        findBar->hide();
        if (auto v = current()) {
            if (v->preview && v->markdownView() == DocumentView::MarkdownView::Preview)
                v->preview->setFocus();
            else if (v->code)
                v->activeCode()->setFocus();
            else
                v->rich->setFocus();
        }
    };
    connect(closeFind, &QToolButton::clicked, this, hideFind);
    auto escapeFind = new QShortcut(QKeySequence(Qt::Key_Escape), findBar);
    escapeFind->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeFind, &QShortcut::activated, this, hideFind);
    layout->addWidget(findBar);
    findBar->hide();
    setCentralWidget(center);
    cursorStatus = new QLabel;
    cursorStatus->setObjectName("cursorStatus");
    cursorStatus->setContentsMargins(8, 0, 8, 0);
    documentStatus = new QLabel;
    documentStatus->setObjectName("documentStatus");
    documentStatus->setContentsMargins(8, 0, 8, 0);
    statusBar()->addPermanentWidget(cursorStatus);
    statusBar()->addPermanentWidget(documentStatus);
    connect(findText, &QLineEdit::returnPressed, this, [this] { findNext(); });
    explorerDock = new QDockWidget(tr("Project"), this);
    explorerDock->setObjectName("explorerDock");
    files = new QFileSystemModel(this);
    files->setReadOnly(true);
    files->setRootPath(QDir::homePath());
    tree = new QTreeView;
    tree->setObjectName("projectTree");
    tree->setAccessibleName(tr("Project files"));
    tree->setMinimumWidth(120);
    tree->setModel(files);
    tree->setHeaderHidden(true);
    for (int i = 1; i < 4; ++i)
        tree->hideColumn(i);
    tree->setRootIndex(files->index(QDir::homePath()));
    explorerDock->setWidget(tree);
    addDockWidget(Qt::LeftDockWidgetArea, explorerDock);
    resizeDocks({explorerDock}, {230}, Qt::Horizontal);
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
    searchDock = new QDockWidget(tr("Project search"), this);
    searchDock->setObjectName("searchDock");
    searchDock->setWidget(projectSearch);
    addDockWidget(Qt::BottomDockWidgetArea, searchDock);
    searchDock->hide();
    connect(projectSearch, &ProjectSearch::openHit, this, &MainWindow::openFile);
    git = new GitPanel;
    gitDock = new QDockWidget(tr("Git"), this);
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
            if (v->model->path() == path) {
                v->setExternalChange(v->model->changedOnDisk());
                updateUi();
            }
        }
    });
    auto timer = new QTimer(this);
    timer->setInterval(15000);
    connect(timer, &QTimer::timeout, this, &MainWindow::recoverySnapshot);
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("layout").toByteArray());
    if (restoreWorkspace)
        restoreSession();
    if (!tabs->count() && loading.isEmpty())
        newDocument();
    updateUi();
    timer->start();
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
    connect(view, &DocumentView::reloadRequested, this, [this, view] {
        tabs->setCurrentWidget(view);
        reloadCurrent();
    });
    connect(view, &DocumentView::saveCopyRequested, this, [this, view] {
        tabs->setCurrentWidget(view);
        saveCurrent(true);
    });
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
            if (loading.isEmpty() && !tabs->count())
                newDocument();
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
    return saveView(current(), saveAs);
}
bool MainWindow::saveView(DocumentView *v, bool saveAs) {
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
    if (d->foreignHtml() && QFileInfo(path).canonicalFilePath() == d->path()) {
        QMessageBox box(QMessageBox::Warning, tr("Rewrite HTML file?"),
                        tr("%1 was not written by this editor. Saving rewrites it through the visual editor, "
                           "which drops scripts, stylesheets, the doctype and other markup it cannot show.")
                            .arg(d->title()),
                        QMessageBox::Cancel, this);
        auto copy = box.addButton(tr("Save a copy…"), QMessageBox::AcceptRole);
        auto overwrite = box.addButton(tr("Overwrite original"), QMessageBox::DestructiveRole);
        box.setDefaultButton(copy);
        box.exec();
        if (box.clickedButton() == copy)
            return saveView(v, true);
        if (box.clickedButton() != overwrite)
            return false;
    }
    QString error;
    if (!d->save(path, &error)) {
        QMessageBox::warning(this, tr("Save failed"), error);
        return false;
    }
    watcher.addPath(d->path());
    v->setExternalChange(false);
    recoverySnapshot();
    updateUi();
    return true;
}
bool MainWindow::canClose(DocumentView *v) {
    if (!v->model->isModified())
        return true;
    tabs->setCurrentWidget(v);
    auto answer = QMessageBox::warning(
        this, tr("Unsaved document"), tr("Save changes to %1?").arg(v->model->title()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    return answer == QMessageBox::Discard || (answer == QMessageBox::Save && saveView(v));
}
bool MainWindow::closeTab(int i) {
    auto v = qobject_cast<DocumentView *>(tabs->widget(i));
    if (!v || !canClose(v))
        return false;
    if (!v->model->path().isEmpty())
        closed.prepend(v->model->path());
    auto d = v->model;
    if (!d->path().isEmpty())
        watcher.removePath(d->path()); // each file is open in at most one tab
    tabs->removeTab(i);
    delete v;
    delete d;
    if (!tabs->count())
        newDocument();
    recoverySnapshot();
    updateUi();
    return true;
}
void MainWindow::configureViews() {
    auto sidebarFont = QApplication::font();
    sidebarFont.setPointSize(qBound(7, settings.value("sidebarFontSize", sidebarFont.pointSize()).toInt(), 48));
    tree->setFont(sidebarFont);
    QFont font(settings.value("font", "monospace").toString(), settings.value("fontSize", 12).toInt());
    font.setStyleHint(QFont::Monospace);
    for (int i = 0; i < tabs->count(); ++i) {
        auto view = qobject_cast<DocumentView *>(tabs->widget(i));
        view->configure(font, settings.value("wrap", false).toBool(), settings.value("tabWidth", 4).toInt(),
                        settings.value("spaces", true).toBool(),
                        settings.value("lineNumbers", true).toBool());
        for (auto syntax : view->model->text()->findChildren<Highlighter *>())
            syntax->setLightTheme(settings.value("light", false).toBool());
    }
}
void MainWindow::updateUi() {
    for (int i = 0; i < tabs->count(); ++i) {
        auto v = qobject_cast<DocumentView *>(tabs->widget(i));
        auto title = v->model->title();
        bool modified = v->model->isModified();
        tabs->setTabText(i, title + (modified ? " •" : ""));
        tabs->tabBar()->setAccessibleTabName(
            i, title + (modified ? tr(", unsaved changes") : QString()) +
                   (v->externallyChanged() ? tr(", changed on disk") : QString()));
        for (auto side : {QTabBar::LeftSide, QTabBar::RightSide}) {
            if (auto close = tabs->tabBar()->tabButton(i, side)) {
                close->setAccessibleName(tr("Close %1").arg(title));
                close->setToolTip(tr("Close %1").arg(title));
            }
        }
        tabs->setTabToolTip(i, v->model->path() +
            (v->externallyChanged() ? tr("\nChanged on disk. Reload or save a copy.") : QString()));
    }
    auto v = current();
    if (formatBar)
        formatBar->setVisible(!focusMode && v && v->rich);
    const bool markdownSource = v && v->code && v->activeCode()->isMarkdown();
    if (markdownBar)
        markdownBar->setVisible(!focusMode && markdownSource);
    for (auto action : commands) {
        const auto id = action->objectName();
        if (id == "formatBold" || id == "formatItalic" || id == "formatStrike")
            action->setEnabled(v && (v->rich || markdownSource));
        else if (id.startsWith("format"))
            action->setEnabled(v && v->rich);
        else if (id.startsWith("md"))
            action->setEnabled(markdownSource);
    }
    if (!v) {
        cursorStatus->clear();
        documentStatus->clear();
        return;
    }
    auto c = v->cursor();
    int chars = v->model->text()->characterCount() - 1;
    int words = v->model->property("words").toInt();
    cursorStatus->setText(tr("Ln %1, Col %2").arg(c.blockNumber() + 1).arg(c.positionInBlock() + 1));
    auto language = v->model->mode() == Document::Mode::Markdown
                        ? QString("Markdown") : Highlighter::languageForPath(v->model->path());
    documentStatus->setText(tr("%1   %2   %3%4")
                               .arg(language, v->model->encoding(), v->model->newline(),
                                    v->model->mixed() ? tr(" (mixed)") : QString()));
    documentStatus->setToolTip(tr("%1 characters, %2 words, about %3 min\n%4: %5%6")
                                  .arg(chars).arg(words).arg(qMax(1, (words + 199) / 200))
                                  .arg(settings.value("spaces", true).toBool() ? tr("Spaces") : tr("Tabs"))
                                  .arg(settings.value("tabWidth", 4).toInt())
                                  .arg(v->model->mixed()
                                           ? tr("\nMixed line endings normalize on save.") : QString()));
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
    p.setColor(QPalette::Link, QColor(light ? "#1E659E" : "#4DA8FF"));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(light ? "#536477" : "#93A3B5"));
    p.setColor(QPalette::ToolTipBase, p.color(QPalette::Base));
    p.setColor(QPalette::ToolTipText, p.color(QPalette::Text));
    qApp->setPalette(p);
    qApp->setStyleSheet(QString(
        "QToolBar {spacing: 2px; padding: 2px; border: none;} "
        "QToolButton, QPushButton {padding: 4px 8px;} "
        "QTabWidget#documentTabs::pane {border: none;} "
        "QTabBar#documentTabBar {background: palette(window);} "
        "QTabBar#documentTabBar::tab {background: palette(window); color: %4; "
        "border: none; border-right: 1px solid %2; border-top: 2px solid transparent; "
        "border-bottom: 2px solid transparent; min-width: 96px; max-width: 220px; min-height: 20px; "
        "padding: 6px 12px; margin: 0;} "
        "QTabBar#documentTabBar::tab:!selected:hover {background: palette(alternate-base); "
        "color: palette(text);} "
        "QTabBar#documentTabBar::tab:selected {background: palette(base); color: palette(text); "
        "border-top-color: %1;} "
        "QTabBar#documentTabBar::tab:selected:focus {border-bottom-color: %1;} "
        "QTabBar#documentTabBar::close-button {image: url(:/strata/tab-close-%3.png); "
        "width: 20px; height: 20px;} "
        "QTabBar#documentTabBar::close-button:hover {image: url(:/strata/tab-close-hover.png);} "
        "QTabBar#documentTabBar::tear {background: palette(window); border: none; width: 0;} "
        "QTabBar#documentTabBar::scroller {width: 48px;} "
        "QTabBar#documentTabBar QToolButton {background: palette(window); border: none; "
        "border-left: 1px solid %2; padding: 0; width: 24px;} "
        "QTabBar#documentTabBar QToolButton:hover {background: palette(alternate-base);} "
        "QTabBar#documentTabBar QToolButton:focus {border: 2px solid %1;} "
        "QDockWidget::title {padding: 6px;} QLineEdit, QSpinBox, QComboBox {padding: 4px;} "
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QAbstractButton:focus {border: 2px solid %1;} "
        "QTreeView {border:none;} QStatusBar {padding: 2px;} "
        "#markdownControls QToolButton:checked {background: palette(alternate-base); "
        "border-bottom: 2px solid %1;} "
        "#externalWarning {background: palette(alternate-base);}")
        .arg(p.color(QPalette::Link).name(), light ? "#CBD5E1" : "#293545", light ? "light" : "dark",
             p.color(QPalette::PlaceholderText).name()));
    configureViews();
}
void MainWindow::reloadCurrent() {
    auto v = current();
    if (!v || v->model->path().isEmpty())
        return;
    if (v->model->isModified() &&
        QMessageBox::warning(this, tr("Reload"), tr("Discard unsaved edits and reload from disk?"),
                             QMessageBox::Discard | QMessageBox::Cancel,
                             QMessageBox::Cancel) != QMessageBox::Discard)
        return;
    auto data = Document::read(v->model->path());
    if (!data.error.isEmpty())
        QMessageBox::warning(this, tr("Reload failed"), data.error);
    else {
        v->model->apply(data);
        if (!watcher.files().contains(v->model->path()))
            watcher.addPath(v->model->path());
        v->setExternalChange(false);
        updateUi();
    }
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (git->isBusy()) {
        gitDock->show();
        statusBar()->showMessage(
            tr("Git is running. Wait for it to finish or cancel the read-only command."), 10000);
        event->ignore();
        return;
    }
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
    QString recoveryError;
    if (!recoveryBlocked && !preserveRecovery &&
        !Recovery::save(Persistence::dataPath("recovery.json"), {{"buffers", QJsonArray{}}}, &recoveryError))
        QMessageBox::warning(this, tr("Recovery cleanup failed"),
                             tr("Previous recovery data was retained: %1").arg(recoveryError));
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
