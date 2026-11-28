#include "core/document.h"
#include "core/instancebridge.h"
#include "core/persistence.h"
#include "core/recovery.h"
#include "ui/documentview.h"
#include "ui/gitpanel.h"
#include "ui/mainwindow.h"
#include <QTemporaryDir>
#include <QtTest>
#include <QtWidgets>
class HardeningTests : public QObject {
    Q_OBJECT
    QTemporaryDir paths;
    QByteArray originalPath;
    static QJsonObject snapshot(const QString &text) {
        return {{"buffers", QJsonArray{QJsonObject{{"mode", 0},
                                                   {"path", ""},
                                                   {"text", text},
                                                   {"encoding", "UTF-8"},
                                                   {"newline", "LF"},
                                                   {"digest", ""}}}}};
    }
    static void writeMode(const QString &root, const QByteArray &mode) {
        QFile f(root + "/.fake-git-mode");
        if (f.open(QIODevice::WriteOnly))
            f.write(mode);
    }
    static void installFake(const QString &root) {
#ifdef Q_OS_WIN
        QString name = "/git.exe";
#else
        QString name = "/git";
#endif
        QVERIFY(QFile::copy(QString::fromLocal8Bit(STRATA_FAKE_GIT_PATH), root + name));
        QFile::setPermissions(root + name,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
#ifdef Q_OS_UNIX
        QStandardPaths::setTestModeEnabled(false);
        qputenv("XDG_DATA_HOME", paths.path().toUtf8());
#endif
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, paths.path());
    }
    void init() {
        originalPath = qgetenv("PATH");
    }
    void cleanup() {
        qputenv("PATH", originalPath);
    }
    void secondLaunchHandsFilesToTheRunningInstance() {
        QTemporaryDir dir;
        const auto requests = dir.filePath("open-requests");
        InstanceBridge bridge(requests);
        QSignalSpy spy(&bridge, &InstanceBridge::requested);
        QString error;
        const auto other = QDir::temp().absoluteFilePath("other.md"); // "/abs/..." is not absolute on Windows
        QVERIFY(InstanceBridge::send(requests, {"relative.txt", other}, &error));
        QTRY_COMPARE(spy.count(), 1);
        const auto paths = spy.takeFirst().at(0).toStringList();
        QCOMPARE(paths.size(), 2);
        QCOMPARE(paths[0], QFileInfo("relative.txt").absoluteFilePath());
        QCOMPARE(paths[1], other);
        QVERIFY(QDir(requests).entryList({"*.request"}, QDir::Files).isEmpty()); // consumed exactly once
        QVERIFY(InstanceBridge::send(requests, {}, &error)); // a bare launch just asks to be shown
        QTRY_COMPARE(spy.count(), 1);
        QVERIFY(spy.takeFirst().at(0).toStringList().isEmpty());
    }
    void staleRequestsAreDiscardedNotExecuted() {
        QTemporaryDir dir;
        const auto requests = dir.filePath("open-requests");
        QVERIFY(QDir().mkpath(requests));
        QFile old(requests + "/old.request");
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write(R"(["/etc/hosts"])");
        old.flush();
        QVERIFY(old.setFileTime(QDateTime::currentDateTime().addSecs(-3600), QFileDevice::FileModificationTime));
        old.close();
        InstanceBridge bridge(requests);
        QSignalSpy spy(&bridge, &InstanceBridge::requested);
        QTest::qWait(100);
        QCOMPARE(spy.count(), 0);
        QVERIFY(!QFileInfo::exists(requests + "/old.request"));
    }
    void foreignHtmlIsFlaggedUntilSaved() {
        QTemporaryDir dir;
        const auto path = dir.filePath("page.html");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("<!DOCTYPE html><html><head><script>var x=1;</script></head><body><p>hi</p></body></html>");
        f.close();
        Document doc(Document::Mode::Html);
        doc.apply(Document::read(path));
        QVERIFY(doc.foreignHtml());
        QString error;
        QVERIFY(doc.save(path, &error));
        QVERIFY(!doc.foreignHtml());
        Document again(Document::Mode::Html); // now written by this editor: no warning on later saves
        again.apply(Document::read(path));
        QVERIFY(!again.foreignHtml());
        Document text(Document::Mode::Text);
        text.apply(Document::read(path));
        QVERIFY(!text.foreignHtml());
    }
    void recoveryRejectsInvalidStructure() {
        QTemporaryDir dir;
        auto path = dir.filePath("recovery.json");
        QVERIFY(Persistence::write(path, {{"buffers", QJsonArray{QJsonObject{{"text", "keep"}}}}}));
        QVERIFY(!Recovery::load(path).error.isEmpty());
        auto directory = dir.filePath("directory");
        QVERIFY(QDir().mkdir(directory));
        QString error;
        QVERIFY(Recovery::preserveInvalid(directory, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(QFileInfo(directory).isDir());
        QVERIFY(!Recovery::save(directory, snapshot("new"), &error));
    }
    void restoresBackupAfterCorruption() {
        const auto path = Persistence::dataPath("recovery.json");
        QFile::remove(path);
        for (const auto &backup : Recovery::backupPaths(path))
            QFile::remove(backup);
        QString error;
        QVERIFY(Recovery::save(path, snapshot("older"), &error));
        QVERIFY(Recovery::save(path, snapshot("newer"), &error));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("corrupt primary");
        }
        QSettings settings("StrataEditor", "StrataEditor");
        settings.setValue("restore", false);
        int warnings = 0, questions = 0;
        bool snapshotsRunningDuringRestore = false;
        QTimer responder;
        connect(&responder, &QTimer::timeout, this, [&] {
            for (auto window : QApplication::topLevelWidgets())
                if (qobject_cast<MainWindow *>(window))
                    for (auto timer : window->findChildren<QTimer *>())
                        if (timer->interval() == 15000 && timer->isActive())
                            snapshotsRunningDuringRestore = true;
            for (auto widget : QApplication::topLevelWidgets())
                if (auto box = qobject_cast<QMessageBox *>(widget)) {
                    if (box->icon() == QMessageBox::Question) {
                        ++questions;
                        box->button(QMessageBox::Yes)->click();
                    } else {
                        ++warnings;
                        box->button(QMessageBox::Ok)->click();
                    }
                }
        });
        responder.start(10);
        MainWindow window(true);
        responder.stop();
        QVERIFY(!snapshotsRunningDuringRestore);
        QCOMPARE(warnings, 1);
        QCOMPARE(questions, 1);
        QCOMPARE(window.current()->model->sourceText(), QString("older"));
        QVERIFY(window.current()->model->isModified());
        QDir directory(QFileInfo(path).absolutePath());
        QCOMPARE(directory.entryList({"recovery.json.corrupt.*"}, QDir::Files).size(), 1);
        for (const auto &backup : Recovery::backupPaths(path))
            QFile::remove(backup);
    }
    void gitMissingAndRealRepository() {
        const auto executable = QStandardPaths::findExecutable("git");
        QTemporaryDir dir;
        GitPanel panel;
        panel.root = dir.path();
        qputenv("PATH", QByteArray());
        panel.refresh();
        QVERIFY(!panel.isBusy());
        QVERIFY(panel.findChild<QPlainTextEdit *>()->toPlainText().contains("not found"));
        qputenv("PATH", originalPath);
        if (executable.isEmpty())
            QSKIP("Git unavailable for real repository case");
        QProcess git;
        git.setWorkingDirectory(dir.path());
        git.start(executable, {"init"});
        QVERIFY(git.waitForFinished());
        QCOMPARE(git.exitCode(), 0);
        {
            QFile f(dir.filePath("untracked.txt"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("text");
        }
        panel.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!panel.isBusy(), 5000);
        auto output = panel.findChild<QPlainTextEdit *>()->toPlainText();
        QVERIFY(output.contains("untracked.txt"));
        QVERIFY(output.contains("code 0"));
    }
    void gitWritesFinishWithoutCancellation() {
        QTemporaryDir dir;
        installFake(dir.path());
        qputenv("PATH", QFile::encodeName(dir.path()) + QDir::listSeparator().toLatin1() + originalPath);
        writeMode(dir.path(), "delay");
        GitPanel panel;
        panel.root = dir.path();
        QTimer::singleShot(0, [] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto dialog = qobject_cast<QInputDialog *>(widget)) {
                    dialog->setTextValue("file.txt");
                    dialog->accept();
                }
        });
        for (auto button : panel.findChildren<QPushButton *>())
            if (button->text().startsWith("Stage file"))
                button->click();
        QVERIFY(panel.isBusy());
        panel.cancel();
        QVERIFY(panel.isBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!panel.isBusy(), 5000);
        QVERIFY(!panel.findChild<QPlainTextEdit *>()->toPlainText().contains("cancelled"));
    }
    void corruptDataIsReportedAndPreserved() {
        QTemporaryDir dir;
        auto path = dir.filePath("recovery.json");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("broken recovery bytes");
        }
        auto result = Persistence::readChecked(path);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(!result.missing);
        QString error;
        auto preserved = Recovery::preserveInvalid(path, &error);
        QVERIFY2(!preserved.isEmpty(), qPrintable(error));
        QFile f(preserved);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("broken recovery bytes"));
        QVERIFY(!QFileInfo::exists(path));
    }
    void backupGenerationsAndFailures() {
        QTemporaryDir dir;
        auto path = dir.filePath("recovery.json");
        QString error;
        for (auto text : {"A", "B", "C", "D"})
            QVERIFY2(Recovery::save(path, snapshot(text), &error), qPrintable(error));
        auto backups = Recovery::backupPaths(path);
        QCOMPARE(backups.size(), 3);
        QStringList expected{"C", "B", "A"};
        for (int i = 0; i < 3; ++i)
            QCOMPARE(Recovery::load(backups[i]).value["buffers"].toArray()[0].toObject()["text"].toString(),
                     expected[i]);
        QVERIFY(QFile::remove(backups[2]));
        QVERIFY(QDir().mkdir(backups[2]));
        QVERIFY(!Recovery::save(path, snapshot("E"), &error));
        QCOMPARE(Recovery::load(path).value["buffers"].toArray()[0].toObject()["text"].toString(),
                 QString("D"));
    }
    void invalidRecoveryCannotOverwriteGoodBackup() {
        QTemporaryDir dir;
        auto path = dir.filePath("recovery.json");
        QString error;
        QVERIFY(Recovery::save(path, snapshot("A"), &error));
        QVERIFY(Recovery::save(path, snapshot("B"), &error));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("corrupt");
        }
        QVERIFY(!Recovery::save(path, snapshot("C"), &error));
        QCOMPARE(Recovery::load(Recovery::backupPaths(path)[0])
                     .value["buffers"]
                     .toArray()[0]
                     .toObject()["text"]
                     .toString(),
                 QString("A"));
    }
    void gitOutputIsBounded() {
        QTemporaryDir dir;
        installFake(dir.path());
        auto previous = qgetenv("PATH");
        qputenv("PATH", QFile::encodeName(dir.path()) + QDir::listSeparator().toLatin1() + previous);
        writeMode(dir.path(), "flood");
        GitPanel panel;
        panel.root = dir.path();
        panel.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!panel.isBusy(), 5000);
        qputenv("PATH", previous);
        auto output = panel.findChild<QPlainTextEdit *>()->toPlainText();
        QVERIFY(output.contains("truncated", Qt::CaseInsensitive));
        QVERIFY(output.size() < 1100000);
    }
    void gitUnicodeAndCancellation() {
        QTemporaryDir dir;
        installFake(dir.path());
        auto previous = qgetenv("PATH");
        qputenv("PATH", QFile::encodeName(dir.path()) + QDir::listSeparator().toLatin1() + previous);
        writeMode(dir.path(), "unicode");
        GitPanel panel;
        panel.root = dir.path();
        panel.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!panel.isBusy(), 5000);
        auto output = panel.findChild<QPlainTextEdit *>()->toPlainText();
        QVERIFY(output.contains(QString::fromUtf8("🙂")));
        QVERIFY(!output.contains(QChar::ReplacementCharacter));
        writeMode(dir.path(), "wait");
        panel.refresh();
        QTRY_VERIFY(panel.isBusy());
        panel.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!panel.isBusy(), 3000);
        qputenv("PATH", previous);
    }
    void closingWaitsForGit() {
        QTemporaryDir dir;
        installFake(dir.path());
        auto previous = qgetenv("PATH");
        qputenv("PATH", QFile::encodeName(dir.path()) + QDir::listSeparator().toLatin1() + previous);
        writeMode(dir.path(), "wait");
        MainWindow window(false);
        auto panel = window.findChild<GitPanel *>();
        panel->root = dir.path();
        panel->refresh();
        QTRY_VERIFY(panel->isBusy());
        QVERIFY(!window.close());
        panel->cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!panel->isBusy(), 3000);
        QVERIFY(window.close());
        qputenv("PATH", previous);
    }
};
QTEST_MAIN(HardeningTests)
#include "test_hardening.moc"
