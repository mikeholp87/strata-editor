#include "core/document.h"
#include "core/export.h"
#include "core/persistence.h"
#include "editor/codeeditor.h"
#include "editor/highlighter.h"
#include "ui/documentview.h"
#include "ui/mainwindow.h"
#include "ui/projectsearch.h"
#include <QFile>
#include <QJsonArray>
#include <QLineEdit>
#include <QMessageBox>
#include <QPrinter>
#include <QProcess>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QtTest>
class StrataTests : public QObject {
    Q_OBJECT
    QTemporaryDir testPaths;
  private slots:
    void initTestCase() {
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
