#include "core/document.h"
#include "core/export.h"
#include "core/persistence.h"
#include "editor/codeeditor.h"
#include "editor/highlighter.h"
#include "ui/documentview.h"
#include "ui/mainwindow.h"
#include "ui/projectsearch.h"
#include <QFile>
#include <QClipboard>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextLayout>
#include <QToolButton>
#include <cmath>
#include <algorithm>
#include <QJsonArray>
#include <QLineEdit>
#include <QMessageBox>
#include <QPrinter>
#include <QProcess>
#include <QSettings>
#include <QTabWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QtTest>
class StrataTests : public QObject {
    Q_OBJECT
    QTemporaryDir testPaths;
  private slots:
    void initTestCase() {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QStandardPaths::setTestModeEnabled(true);
#ifdef Q_OS_UNIX
        QStandardPaths::setTestModeEnabled(false);
        qputenv("XDG_DATA_HOME", testPaths.path().toUtf8());
#endif
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, testPaths.path());
        QCoreApplication::setOrganizationName("StrataTests");
    }
    void encodingRoundTrips_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("utf8") << QString::fromUtf8("Hello 🙂 مرحبا\n").toUtf8();
        QTest::newRow("utf8bom") << (QByteArray::fromHex("efbbbf") + QByteArray("hi\r\n"));
        QTest::newRow("utf16le") << QByteArray::fromHex("fffe41003dd842de0d000a00");
        QTest::newRow("utf16be") << QByteArray::fromHex("feff0041d83dde42000d000a");
        QTest::newRow("unicode-spaces") << QByteArray::fromHex("61c2a062e280a863");
        QTest::newRow("empty") << QByteArray();
    }
    void encodingRoundTrips() {
        QFETCH(QByteArray, bytes);
        QTemporaryDir dir;
        QString path = dir.filePath("file.txt");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(bytes);
        }
        auto data = Document::read(path);
        QVERIFY2(data.error.isEmpty(), qPrintable(data.error));
        Document doc;
        doc.apply(data);
        QString error;
        QVERIFY2(doc.save(path, &error), qPrintable(error));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), bytes);
    }
    void conflict() {
        QTemporaryDir dir;
        auto p = dir.filePath("x");
        {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("old");
        }
        Document d;
        d.apply(Document::read(p));
        d.text()->setPlainText("mine");
        {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("external");
        }
        QString error;
        QVERIFY(!d.save(p, &error));
        QFile f(p);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("external"));
        QVERIFY(d.text()->isModified());
    }
    void invalidInput() {
        QTemporaryDir dir;
        auto p = dir.filePath("bad");
        for (auto bytes : {QByteArray::fromHex("ff807f"), QByteArray::fromHex("00010203"),
                           QByteArray::fromHex("e282"), QByteArray::fromHex("fffe41")}) {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(bytes);
            f.close();
            QVERIFY(!Document::read(p).error.isEmpty());
        }
    }
    void sharedUndo() {
        Document d;
        DocumentView v(&d);
        v.split(Qt::Horizontal);
        auto editors = v.findChildren<CodeEditor *>();
        QCOMPARE(editors.size(), 2);
        QTest::keyClicks(editors[0], "shared");
        QCOMPARE(editors[1]->toPlainText(), QString("shared"));
        d.text()->undo();
        QVERIFY(editors[1]->toPlainText().isEmpty());
    }
    void commentIndent() {
        CodeEditor e;
        e.setPlainText("alpha\nbeta");
        e.selectAll();
        e.indent();
        QCOMPARE(e.toPlainText(), QString("    alpha\n    beta"));
        e.undo();
        QCOMPARE(e.toPlainText(), QString("alpha\nbeta"));
        e.selectAll();
        e.toggleComment();
        QCOMPARE(e.toPlainText(), QString("// alpha\n// beta"));
        e.selectAll();
        e.toggleComment();
        QCOMPARE(e.toPlainText(), QString("alpha\nbeta"));
    }
    void richRoundTrip() {
        QTemporaryDir dir;
        Document d(Document::Mode::Html);
        QTextCursor c(d.text());
        QTextCharFormat f;
        f.setFontWeight(QFont::Bold);
        c.insertText("Formatted", f);
        QString error;
        auto p = dir.filePath("note.html");
        QVERIFY(d.save(p, &error));
        Document second(Document::Mode::Html);
        second.apply(Document::read(p));
        QCOMPARE(second.text()->toPlainText(), QString("Formatted"));
        QTextCursor selection(second.text());
        selection.select(QTextCursor::Document);
        QCOMPARE(selection.charFormat().fontWeight(), int(QFont::Bold));
    }
    void markdownSafety() {
        Document d(Document::Mode::Markdown);
        d.text()->setPlainText(
            "# Heading\n\n**bold**\n\n<script>alert(1)</script>\n![image](https://example.com/a.png)");
        DocumentView v(&d);
        QVERIFY(v.preview);
        QVERIFY(v.preview->toPlainText().contains("Heading"));
        QVERIFY(d.text()->toPlainText().contains("**bold**"));
        QVERIFY(!v.preview->document()
                     ->resource(QTextDocument::ImageResource, QUrl("https://example.com/a.png"))
                     .isValid());
    }
    void recoveryAndSession() {
        QTemporaryDir dir;
        Document d(Document::Mode::Html);
        d.text()->setHtml("<b>recovered</b>");
        auto p = dir.filePath("snapshot.json");
        QVERIFY(Persistence::write(p, {{"buffers", QJsonArray{d.snapshot()}}}));
        auto o = Persistence::read(p)["buffers"].toArray()[0].toObject();
        Document restored(Document::Mode::Html);
        restored.restore(o);
        QCOMPARE(restored.text()->toPlainText(), QString("recovered"));
        QVERIFY(restored.text()->isModified());
        {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("broken");
        }
        QVERIFY(Persistence::read(p).isEmpty());
    }
    void projectSearch() {
        QTemporaryDir dir;
        QFile f(dir.filePath("a.txt"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("one\nneedle\nthree\n");
        f.close();
        auto cancel = std::make_shared<std::atomic_bool>(false);
        auto hits = ProjectSearch::search(dir.path(), "Needle", cancel);
        QCOMPARE(hits.size(), 1);
        QCOMPARE(hits[0].line, 2);
        *cancel = true;
        QVERIFY(ProjectSearch::search(dir.path(), "needle", cancel).isEmpty());
    }
    void readOnlySave() {
        QTemporaryDir dir;
        auto p = dir.filePath("readonly.txt");
        {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("original");
        }
        Document d;
        d.apply(Document::read(p));
        QVERIFY(QFile::setPermissions(p, QFileDevice::ReadOwner));
        QString error;
        QVERIFY(!d.save(p, &error));
        QFile::setPermissions(p, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QCOMPARE(Document::read(p).text, QString("original"));
    }
    void unsavedCloseCancel() {
        MainWindow w(false);
        w.current()->code->insertPlainText("keep me");
        auto tabs = w.findChild<QTabWidget *>("documentTabs");
        int count = tabs->count();
        QTimer::singleShot(0, [] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto box = qobject_cast<QMessageBox *>(widget))
                    box->done(QMessageBox::Cancel);
        });
        w.findChild<QAction *>("close")->trigger();
        QCOMPARE(tabs->count(), count);
        QCOMPARE(w.current()->code->toPlainText(), QString("keep me"));
        w.current()->model->text()->setModified(false);
    }
    void documentTabsOverflowAndClose() {
        for (auto theme : {QString("dark"), QString("light"), QString("hover")}) {
            QImage icon(":/strata/tab-close-" + theme + ".png");
            QVERIFY(!icon.isNull());
            QCOMPARE(icon.pixelColor(0, 0).alpha(), 0);
            QVERIFY(icon.pixelColor(icon.width() / 2, icon.height() / 2).alpha() > 0);
        }
        QTemporaryDir dir;
        MainWindow window(false);
        window.resize(640, 420);
        window.showNormal();
        auto tabs = window.findChild<QTabWidget *>("documentTabs");
        auto bar = tabs->tabBar();
        QPointer<DocumentView> edited;
        QString editedTitle;
        for (int i = 0; i < 6; ++i) {
            auto title = QString("project-document-with-a-long-descriptive-name-%1.cpp").arg(i);
            auto path = dir.filePath(title);
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.close();
            window.openFile(path);
            QTRY_COMPARE(window.current()->model->path(), QFileInfo(path).canonicalFilePath());
            if (!i) {
                edited = window.current();
                editedTitle = title;
            }
        }
        QCOMPARE(window.width(), 640);
        auto arrows = bar->findChildren<QToolButton *>();
        QVERIFY(std::any_of(arrows.begin(), arrows.end(), [](auto b) { return b->isVisible(); }));
        bar->setFocus();
        QTRY_VERIFY(bar->hasFocus());
        int previous = tabs->currentIndex();
        QTest::keyClick(bar, Qt::Key_Left);
        QCOMPARE(tabs->currentIndex(), previous - 1);
        edited->code->insertPlainText("keep these edits");
        int index = tabs->indexOf(edited);
        QVERIFY(bar->accessibleTabName(index).contains("unsaved changes"));
        QCOMPARE(bar->tabToolTip(index), edited->model->path());
        bar->moveTab(index, 3);
        QCOMPARE(tabs->indexOf(edited), 3);
        tabs->setCurrentWidget(edited);
        auto close = qobject_cast<QAbstractButton *>(bar->tabButton(3, QTabBar::RightSide));
        if (!close)
            close = qobject_cast<QAbstractButton *>(bar->tabButton(3, QTabBar::LeftSide));
        QVERIFY(close);
        QCOMPARE(close->accessibleName(), "Close " + editedTitle);
        int count = tabs->count();
        QTimer::singleShot(0, &window, [] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto box = qobject_cast<QMessageBox *>(widget))
                    box->button(QMessageBox::Cancel)->click();
        });
        QTest::mouseClick(close, Qt::LeftButton);
        QCOMPARE(tabs->count(), count);
        QCOMPARE(edited->model->sourceText(), QString("keep these edits"));
        edited->model->text()->setModified(false);
        QCOMPARE(bar->accessibleTabName(3), editedTitle);
        QTest::mouseClick(close, Qt::LeftButton);
        QCOMPARE(tabs->count(), count - 1);
        QVERIFY(edited.isNull());
    }
    void settingsPersistence() {
        QTemporaryDir dir;
        auto path = dir.filePath("settings.ini");
        {
            QSettings s(path, QSettings::IniFormat);
            s.setValue("fontSize", 15);
            s.setValue("schemaVersion", 1);
            s.sync();
            QCOMPARE(s.status(), QSettings::NoError);
        }
        QSettings loaded(path, QSettings::IniFormat);
        QCOMPARE(loaded.value("fontSize").toInt(), 15);
    }
    void viewState() {
        Document d;
        d.text()->setPlainText("alpha\nbeta\ngamma");
        DocumentView first(&d);
        first.split(Qt::Vertical);
        auto editors = first.findChildren<CodeEditor *>();
        auto c = editors[1]->textCursor();
        c.setPosition(9);
        editors[1]->setTextCursor(c);
        auto state = first.viewState();
        DocumentView second(&d);
        second.restoreViewState(state);
        auto restored = second.findChildren<CodeEditor *>();
        QCOMPARE(restored.size(), 2);
        QCOMPARE(restored[1]->textCursor().position(), 9);
    }
    void realSessionRestore() {
        QTemporaryDir dir;
        QFile::remove(Persistence::dataPath("recovery.json"));
        auto p = dir.filePath("session.txt");
        {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("alpha\nbeta\n");
        }
        {
            MainWindow first(false);
            first.setProject(dir.path());
            first.openFile(p);
            QTRY_COMPARE(first.current()->model->path(), QFileInfo(p).canonicalFilePath());
            first.current()->split(Qt::Horizontal);
            QTextCursor c(first.current()->model->text());
            c.setPosition(7);
            first.current()->setCursor(c);
            first.saveSession();
        }
        MainWindow second(true);
        QTRY_VERIFY(second.current());
        QTRY_COMPARE(second.current()->model->path(), QFileInfo(p).canonicalFilePath());
        QCOMPARE(second.current()->findChildren<CodeEditor *>().size(), 2);
        QCOMPARE(second.current()->code->textCursor().position(), 7);
        QFile::remove(Persistence::dataPath("session.json"));
    }
    void gitTemporaryRepo() {
        if (QStandardPaths::findExecutable("git").isEmpty())
            QSKIP("Git not installed");
        QTemporaryDir dir;
        QProcess p;
        p.setWorkingDirectory(dir.path());
        p.start("git", {"init"});
        QVERIFY(p.waitForFinished());
        QCOMPARE(p.exitCode(), 0);
        QFile f(dir.filePath("sample.txt"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("sample");
        f.close();
        p.start("git", {"status", "--porcelain"});
        QVERIFY(p.waitForFinished());
        QVERIFY(p.readAllStandardOutput().contains("?? sample.txt"));
        p.start("git", {"add", "--", "sample.txt"});
        QVERIFY(p.waitForFinished());
        QCOMPARE(p.exitCode(), 0);
        p.start("git", {"diff", "--cached", "--no-ext-diff", "--no-textconv"});
        QVERIFY(p.waitForFinished());
        QVERIFY(p.readAllStandardOutput().contains("+sample"));
    }
    void pdfExport() {
        QTemporaryDir dir;
        SafeDocument doc;
        doc.setMarkdown("# PDF heading\n\nText");
        auto p = dir.filePath("document.pdf");
        QPrinter printer;
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(p);
        doc.print(&printer);
        QFile file(p);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.read(5).startsWith("%PDF-"));
    }

    void exportPreservesSource() {
        QTemporaryDir dir;
        auto path = dir.filePath("source.md");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("# Source");
        }
        Document d(Document::Mode::Markdown);
        d.apply(Document::read(path));
        QString error;
        QVERIFY(!writeExport(d, path, true, &error));
        QCOMPARE(Document::read(path).text, QString("# Source"));
        QVERIFY(!writeExport(d, path, false, &error));
        auto pdf = dir.filePath("output.pdf");
        QVERIFY2(writeExport(d, pdf, true, &error), qPrintable(error));
        QFile file(pdf);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.read(5).startsWith("%PDF-"));
        Document untitled(Document::Mode::Markdown);
        untitled.text()->setPlainText("# Untitled");
        QVERIFY(writeExport(untitled, dir.filePath("untitled.html"), false, &error));
    }
    void focusedSplitCommand() {
        MainWindow window(false);
        window.show();
        auto view = window.current();
        view->code->setPlainText("first\nsecond");
        view->split(Qt::Horizontal);
        auto editors = view->findChildren<CodeEditor *>();
        QTextCursor cursor(view->model->text());
        cursor.setPosition(7);
        editors[1]->setTextCursor(cursor);
        editors[1]->setFocus();
        QTRY_VERIFY(editors[1]->hasFocus());
        window.findChild<QAction *>("duplicate")->trigger();
        QCOMPARE(view->model->sourceText(), QString("first\nsecond\nsecond"));
        view->model->text()->setModified(false);
    }

    void oversizedSnapshotKeepsPrevious() {
        QTemporaryDir dir;
        auto path = dir.filePath("recovery.json");
        QVERIFY(Persistence::write(path, {{"text", "last recoverable snapshot"}}));
        QVERIFY(!Persistence::write(path, {{"text", QString(65 * 1024 * 1024, QLatin1Char('x'))}}));
        QCOMPARE(Persistence::read(path)["text"].toString(), QString("last recoverable snapshot"));
    }
    void syntaxContrast() {
        auto luminance = [](QColor color) {
            auto linear = [](double value) {
                return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
                   0.0722 * linear(color.blueF());
        };
        for (bool light : {false, true}) {
            for (auto language : {QString("Markdown"), QString("C++")}) {
                QTextDocument doc;
                Highlighter syntax(&doc, language);
                doc.setPlainText(language == "Markdown"
                                     ? "# Heading\n`code` **bold** [link](path)"
                                     : "const auto text = \"string\"; int n = 42; // comment");
                syntax.setLightTheme(light);
                syntax.rehighlight();
                int checked = 0;
                for (auto block = doc.begin(); block.isValid(); block = block.next()) {
                    for (const auto &range : block.layout()->formats()) {
                        double foreground = luminance(range.format.foreground().color());
                        for (auto background : {QColor(light ? "#FFFFFF" : "#141B25"),
                                                QColor(light ? "#E8EEF5" : "#202B39")}) {
                            double base = luminance(background);
                            double contrast = (qMax(foreground, base) + 0.05) /
                                              (qMin(foreground, base) + 0.05);
                            QVERIFY2(contrast >= 4.5,
                                     qPrintable(range.format.foreground().color().name() +
                                                " contrast " + QString::number(contrast)));
                        }
                        ++checked;
                    }
                }
                QVERIFY(checked >= 4);
            }
        }
    }
    void liveThemeChange() {
        QSettings settings("StrataEditor", "StrataEditor");
        settings.setValue("light", false);
        MainWindow window(false);
        auto editor = window.current()->code;
        editor->setPlainText("const int count = 42;");
        auto syntax = editor->document()->findChild<Highlighter *>();
        QVERIFY(syntax);
        syntax->setLanguage("C++");
        auto dark = editor->document()->firstBlock().layout()->formats().first().format.foreground();
        window.findChild<QAction *>("theme")->trigger();
        auto light = editor->document()->firstBlock().layout()->formats().first().format.foreground();
        QVERIFY(dark != light);
        QCOMPARE(editor->extraSelections().first().format.background().color(),
                 editor->palette().color(QPalette::AlternateBase));
        window.findChild<QAction *>("theme")->trigger();
        QCOMPARE(editor->document()->firstBlock().layout()->formats().first().format.foreground(), dark);
        window.current()->model->text()->setModified(false);
    }
    void markdownViewModes() {
        Document doc(Document::Mode::Markdown);
        const QString source = "# Heading\n\nA paragraph with **bold** and `code`.\n";
        doc.text()->setPlainText(source);
        auto undoSteps = doc.text()->availableUndoSteps();
        DocumentView view(&doc);
        view.resize(900, 600);
        view.show();
        QVERIFY(view.preview->isReadOnly());
        QCOMPARE(view.preview->document()->documentMargin(), 24.0);
        QCOMPARE(view.preview->document()->firstBlock().blockFormat().lineHeight(), 150.0);
        view.findChild<QToolButton *>("sourceView")->click();
        QCOMPARE(view.markdownView(), DocumentView::MarkdownView::Source);
        QVERIFY(view.preview->isHidden() || view.preview->parentWidget()->isHidden());
        QVERIFY(!view.code->isHidden());
        view.findChild<QToolButton *>("previewView")->click();
        QCoreApplication::processEvents();
        QVERIFY(view.preview->width() >= qMin(view.width() - 64, view.preview->maximumWidth()) - 32);
        QVERIFY(view.code->isHidden());
        QVERIFY(view.preview->isVisible());
        QCOMPARE(view.markdownView(), DocumentView::MarkdownView::Preview);
        DocumentView restored(&doc);
        restored.restoreViewState(view.viewState());
        QCOMPARE(restored.markdownView(), DocumentView::MarkdownView::Preview);
        restored.restoreViewState({{"preview", false}});
        QCOMPARE(restored.markdownView(), DocumentView::MarkdownView::Source);
        restored.restoreViewState({{"markdownView", 999}});
        QCOMPARE(restored.markdownView(), DocumentView::MarkdownView::Split);
        view.split(Qt::Horizontal);
        QCOMPARE(view.markdownView(), DocumentView::MarkdownView::Split);
        auto editors = view.findChildren<CodeEditor *>();
        QCOMPARE(editors.size(), 2);
        view.setMarkdownView(DocumentView::MarkdownView::Preview);
        for (auto editor : editors)
            QVERIFY(editor->isHidden());
        QTextCursor cursor(doc.text());
        cursor.setPosition(3);
        view.setCursor(cursor);
        QCOMPARE(view.markdownView(), DocumentView::MarkdownView::Split);
        QCOMPARE(doc.sourceText(), source);
        QCOMPARE(doc.text()->availableUndoSteps(), undoSteps);
        QCOMPARE(view.preview->toPlainText().contains("Heading"), true);
    }
    void findFromPreview() {
        MainWindow window(false);
        window.show();
        auto view = window.newDocument(Document::Mode::Markdown);
        view->code->setPlainText("# needle");
        view->setMarkdownView(DocumentView::MarkdownView::Preview);
        window.findChild<QAction *>("find")->trigger();
        auto find = window.findChild<QLineEdit *>("findText");
        find->setText("needle");
        QTRY_VERIFY(find->hasFocus());
        QTest::keyClick(find, Qt::Key_Return);
        QCOMPARE(view->markdownView(), DocumentView::MarkdownView::Split);
        QCOMPARE(view->cursor().selectedText(), QString("needle"));
        QVERIFY(find->hasFocus());
        QTest::keyClick(find, Qt::Key_Return);
        QTest::keyClicks(find, " revised query");
        QCOMPARE(view->model->sourceText(), QString("# needle"));
        view->setMarkdownView(DocumentView::MarkdownView::Preview);
        window.findChild<QAction *>("find")->trigger();
        find->setFocus();
        QTRY_VERIFY(find->hasFocus());
        QTest::keyClick(find, Qt::Key_Escape);
        QVERIFY(window.findChild<QWidget *>("findBar")->isHidden());
        QTRY_VERIFY(view->preview->hasFocus());
        window.findChild<QAction *>("selectAll")->trigger();
        window.findChild<QAction *>("copy")->trigger();
        QCOMPARE(QApplication::clipboard()->text(), QString("needle"));
        QCOMPARE(view->markdownView(), DocumentView::MarkdownView::Preview);
        QTest::qWait(250);
        QVERIFY(view->preview->textCursor().hasSelection());
        window.findChild<QAction *>("duplicate")->trigger();
        QCOMPARE(view->markdownView(), DocumentView::MarkdownView::Split);
        QCOMPARE(view->model->sourceText(), QString("# needle\n# needle"));
        view->setMarkdownView(DocumentView::MarkdownView::Preview);
        window.findChild<QAction *>("replace")->trigger();
        find->setText("needle");
        window.findChild<QLineEdit *>("replaceText")->setText("match");
        window.replace(true);
        QVERIFY(find->hasFocus());
        QCOMPARE(view->model->sourceText(), QString("# match\n# match"));
        view->model->text()->setModified(false);
    }
    void progressiveFindAndStatus() {
        MainWindow window(false);
        window.resize(640, 420);
        window.show();
        auto field = window.findChild<QLineEdit *>("findText");
        auto replaceRow = window.findChild<QWidget *>("replaceRow");
        window.findChild<QAction *>("find")->trigger();
        QVERIFY(field->isVisible());
        QVERIFY(replaceRow->isHidden());
        window.findChild<QAction *>("replace")->trigger();
        QVERIFY(replaceRow->isVisible());
        QCoreApplication::processEvents();
        QCOMPARE(window.width(), 640);
        for (auto button : replaceRow->findChildren<QPushButton *>()) {
            QVERIFY(button->isVisible());
            QVERIFY(window.rect().contains(button->mapTo(&window, button->rect().bottomRight())));
        }
        field->setFocus();
        QTest::keyClick(field, Qt::Key_Escape);
        QVERIFY(window.findChild<QWidget *>("findBar")->isHidden());
        QTRY_VERIFY(window.current()->code->hasFocus());
        window.statusBar()->showMessage("Important message");
        window.current()->code->setPlainText("move the cursor");
        auto cursor = window.current()->cursor();
        cursor.setPosition(4);
        window.current()->setCursor(cursor);
        QCOMPARE(window.statusBar()->currentMessage(), QString("Important message"));
        QCOMPARE(window.findChild<QLabel *>("cursorStatus")->text(), QString("Ln 1, Col 5"));
        window.current()->model->text()->setModified(false);
        window.findChild<QAction *>("close")->trigger();
        QVERIFY(window.findChild<QLabel *>("cursorStatus")->text().isEmpty());
        QVERIFY(window.findChild<QLabel *>("documentStatus")->text().isEmpty());
    }
    void persistentExternalWarning() {
        QTemporaryDir dir;
        auto path = dir.filePath("conflict.txt");
        auto write = [&](const QByteArray &text) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(text), text.size());
        };
        write("original");
        MainWindow window(false);
        window.show();
        window.openFile(path);
        QTRY_COMPARE(window.current()->model->path(), QFileInfo(path).canonicalFilePath());
        auto view = window.current();
        view->code->setPlainText("my edits");
        view->model->text()->setModified(true);
        write("external");
        QTRY_VERIFY(view->externallyChanged());
        auto cursor = view->cursor();
        cursor.setPosition(2);
        view->setCursor(cursor);
        QVERIFY(view->externallyChanged());
        auto other = window.newDocument();
        auto tabs = window.findChild<QTabWidget *>("documentTabs");
        tabs->setCurrentWidget(view);
        QVERIFY(view->findChild<QWidget *>("externalWarning")->isVisible());
        QVERIFY(tabs->tabToolTip(tabs->currentIndex()).contains("Changed on disk"));
        QTimer::singleShot(0, [] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto box = qobject_cast<QMessageBox *>(widget))
                    box->done(QMessageBox::Cancel);
        });
        view->findChild<QPushButton *>("reloadExternal")->click();
        QVERIFY(view->externallyChanged());
        QCOMPARE(view->model->sourceText(), QString("my edits"));
        QTimer::singleShot(0, [] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto dialog = qobject_cast<QFileDialog *>(widget))
                    dialog->reject();
        });
        view->findChild<QPushButton *>("saveExternalCopy")->click();
        QVERIFY(view->externallyChanged());
        QTimer::singleShot(0, [] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto box = qobject_cast<QMessageBox *>(widget))
                    box->button(QMessageBox::Discard)->click();
        });
        view->findChild<QPushButton *>("reloadExternal")->click();
        QVERIFY(!view->externallyChanged());
        QCOMPARE(view->model->sourceText(), QString("external"));
        QCOMPARE(other->model->sourceText(), QString());
        view->code->selectAll();
        QTest::keyClicks(view->code, "saved copy");
        write("another external edit");
        QTRY_VERIFY(view->externallyChanged());
        auto copyPath = dir.filePath("copy.txt");
        QTimer::singleShot(0, [copyPath] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto dialog = qobject_cast<QFileDialog *>(widget)) {
                    dialog->selectFile(copyPath);
                    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                }
        });
        view->findChild<QPushButton *>("saveExternalCopy")->click();
        QVERIFY(!view->externallyChanged());
        QCOMPARE(view->model->path(), QFileInfo(copyPath).canonicalFilePath());
        QCOMPARE(Document::read(copyPath).text, QString("saved copy"));
        QCOMPARE(Document::read(path).text, QString("another external edit"));
    }
    void languageDetection() {
        QCOMPARE(Highlighter::languageForPath("a.cpp"), QString("C++"));
        QCOMPARE(Highlighter::languageForPath("a.py"), QString("Python"));
        QCOMPARE(Highlighter::languageForPath("a.md"), QString("Markdown"));
    }
    void guiFindReplace() {
        MainWindow w(false);
        auto v = w.current();
        v->code->setPlainText("one two one");
        w.findChild<QAction *>("find")->trigger();
        auto find = w.findChild<QLineEdit *>("findText");
        find->setText("one");
        QTextCursor c(v->model->text());
        v->setCursor(c);
        w.findNext();
        QCOMPARE(v->cursor().selectedText(), QString("one"));
        w.findChild<QLineEdit *>("replaceText")->setText("three");
        w.replace(true);
        QCOMPARE(v->code->toPlainText(), QString("three two three"));
        v->model->text()->undo();
        QCOMPARE(v->code->toPlainText(), QString("one two one"));
        v->model->text()->setModified(false);
    }
    void asyncOpenSave() {
        QTemporaryDir dir;
        auto p = dir.filePath("file.txt");
        {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("original");
        }
        MainWindow w(false);
        w.openFile(p);
        QTRY_COMPARE(w.current()->model->path(), QFileInfo(p).canonicalFilePath());
        auto v = w.current();
        v->code->selectAll();
        QTest::keyClicks(v->code, "edited");
        QVERIFY(w.saveCurrent());
        QCOMPARE(Document::read(p).text, QString("edited"));
    }
};
QTEST_MAIN(StrataTests)
#include "test_strata.moc"
