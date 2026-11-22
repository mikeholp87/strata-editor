#include "core/document.h"
#include "core/persistence.h"
#include "core/recovery.h"
#include "editor/codeeditor.h"
#include "ui/documentview.h"
#include "ui/mainwindow.h"
#include "ui/projectsearch.h"
#include <QAction>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QtTest>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif
class AuditTests : public QObject {
    Q_OBJECT
    QTemporaryDir paths;
  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
#ifdef Q_OS_UNIX
        QStandardPaths::setTestModeEnabled(false);
        qputenv("XDG_DATA_HOME", paths.path().toUtf8());
#endif
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, paths.path());
    }
    void oversizedSaveKeepsOriginal() {
        QTemporaryDir dir;
        const auto path = dir.filePath("file.txt");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QCOMPARE(f.write("original"), 8);
        }
        Document doc;
        doc.apply(Document::read(path));
        doc.text()->setPlainText(QString(8 * 1024 * 1024 + 1, QLatin1Char('x')));
        QString error;
        QVERIFY(!doc.save(path, &error));
        QVERIFY(doc.isModified());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("original"));
    }
    void savePromptKeepsOriginalTarget() {
        QTemporaryDir dir;
        const auto first = dir.filePath("first.txt");
        const auto second = dir.filePath("second.txt");
        for (auto path : {first, second}) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("old");
        }
        MainWindow window(false);
        window.openFile(first);
        QTRY_COMPARE(window.current()->model->path(), QFileInfo(first).canonicalFilePath());
        auto original = window.current();
        original->code->setPlainText("new content");
        original->model->text()->setModified(true);
        QTimer::singleShot(0, [&] {
            window.openFile(second);
            QTimer *poll = new QTimer(&window);
            connect(poll, &QTimer::timeout, &window, [&, poll] {
                if (window.current() == original)
                    return;
                for (auto widget : QApplication::topLevelWidgets())
                    if (auto box = qobject_cast<QMessageBox *>(widget)) {
                        poll->stop();
                        box->button(QMessageBox::Save)->click();
                    }
            });
            poll->start(1);
        });
        window.findChild<QAction *>("close")->trigger();
        QFile f(first);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("new content"));
    }
    void noSessionKeepsRecovery() {
        auto path = Persistence::dataPath("recovery.json");
        QString error;
        QVERIFY(Recovery::save(path,
                               {{"buffers", QJsonArray{QJsonObject{{"path", ""},
                                                                   {"mode", 0},
                                                                   {"text", "recover me"},
                                                                   {"encoding", "UTF-8"},
                                                                   {"newline", "LF"},
                                                                   {"digest", ""}}}}},
                               &error));
        auto session = Persistence::dataPath("session.json");
        QVERIFY(Persistence::write(session, {{"active", "previous"}}));
        {
            MainWindow window(false, true);
            QVERIFY(window.close());
        }
        QCOMPARE(Persistence::read(session)["active"].toString(), QString("previous"));
        QFile::remove(session);
        QCOMPARE(Recovery::load(path).value["buffers"].toArray()[0].toObject()["text"].toString(),
                 QString("recover me"));
        QVERIFY(QFile::remove(path));
    }
    void projectSearchSupportsUtf16AndBuildAncestor() {
        QTemporaryDir dir;
        QVERIFY(QDir().mkpath(dir.path() + "/build/project"));
        auto root = dir.path() + "/build/project";
        QFile f(root + "/hello.txt");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray::fromHex("fffe680065006c006c006f00"));
        f.close();
        QVERIFY(QDir().mkpath(root + "/build/nested"));
        {
            QFile skipped(root + "/build/nested/skip.txt");
            QVERIFY(skipped.open(QIODevice::WriteOnly));
            skipped.write("hello");
        }
        auto hits = ProjectSearch::search(root, "hello", std::make_shared<std::atomic_bool>(false));
        QCOMPARE(hits.size(), 1);
        QCOMPARE(hits[0].line, 1);
    }
    void replaceHonorsWholeWord() {
        MainWindow window(false);
        auto view = window.current();
        view->code->setPlainText("scatter cat");
        auto cursor = view->code->textCursor();
        cursor.setPosition(1);
        cursor.setPosition(4, QTextCursor::KeepAnchor);
        view->code->setTextCursor(cursor);
        window.findChild<QLineEdit *>("findText")->setText("cat");
        window.findChild<QLineEdit *>("replaceText")->setText("dog");
        window.findChild<QCheckBox *>("wholeWord")->setChecked(true);
        window.replace(false);
        QCOMPARE(view->model->sourceText(), QString("scatter cat"));
    }
    void splitKeepsLineNumberPreference() {
        Document document;
        DocumentView view(&document);
        view.code->numbers = false;
        view.split(Qt::Horizontal);
        const auto editors = view.findChildren<CodeEditor *>();
        QCOMPARE(editors.size(), 2);
        QVERIFY(!editors[1]->numbers);
    }
    void failedSessionWriteIsReported() {
        const auto path = Persistence::dataPath("session.json");
        QFile::remove(path);
        QVERIFY(QDir().mkdir(path));
        bool warned = false;
        MainWindow window(false);
        QTimer::singleShot(0, [&] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto box = qobject_cast<QMessageBox *>(widget)) {
                    warned = true;
                    box->accept();
                }
        });
        window.saveSession();
        QVERIFY(warned);
        QVERIFY(QDir().rmdir(path));
    }
    void lineCommandsAtBlockStart_data() {
        QTest::addColumn<bool>("comment");
        QTest::newRow("indent") << false;
        QTest::newRow("comment") << true;
    }
    void lineCommandsAtBlockStart() {
        QFETCH(bool, comment);
        CodeEditor editor;
        editor.setPlainText("first\nsecond");
        QTextCursor c(editor.document());
        c.setPosition(6);
        editor.setTextCursor(c);
        if (comment)
            editor.toggleComment();
        else
            editor.indent();
        QCOMPARE(editor.toPlainText(), comment ? QString("first\n// second") : QString("first\n    second"));
        editor.undo();
        QCOMPARE(editor.toPlainText(), QString("first\nsecond"));
    }
    void closingBracketReplacesSelection() {
        CodeEditor editor;
        editor.setPlainText("abc)");
        auto c = editor.textCursor();
        c.setPosition(0);
        c.setPosition(3, QTextCursor::KeepAnchor);
        editor.setTextCursor(c);
        QTest::keyClicks(&editor, ")");
        QCOMPARE(editor.toPlainText(), QString("))"));
        editor.undo();
        QCOMPARE(editor.toPlainText(), QString("abc)"));
    }
    void readOnlyCustomKeys() {
        CodeEditor editor;
        editor.setPlainText("keep");
        editor.setReadOnly(true);
        QTest::keyClicks(&editor, "(");
        QTest::keyClick(&editor, Qt::Key_Return);
        QTest::keyClick(&editor, Qt::Key_Tab);
        QCOMPARE(editor.toPlainText(), QString("keep"));
    }

    void readOnlyMenuCommands() {
        MainWindow window(false);
        auto editor = window.current()->code;
        editor->setPlainText("selected");
        editor->selectAll();
        editor->setReadOnly(true);
        window.findChild<QAction *>("uppercase")->trigger();
        QCOMPARE(editor->toPlainText(), QString("selected"));
        window.findChild<QAction *>("duplicate")->trigger();
        QCOMPARE(editor->toPlainText(), QString("selected"));
        window.findChild<QLineEdit *>("findText")->setText("selected");
        window.findChild<QLineEdit *>("replaceText")->setText("changed");
        window.replace(true);
        QCOMPARE(editor->toPlainText(), QString("selected"));
        editor->document()->setModified(false);
    }

    void readOnlyRichFormatting() {
        MainWindow window(false);
        auto view = window.newDocument(Document::Mode::Html);
        view->rich->setPlainText("plain");
        view->rich->selectAll();
        view->rich->setReadOnly(true);
        auto before = view->model->text()->toHtml();
        window.findChild<QAction *>("formatBold")->trigger();
        QCOMPARE(view->model->text()->toHtml(), before);
        view->model->text()->setModified(false);
    }
    void directoryCannotBeOpenedAsText() {
        QTemporaryDir dir;
        QVERIFY(!Document::read(dir.path()).error.isEmpty());
    }
    void fifoCannotBlockOpen() {
#ifdef Q_OS_UNIX
        QTemporaryDir dir;
        auto path = dir.filePath("pipe");
        QCOMPARE(::mkfifo(QFile::encodeName(path).constData(), 0600), 0);
        QVERIFY(!Document::read(path).error.isEmpty());
        QVERIFY(QFile::remove(path));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("original");
        }
        Document document;
        document.apply(Document::read(path));
        QVERIFY(QFile::remove(path));
        QCOMPARE(::mkfifo(QFile::encodeName(path).constData(), 0600), 0);
        QVERIFY(document.changedOnDisk());
        QString error;
        QVERIFY(!document.save(path, &error));
