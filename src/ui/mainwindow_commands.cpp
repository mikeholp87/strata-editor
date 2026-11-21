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
                if (current()->model->text()->isModified() && !saveCurrent())
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
            while (tabs->count() && closeTab(tabs->count() - 1)) {
            }
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
            QPrinter p;
            QPrintDialog dialog(&p, this);
            if (dialog.exec() == QDialog::Accepted)
                current()->model->text()->print(&p);
        },
        file);
    command(
        "printPreview", tr("Print preview…"), {},
        [this] {
            if (!current())
                return;
            QPrinter p;
            QPrintPreviewDialog dialog(&p, this);
            connect(&dialog, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter *printer) {
                if (current())
                    current()->model->text()->print(printer);
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
                if (current())
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
            auto v = current();
            auto codes = v->findChildren<CodeEditor *>();
            for (auto e : codes)
                if (e != v->code)
                    delete e;
            auto riches = v->findChildren<QTextEdit *>();
            for (auto e : riches)
                if (e != v->rich && e != static_cast<QTextEdit *>(v->preview))
                    delete e;
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
    command(
        "focus", tr("Focus mode"), QKeySequence("F11"),
        [this] {
            bool focus = !menuBar()->isVisible();
            menuBar()->setVisible(focus);
            statusBar()->setVisible(focus);
            explorerDock->setVisible(focus);
            for (auto t : findChildren<QToolBar *>())
                t->setVisible(focus && (t != formatBar || (current() && current()->rich)));
        },
        view);
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
                               tr("A native text, Markdown and visual HTML editor.\nVersion 0.1.0 • MIT\nQt "
                                  "%1\nNo telemetry or cloud account.")
                                   .arg(qVersion()));
        },
        help);
}
