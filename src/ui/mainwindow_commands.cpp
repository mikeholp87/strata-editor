#include "core/persistence.h"
#include "documentview.h"
#include "editor/codeeditor.h"
#include "editor/highlighter.h"
#include "gitpanel.h"
#include "mainwindow.h"
#include "projectsearch.h"
#include <QJsonArray>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QtConcurrent>
#include <QtWidgets>

void MainWindow::buildCommands() {
    auto file = menuBar()->addMenu(tr("&File"));
    auto edit = menuBar()->addMenu(tr("&Edit"));
    auto view = menuBar()->addMenu(tr("&View"));
    auto projectMenu = menuBar()->addMenu(tr("&Project"));
    auto tools = menuBar()->addMenu(tr("&Tools"));
    auto help = menuBar()->addMenu(tr("&Help"));
    auto toolbar = addToolBar(tr("Quick actions"));
    toolbar->setObjectName("quickActions");
    toolbar->setMovable(false);
    auto newFile = command("new", tr("New text file"), QKeySequence::New, [this] { newDocument(); }, file);
    toolbar->addAction(newFile);
    qobject_cast<QToolButton *>(toolbar->widgetForAction(newFile))->setText(tr("New"));
    command("newMarkdown", tr("New Markdown"), {}, [this] { newDocument(Document::Mode::Markdown); }, file);
    command("newHtml", tr("New rich-text HTML"), {}, [this] { newDocument(Document::Mode::Html); }, file);
    toolbar->addAction(command(
        "open", tr("Open…"), QKeySequence::Open,
        [this] {
            for (const auto &p : QFileDialog::getOpenFileNames(this, tr("Open files")))
                openFile(p);
        },
        file));
    command(
        "recent", tr("Recent files…"), {},
        [this] {
            bool ok;
            auto p = QInputDialog::getItem(this, tr("Recent files"), tr("File"), recent, 0, false, &ok);
            if (ok && !p.isEmpty())
                openFile(p);
        },
        file);
    toolbar->addAction(command("save", tr("Save"), QKeySequence::Save, [this] { saveCurrent(); }, file));
    command("saveAs", tr("Save As…"), QKeySequence::SaveAs, [this] { saveCurrent(true); }, file);
    command(
        "saveAll", tr("Save all"), {},
        [this] {
            int active = tabs->currentIndex();
            for (int i = 0; i < tabs->count(); ++i) {
                tabs->setCurrentIndex(i);
                if (current()->model->isModified() && !saveCurrent())
                    break;
            }
            tabs->setCurrentIndex(active);
        },
        file);
    command("reload", tr("Reload from disk"), {}, [this] { reloadCurrent(); }, file);
    command(
        "close", tr("Close document"), QKeySequence::Close, [this] { closeTab(tabs->currentIndex()); }, file);
    command(
        "closeAll", tr("Close all documents"), {},
        [this] {
            for (int i = tabs->count() - 1; i >= 0; --i)
                if (!closeTab(i))
                    break;
        },
        file);
    command(
        "reopen", tr("Reopen closed document"), QKeySequence("Ctrl+Shift+T"),
        [this] {
            if (!closed.isEmpty())
                openFile(closed.takeFirst());
        },
        file);
    command(
        "print", tr("Print…"), QKeySequence("Ctrl+Alt+P"),
        [this] {
            if (!current())
                return;
            QPointer<Document> document = current()->model;
            SafeDocument rendered;
            if (document->mode() == Document::Mode::Markdown)
                rendered.setMarkdown(document->sourceText(), QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) | QTextDocument::MarkdownNoHTML);
            QPrinter p;
            QPrintDialog dialog(&p, this);
            if (dialog.exec() == QDialog::Accepted && document)
                (document->mode() == Document::Mode::Markdown ? &rendered : document->text())->print(&p);
        },
        file);
    command(
        "printPreview", tr("Print preview…"), {},
        [this] {
            if (!current())
                return;
            QPointer<Document> document = current()->model;
            SafeDocument rendered;
            if (document->mode() == Document::Mode::Markdown)
                rendered.setMarkdown(document->sourceText(), QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) | QTextDocument::MarkdownNoHTML);
            QPrinter p;
            QPrintPreviewDialog dialog(&p, this);
            connect(&dialog, &QPrintPreviewDialog::paintRequested, this, [document, &rendered](QPrinter *printer) {
                if (document)
                    (document->mode() == Document::Mode::Markdown ? &rendered : document->text())->print(printer);
            });
            dialog.exec();
        },
        file);
    command("pdf", tr("Export PDF…"), {}, [this] { exportPdf(); }, file);
    command("htmlExport", tr("Export HTML…"), {}, [this] { exportHtml(); }, file);
    command("quit", tr("Quit"), QKeySequence::Quit, [this] { close(); }, file);
    auto cursorAction = [this, edit](QString id, QString title, QKeySequence key, auto fn) {
        command(
            id, title, key,
            [this, fn, id] {
                if (auto v = current()) {
                    if (v->isReadOnly() && id != "copy" && id != "selectAll")
                        return;
                    if (v->preview && v->markdownView() == DocumentView::MarkdownView::Preview &&
                        id != "copy" && id != "selectAll")
                        v->setMarkdownView(DocumentView::MarkdownView::Split);
                    fn(v);
                }
            },
            edit);
    };
    cursorAction("undo", tr("Undo"), QKeySequence::Undo, [](DocumentView *v) { v->model->text()->undo(); });
    cursorAction("redo", tr("Redo"), QKeySequence("Ctrl+Shift+Z"),
                 [](DocumentView *v) { v->model->text()->redo(); });
    cursorAction("cut", tr("Cut"), QKeySequence::Cut, [](DocumentView *v) {
        if (v->rich) {
            v->activeRich()->cut();
            return;
        }
        auto c = v->cursor();
        if (!c.hasSelection())
            return;
        QApplication::clipboard()->setText(c.selectedText().replace(QChar::ParagraphSeparator, '\n'));
        c.removeSelectedText();
    });
    cursorAction("copy", tr("Copy"), QKeySequence::Copy, [](DocumentView *v) {
        if (v->preview && v->markdownView() == DocumentView::MarkdownView::Preview) {
            v->preview->copy();
            return;
        }
        if (v->rich) {
            v->activeRich()->copy();
            return;
        }
        if (!v->cursor().hasSelection())
            return;
        QApplication::clipboard()->setText(
            v->cursor().selectedText().replace(QChar::ParagraphSeparator, '\n'));
    });
    cursorAction("paste", tr("Paste as text"), QKeySequence::Paste, [](DocumentView *v) {
        auto c = v->cursor();
        c.insertText(QApplication::clipboard()->text());
        v->setCursor(c);
    });
    cursorAction("selectAll", tr("Select all"), QKeySequence::SelectAll, [](DocumentView *v) {
        if (v->preview && v->markdownView() == DocumentView::MarkdownView::Preview) {
            v->preview->selectAll();
            return;
        }
        auto c = v->cursor();
        c.select(QTextCursor::Document);
        v->setCursor(c);
    });
    command(
        "find", tr("Find"), QKeySequence::Find,
        [this] {
            findBar->show();
            findChild<QPushButton *>("expandReplace")->setChecked(false);
            findText->setFocus();
            findText->selectAll();
        },
        edit);
    command(
        "replace", tr("Find and replace"), QKeySequence("Ctrl+H"),
        [this] {
            findBar->show();
            findChild<QPushButton *>("expandReplace")->setChecked(true);
            findText->setFocus();
        },
        edit);
    command("findNext", tr("Find next"), QKeySequence::FindNext, [this] { findNext(); }, edit);
    command(
        "findPrevious", tr("Find previous"), QKeySequence::FindPrevious, [this] { findNext(true); }, edit);
    command(
        "goto", tr("Go to line and column…"), QKeySequence("Ctrl+G"),
        [this] {
            if (!current())
                return;
            bool ok;
            auto s =
                QInputDialog::getText(this, tr("Go to"), tr("Line:column"), QLineEdit::Normal, "1:1", &ok);
            if (!ok)
                return;
            auto parts = s.split(':');
            int line = parts.value(0).toInt();
            auto block = current()->model->text()->findBlockByNumber(qMax(0, line - 1));
            if (!block.isValid())
                return;
            QTextCursor c(block);
            c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                           qBound(0, parts.value(1, "1").toInt() - 1, block.length() - 1));
            current()->setCursor(c);
        },
        edit);
    cursorAction("duplicate", tr("Duplicate line"), QKeySequence("Ctrl+D"), [](DocumentView *v) {
        if (v->code)
            v->activeCode()->duplicateLine();
    });
    cursorAction("comment", tr("Toggle comment"), QKeySequence("Ctrl+/"), [](DocumentView *v) {
        if (v->code)
            v->activeCode()->toggleComment();
    });
    command(
        "complete", tr("Show completions"), QKeySequence("Ctrl+Space"),
        [this] {
            if (auto v = current(); v && v->code && !v->isReadOnly())
                v->activeCode()->showCompletion(true);
        },
        edit);
    auto markdownMenu = edit->addMenu(tr("Markdown"));
    auto markdown = [this, markdownMenu](QString id, QString title, QKeySequence key, auto fn) {
        command(
            id, title, key,
            [this, fn] {
                auto v = current();
                if (!v || !v->code || v->isReadOnly() || !v->activeCode()->isMarkdown())
                    return;
                if (v->markdownView() == DocumentView::MarkdownView::Preview)
                    v->setMarkdownView(DocumentView::MarkdownView::Split);
                fn(v->activeCode());
            },
            markdownMenu);
    };
    markdown("mdCode", tr("Inline code"), QKeySequence("Ctrl+E"), [](CodeEditor *e) { e->toggleInline("`"); });
    markdown("mdHeading1", tr("Heading 1"), QKeySequence("Ctrl+Alt+1"), [](CodeEditor *e) { e->setHeading(1); });
    markdown("mdHeading2", tr("Heading 2"), QKeySequence("Ctrl+Alt+2"), [](CodeEditor *e) { e->setHeading(2); });
    markdown("mdHeading3", tr("Heading 3"), QKeySequence("Ctrl+Alt+3"), [](CodeEditor *e) { e->setHeading(3); });
    markdown("mdBullets", tr("Bulleted list"), {}, [](CodeEditor *e) { e->toggleLineKind(CodeEditor::LineKind::Bullet); });
    markdown("mdNumbers", tr("Numbered list"), {}, [](CodeEditor *e) { e->toggleLineKind(CodeEditor::LineKind::Number); });
    markdown("mdTasks", tr("Task list"), {}, [](CodeEditor *e) { e->toggleLineKind(CodeEditor::LineKind::Task); });
    markdown("mdQuote", tr("Quote"), {}, [](CodeEditor *e) { e->toggleLineKind(CodeEditor::LineKind::Quote); });
    markdown("mdLink", tr("Link"), QKeySequence("Ctrl+Alt+K"), [](CodeEditor *e) { e->insertLink(); });
    markdown("mdCodeBlock", tr("Code block"), {}, [](CodeEditor *e) { e->insertCodeBlock(); });
    cursorAction("indent", tr("Indent"), QKeySequence("Ctrl+]"), [](DocumentView *v) {
        if (v->code)
            v->activeCode()->indent();
    });
    cursorAction("unindent", tr("Unindent"), QKeySequence("Ctrl+["), [](DocumentView *v) {
        if (v->code)
            v->activeCode()->indent(true);
    });
    cursorAction("uppercase", tr("Uppercase selection"), {}, [](DocumentView *v) {
        auto c = v->cursor();
        c.insertText(c.selectedText().toUpper());
    });
    cursorAction("lowercase", tr("Lowercase selection"), {}, [](DocumentView *v) {
        auto c = v->cursor();
        c.insertText(c.selectedText().toLower());
    });
    cursorAction("trim", tr("Trim trailing whitespace"), {}, [](DocumentView *v) {
        if (v->preview && v->markdownView() == DocumentView::MarkdownView::Preview)
            v->setMarkdownView(DocumentView::MarkdownView::Split);
        QTextCursor c(v->model->text());
        c.beginEditBlock();
        auto b = v->model->text()->lastBlock();
        while (b.isValid()) {
            auto s = b.text();
            int end = s.size();
            while (end > 0 && s[end - 1].isSpace())
                --end;
            if (end < s.size()) {
                QTextCursor t(b);
                t.setPosition(b.position() + end);
                t.setPosition(b.position() + s.size(), QTextCursor::KeepAnchor);
                t.removeSelectedText();
            }
            b = b.previous();
        }
        c.endEditBlock();
    });
    for (auto e : {QString("LF"), QString("CRLF")})
        command(
            "newline" + e, tr("Use %1 line endings").arg(e), {},
            [this, e] {
                if (current() && !current()->isReadOnly())
                    current()->model->setNewline(e);
            },
            edit);
    auto toolbarSpacer = new QWidget;
    toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(toolbarSpacer);
    toolbar->addAction(
        command("palette", tr("Commands…"), QKeySequence("Ctrl+Shift+P"), [this] { palette(); }, view));
    command("quickOpen", tr("Quick open…"), QKeySequence("Ctrl+P"), [this] { palette(true); }, view);
    command(
        "nextTab", tr("Next document"), QKeySequence("Ctrl+Tab"),
        [this] {
            if (tabs->count())
                tabs->setCurrentIndex((tabs->currentIndex() + 1) % tabs->count());
        },
        view);
    command(
        "splitVertical", tr("Split side by side"), {},
        [this] {
            if (current())
                current()->split(Qt::Horizontal);
        },
        view);
    command(
        "splitHorizontal", tr("Split above and below"), {},
        [this] {
            if (current())
                current()->split(Qt::Vertical);
        },
        view);
    command(
        "unsplit", tr("Close extra split views"), {},
        [this] {
            if (!current())
                return;
            current()->closeExtraSplits();
        },
        view);
    command(
        "preview", tr("Toggle Markdown preview"), {},
        [this] {
            if (current())
                current()->togglePreview();
        },
        view);
    auto wrap = command(
        "wrap", tr("Word wrap"), {},
        [this] {
            settings.setValue("wrap", !settings.value("wrap", false).toBool());
            configureViews();
        },
        view);
    wrap->setCheckable(true);
    wrap->setChecked(settings.value("wrap", false).toBool());
    command(
        "theme", tr("Toggle dark / light theme"), {},
        [this] {
            settings.setValue("light", !settings.value("light", false).toBool());
            updateTheme();
        },
        view);
    for (const int direction : {1, -1})
        command(direction > 0 ? "fontIncrease" : "fontDecrease",
                direction > 0 ? tr("Increase font size") : tr("Decrease font size"),
                direction > 0 ? QKeySequence(QKeySequence::ZoomIn) : QKeySequence(QKeySequence::ZoomOut),
                [this, direction] {
                    const bool sidebar =
                        tree->hasFocus() || tree->isAncestorOf(QApplication::focusWidget());
                    const auto key = sidebar ? "sidebarFontSize" : "fontSize";
                    const int size = sidebar ? tree->font().pointSize() : settings.value(key, 12).toInt();
                    settings.setValue(key, qBound(7, size + direction, 48));
                    settings.sync();
                    configureViews();
                }, view);
    auto focusAction = command(
        "focus", tr("Focus mode"), QKeySequence("F11"),
        [this] {
            if (!focusMode) {
                focusLayout = saveState();
                focusMode = true;
                menuBar()->hide();
                statusBar()->hide();
                for (auto dock : findChildren<QDockWidget *>())
                    dock->hide();
                for (auto bar : findChildren<QToolBar *>())
                    bar->hide();
                focusExitBar->show();
            } else {
                focusMode = false;
                restoreState(focusLayout);
                menuBar()->show();
                statusBar()->show();
                focusExitBar->hide();
                updateUi();
            }
        }, view);
    focusAction->setCheckable(true);
    focusExitBar = addToolBar(tr("Focus controls"));
    focusExitBar->setObjectName("focusExitBar");
    focusExitBar->setMovable(false);
    auto exitFocus = new QToolButton;
    exitFocus->setObjectName("exitFocusMode");
    const auto updateExitLabel = [exitFocus, focusAction] {
        const auto shortcut = focusAction->shortcut().toString(QKeySequence::NativeText);
        exitFocus->setText(shortcut.isEmpty() ? tr("Exit focus mode")
                                             : tr("Exit focus mode (%1)").arg(shortcut));
    };
    connect(focusAction, &QAction::changed, exitFocus, updateExitLabel);
    updateExitLabel();
    connect(exitFocus, &QToolButton::clicked, focusAction, &QAction::trigger);
    focusExitBar->addWidget(exitFocus);
    focusExitBar->hide();
    view->addAction(explorerDock->toggleViewAction());
    view->addAction(searchDock->toggleViewAction());
    view->addAction(gitDock->toggleViewAction());
    command(
        "openProject", tr("Open folder…"), {},
        [this] {
            auto p = QFileDialog::getExistingDirectory(this, tr("Open project folder"), project);
            if (!p.isEmpty())
                setProject(p);
        },
        projectMenu);
    command(
        "recentProject", tr("Recent projects…"), {},
        [this] {
            bool ok;
            auto p = QInputDialog::getItem(this, tr("Recent projects"), tr("Folder"),
                                           settings.value("recentProjects").toStringList(), 0, false, &ok);
            if (ok && !p.isEmpty())
                setProject(p);
        },
        projectMenu);
    command(
        "projectSearch", tr("Search project"), QKeySequence("Ctrl+Shift+F"),
        [this] {
            searchDock->show();
            searchDock->raise();
        },
        projectMenu);
    command(
        "hidden", tr("Toggle hidden files"), {}, [this] { files->setFilter(files->filter() ^ QDir::Hidden); },
        projectMenu);
    command(
        "git", tr("Git status"), {},
        [this] {
            gitDock->show();
            git->refresh();
        },
        tools);
    command(
        "terminal", tr("Open external terminal"), {},
        [this] {
#ifdef Q_OS_WIN
            QProcess::startDetached("cmd.exe", {}, project);
#else
            auto executable = QStandardPaths::findExecutable("x-terminal-emulator");
            if (executable.isEmpty())
                QMessageBox::information(this, tr("Terminal"),
                                         tr("Install or configure a system terminal. Embedded terminal "
                                            "emulation is a later milestone."));
            else
                QProcess::startDetached(executable, {}, project);
#endif
        },
        tools);
    command("shortcuts", tr("Keyboard shortcuts…"), {}, [this] { editShortcuts(); }, tools);
    command("settings", tr("Preferences…"), QKeySequence("Ctrl+,"), [this] { preferences(); }, tools);
    command(
        "about", tr("About"), {},
        [this] {
            QMessageBox::about(this, STRATA_PRODUCT_NAME,
                               tr("A native text, Markdown and visual HTML editor.\nVersion 0.1.2 • MIT\nQt "
                                  "%1\nNo telemetry or cloud account.")
                                   .arg(qVersion()));
        },
        help);
}