#else
        QSKIP("POSIX FIFO test");
#endif
    }
    void readErrorsAreReported() {
#ifdef Q_OS_LINUX
        QFile probe("/proc/self/mem");
        if (!probe.open(QIODevice::ReadOnly))
            QSKIP("Self-memory probe unavailable");
        probe.readAll();
        QVERIFY(probe.error() != QFileDevice::NoError);
        QVERIFY(!Document::read("/proc/self/mem").error.isEmpty());
        Document document;
        document.restore({{"path", "/proc/self/mem"},
                          {"text", ""},
                          {"digest", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"}});
        QVERIFY(document.changedOnDisk());
#else
        QSKIP("Linux read-error fixture");
#endif
    }

    void invalidEditorTextCannotCorruptSave_data() {
        QTest::addColumn<QByteArray>("initial");
        QTest::addColumn<QString>("text");
        QTest::newRow("utf8-surrogate") << QByteArray("original") << QString(QChar(0xd800));
        QTest::newRow("utf16-surrogate") << QByteArray::fromHex("fffe4100") << QString(QChar(0xd800));
        QTest::newRow("nul") << QByteArray("original") << QString(QChar(0));
    }
    void invalidEditorTextCannotCorruptSave() {
        QFETCH(QByteArray, initial);
        QFETCH(QString, text);
        QTemporaryDir dir;
        const auto path = dir.filePath("text.txt");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(initial);
        }
        Document document;
        document.apply(Document::read(path));
        document.text()->setPlainText(text);
        QCOMPARE(document.sourceText(), text);
        QString error;
        QVERIFY(!document.save(path, &error));
        QVERIFY(!error.isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), initial);
        QVERIFY(document.isModified());
    }

    void bomContentRoundTrip_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("utf8") << QByteArray::fromHex("efbbbfefbbbf68656c6c6f");
        QTest::newRow("utf16le") << QByteArray::fromHex("fffefffe4100");
        QTest::newRow("utf16be") << QByteArray::fromHex("fefffeff0041");
    }
    void bomContentRoundTrip() {
        QFETCH(QByteArray, bytes);
        QTemporaryDir dir;
        const auto path = dir.filePath("bom.txt");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(bytes);
        }
        auto data = Document::read(path);
        QVERIFY(data.error.isEmpty());
        Document document;
        document.apply(data);
        QString error;
        QVERIFY(document.save(path, &error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), bytes);
    }
    void newlineChangeSurvivesTextUndo() {
        QTemporaryDir dir;
        const auto path = dir.filePath("line.txt");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("a\nb\n");
        }
        MainWindow window(false);
        window.openFile(path);
        QTRY_COMPARE(window.current()->model->path(), QFileInfo(path).canonicalFilePath());
        auto model = window.current()->model;
        window.current()->code->insertPlainText("x");
        model->setNewline("CRLF");
        model->text()->undo();
        QCOMPARE(model->sourceText(), QString("a\nb\n"));
        auto tabs = window.findChild<QTabWidget *>("documentTabs");
        QVERIFY(tabs->tabText(tabs->currentIndex()).contains(QChar(0x2022)));
        window.findChild<QAction *>("saveAll")->trigger();
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("a\r\nb\r\n"));
        QVERIFY(!model->isModified());
        model->setNewline("LF");
        window.current()->code->insertPlainText("x");
        model->text()->undo();
        bool prompted = false;
        int count = tabs->count();
        QTimer::singleShot(0, [&] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto box = qobject_cast<QMessageBox *>(widget)) {
                    prompted = true;
                    box->done(QMessageBox::Cancel);
                }
        });
        window.findChild<QAction *>("close")->trigger();
        QVERIFY(prompted);
        QCOMPARE(tabs->count(), count);
        model->setNewline("CRLF");
        QVERIFY(!model->isModified());
    }
};
QTEST_MAIN(AuditTests)
#include "test_audit.moc"
