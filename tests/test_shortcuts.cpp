#include "core/shortcuts.h"
#include "editor/codeeditor.h"
#include "ui/documentview.h"
#include "ui/mainwindow.h"
#include "ui/shortcutdialog.h"
#include <QAction>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>

class ShortcutTests : public QObject {
    Q_OBJECT
    QTemporaryDir testPaths;
    static void select(QTreeWidget *list, const QString &id) {
        for (int i = 0; i < list->topLevelItemCount(); ++i)
            if (list->topLevelItem(i)->data(0, Qt::UserRole).toString() == id) {
                list->setCurrentItem(list->topLevelItem(i));
                return;
            }
        QFAIL("Command not found in shortcut dialog");
    }
  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
#ifdef Q_OS_UNIX
        QStandardPaths::setTestModeEnabled(false);
        qputenv("XDG_DATA_HOME", testPaths.path().toUtf8());
#endif
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, testPaths.path());
    }
    void conflictsAreTransactional() {
        QAction first, second;
        first.setObjectName("first");
        second.setObjectName("second");
        first.setShortcut(QKeySequence("Ctrl+N"));
        second.setShortcut(QKeySequence("Ctrl+O"));
        const QList<QAction *> actions{&first, &second};
        auto map = Shortcuts::defaults(actions);
        QString error;
        map["second"] = QKeySequence("Ctrl+N");
        QVERIFY(!Shortcuts::validate(actions, map, &error));
        QVERIFY(error.contains("first"));
        map["second"] = QKeySequence("Ctrl+N, Ctrl+O");
        QVERIFY(!Shortcuts::validate(actions, map, &error));
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
        QVERIFY(!Shortcuts::apply(settings, actions, map, &error));
        QCOMPARE(second.shortcut(), QKeySequence("Ctrl+O"));
        QVERIFY(!settings.contains("shortcuts/v1"));
        map["second"] = QKeySequence("Ctrl+Alt+O");
        QVERIFY(Shortcuts::apply(settings, actions, map, &error));
        QCOMPARE(second.shortcut(), QKeySequence("Ctrl+Alt+O"));
    }
    void roundTripPersistenceAndUnbind() {
        QTemporaryDir dir;
        QAction action;
        action.setObjectName("new");
        action.setShortcut(QKeySequence("Ctrl+N"));
        action.setProperty("defaultShortcut", action.shortcut());
        QList<QAction *> actions{&action};
        QString error;
        const auto path = dir.filePath("settings.ini");
        auto map = Shortcuts::defaults(actions);
        map["new"] = {};
        {
            QSettings settings(path, QSettings::IniFormat);
            QVERIFY(Shortcuts::apply(settings, actions, map, &error));
            QVERIFY(action.shortcut().isEmpty());
        }
        action.setShortcut(QKeySequence("Ctrl+N"));
        QSettings restored(path, QSettings::IniFormat);
        QVERIFY(Shortcuts::load(restored, actions, &error));
        QVERIFY(action.shortcut().isEmpty());
        auto file = dir.filePath("keys.json");
        QVERIFY(Shortcuts::exportFile(file, actions, map, &error));
        Shortcuts::Mapping imported;
        QVERIFY(Shortcuts::importFile(file, actions, &imported, &error));
        QCOMPARE(imported, map);
        QVERIFY(Shortcuts::apply(restored, actions, Shortcuts::defaults(actions), &error));
        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+N"));
    }
    void invalidImport_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("malformed") << QByteArray("not JSON");
        QTest::newRow("version") << QByteArray(R"({"format":"strata-shortcuts","version":2,"bindings":{}})");
        QTest::newRow("unknown") << QByteArray(
            R"({"format":"strata-shortcuts","version":1,"bindings":{"missing":"Ctrl+N"}})");
        QTest::newRow("invalid-key") << QByteArray(
            R"({"format":"strata-shortcuts","version":1,"bindings":{"new":"Ctrl+NotAKey"}})");
        QTest::newRow("plain-typing")
            << QByteArray(R"({"format":"strata-shortcuts","version":1,"bindings":{"new":"N"}})");
        QTest::newRow("wrong-type") << QByteArray(
            R"({"format":"strata-shortcuts","version":1,"bindings":{"new":4}})");
        QTest::newRow("oversized") << QByteArray(256 * 1024 + 1, ' ');
    }
    void invalidImport() {
        QFETCH(QByteArray, bytes);
        QAction action;
        action.setObjectName("new");
        action.setShortcut(QKeySequence("Ctrl+N"));
        QList<QAction *> actions{&action};
        QString error;
        Shortcuts::Mapping draft{{"sentinel", QKeySequence("F12")}};
        auto previous = draft;
        QVERIFY(!Shortcuts::decode(bytes, actions, &draft, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(draft, previous);
        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+N"));
    }
    void omittedIdsUseDefaults() {
        QAction first, second;
        first.setObjectName("first");
        second.setObjectName("second");
        first.setShortcut(QKeySequence("Ctrl+N"));
        second.setShortcut(QKeySequence("Ctrl+O"));
        second.setProperty("defaultShortcut", second.shortcut());
        second.setShortcut(QKeySequence("F12"));
        QList<QAction *> actions{&first, &second};
        Shortcuts::Mapping map;
        QString error;
        QVERIFY(Shortcuts::decode(
            R"({"format":"strata-shortcuts","version":1,"bindings":{"first":"Ctrl+Alt+N"}})", actions, &map,
            &error));
        QCOMPARE(map["second"], QKeySequence("Ctrl+O"));
    }
    void badSettingsKeepActions() {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
        settings.setValue("shortcuts/v1", "broken");
        QAction action;
        action.setObjectName("new");
        action.setShortcut(QKeySequence("Ctrl+N"));
        QString error;
        QVERIFY(!Shortcuts::load(settings, {&action}, &error));
        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+N"));
    }
    void failedExportKeepsPrevious() {
        QTemporaryDir dir;
        QAction action;
        action.setObjectName("new");
        action.setShortcut(QKeySequence("Ctrl+N"));
        QString error;
        auto path = dir.filePath("keys.json");
        QVERIFY(Shortcuts::exportFile(path, {&action}, Shortcuts::current({&action}), &error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto before = file.readAll();
        file.close();
        QVERIFY(!Shortcuts::exportFile(path, {&action}, {{"new", QKeySequence("N")}}, &error));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), before);
        QVERIFY(!Shortcuts::exportFile(dir.filePath("missing/keys.json"), {&action},
                                       Shortcuts::current({&action}), &error));
    }
    void dialogCancelAndConflict() {
        QAction first, second;
        first.setObjectName("first");
        second.setObjectName("second");
        first.setText("First command");
        second.setText("Second command");
        first.setShortcut(QKeySequence("Ctrl+N"));
        second.setShortcut(QKeySequence("Ctrl+O"));
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
        ShortcutDialog dialog({&first, &second}, settings);
        dialog.show();
        auto list = dialog.findChild<QTreeWidget *>("shortcutCommands");
        select(list, "first");
        auto recorder = dialog.findChild<QKeySequenceEdit *>("shortcutRecorder");
        recorder->setKeySequence(QKeySequence("Ctrl+O"));
        QVERIFY(!dialog.findChild<QPushButton *>("saveShortcuts")->isEnabled());
        QVERIFY(dialog.findChild<QLabel *>("shortcutMessage")->text().contains("Conflicting"));
        QCOMPARE(first.shortcut(), QKeySequence("Ctrl+N"));
        QVERIFY(!settings.contains("shortcuts/v1"));
        recorder->clear();
        recorder->setFocus();
        QTest::keyClick(recorder, Qt::Key_N, Qt::ControlModifier | Qt::AltModifier);
        QTRY_COMPARE(recorder->keySequence(), QKeySequence("Ctrl+Alt+N"));
        QVERIFY(dialog.findChild<QPushButton *>("saveShortcuts")->isEnabled());
        auto filter = dialog.findChild<QLineEdit *>("shortcutFilter");
        filter->setText("second");
        QCOMPARE(list->currentItem()->data(0, Qt::UserRole).toString(), QString("second"));
        QTest::mouseClick(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel),
                          Qt::LeftButton);
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QCOMPARE(first.shortcut(), QKeySequence("Ctrl+N"));
        QVERIFY(!settings.contains("shortcuts/v1"));
    }
    void remapNewAndRestart() {
        QSettings preferences("StrataEditor", "StrataEditor");
        preferences.remove("shortcuts");
        preferences.sync();
        {
            MainWindow window(false);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            auto actions = window.findChildren<QAction *>();
            QList<QAction *> registered;
            for (auto action : actions)
                if (action->property("defaultShortcut").isValid())
                    registered.append(action);
            ShortcutDialog dialog(registered, preferences, &window);
            dialog.show();
            select(dialog.findChild<QTreeWidget *>("shortcutCommands"), "new");
            dialog.findChild<QKeySequenceEdit *>("shortcutRecorder")
                ->setKeySequence(QKeySequence("Ctrl+Alt+N"));
            QVERIFY2(dialog.findChild<QPushButton *>("saveShortcuts")->isEnabled(),
                     qPrintable(dialog.findChild<QLabel *>("shortcutMessage")->text()));
            QTest::mouseClick(dialog.findChild<QPushButton *>("saveShortcuts"), Qt::LeftButton);
            QVERIFY2(dialog.result() == int(QDialog::Accepted),
                     qPrintable(dialog.findChild<QLabel *>("shortcutMessage")->text()));
            auto action = window.findChild<QAction *>("new");
            QCOMPARE(action->shortcut(), QKeySequence("Ctrl+Alt+N"));
            auto tabs = window.findChild<QTabWidget *>("documentTabs");
            int before = tabs->count();
            window.activateWindow();
            window.current()->code->setFocus();
            QTRY_VERIFY(window.current()->code->hasFocus());
            QTest::keyClick(window.current()->code, Qt::Key_N, Qt::ControlModifier | Qt::AltModifier);
            QTRY_COMPARE(tabs->count(), before + 1);
            action->trigger();
            QCOMPARE(tabs->count(), before + 2);
        }
        {
            MainWindow restarted(false);
            QCOMPARE(restarted.findChild<QAction *>("new")->shortcut(), QKeySequence("Ctrl+Alt+N"));
        }
        preferences.remove("shortcuts");
        preferences.sync();
    }
    void editorShortcutOverrideAndChord() {
        QSettings preferences("StrataEditor", "StrataEditor");
        preferences.remove("shortcuts");
        preferences.sync();
        MainWindow window(false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QList<QAction *> actions;
        for (auto action : window.findChildren<QAction *>())
            if (action->property("defaultShortcut").isValid())
                actions.append(action);
        QString error;
        auto map = Shortcuts::current(actions);
        QVERIFY2(Shortcuts::validate(actions, map, &error), qPrintable(error));
        map["new"] = QKeySequence("Ctrl+Left");
        QVERIFY(Shortcuts::apply(preferences, actions, map, &error));
        auto tabs = window.findChild<QTabWidget *>("documentTabs");
        int before = tabs->count();
        window.activateWindow();
        window.current()->code->setFocus();
        QTRY_VERIFY(window.current()->code->hasFocus());
        QTest::keyClick(window.current()->code, Qt::Key_Left, Qt::ControlModifier);
        QTRY_COMPARE(tabs->count(), before + 1);
        map["new"] = QKeySequence("Ctrl+K, Ctrl+N");
        QVERIFY(Shortcuts::apply(preferences, actions, map, &error));
        QTest::keyClick(window.current()->code, Qt::Key_K, Qt::ControlModifier);
        QCOMPARE(tabs->count(), before + 1);
        QTest::keyClick(window.current()->code, Qt::Key_N, Qt::ControlModifier);
        QTRY_COMPARE(tabs->count(), before + 2);
        auto rich = window.newDocument(Document::Mode::Html);
        map["new"] = QKeySequence("Ctrl+Left");
        QVERIFY(Shortcuts::apply(preferences, actions, map, &error));
        rich->rich->setFocus();
        QTRY_VERIFY(rich->rich->hasFocus());
        int count = tabs->count();
        QTest::keyClick(rich->rich, Qt::Key_Left, Qt::ControlModifier);
        QTRY_COMPARE(tabs->count(), count + 1);
        preferences.remove("shortcuts");
        preferences.sync();
    }

    void nativeEditingActionsRemainUsable() {
        QSettings preferences("StrataEditor", "StrataEditor");
        preferences.remove("shortcuts");
        preferences.sync();
        MainWindow window(false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        auto editor = window.current()->code;
        editor->setPlainText("safe");
        QApplication::clipboard()->setText("keep clipboard");
        editor->setFocus();
        QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("keep clipboard"));
        QTest::keyClick(editor, Qt::Key_X, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("keep clipboard"));
        QCOMPARE(editor->toPlainText(), QString("safe"));
        editor->selectAll();
        editor->setFocus();
        QTRY_VERIFY(editor->hasFocus());
        QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("safe"));
        auto cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor->setTextCursor(cursor);
        QTest::keyClick(editor, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(editor->toPlainText(), QString("safesafe"));
        QTest::keyClick(editor, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(editor->toPlainText(), QString("safe"));
        editor->document()->setModified(false);
    }

    void richCopyKeepsFormatting() {
        QSettings preferences("StrataEditor", "StrataEditor");
        preferences.remove("shortcuts");
        preferences.sync();
        MainWindow window(false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        auto view = window.newDocument(Document::Mode::Html);
        view->rich->setHtml("<b>formatted</b>");
        view->rich->selectAll();
        view->rich->setFocus();
        QTRY_VERIFY(view->rich->hasFocus());
        QTest::keyClick(view->rich, Qt::Key_C, Qt::ControlModifier);
        QVERIFY(QApplication::clipboard()->mimeData()->hasHtml());
        QTextDocument pasted;
        pasted.setHtml(QApplication::clipboard()->mimeData()->html());
        QTextCursor c(&pasted);
        c.select(QTextCursor::Document);
        QCOMPARE(c.charFormat().fontWeight(), int(QFont::Bold));
        QTest::keyClick(view->rich, Qt::Key_X, Qt::ControlModifier);
        QVERIFY(view->model->text()->toPlainText().isEmpty());
        QVERIFY(QApplication::clipboard()->mimeData()->hasHtml());
        view->model->text()->undo();
        QCOMPARE(view->model->text()->toPlainText(), QString("formatted"));
        view->model->text()->setModified(false);
    }
    void applicationDialogCommand() {
        QSettings preferences("StrataEditor", "StrataEditor");
        preferences.remove("shortcuts");
        preferences.sync();
        MainWindow window(false);
        window.show();
        bool opened = false;
        QTimer::singleShot(0, &window, [&] {
            auto dialog = window.findChild<ShortcutDialog *>("shortcutDialog");
            opened = dialog != nullptr;
            if (dialog) {
                const auto path = qEnvironmentVariable("STRATA_SHORTCUT_SCREENSHOT");
                if (!path.isEmpty()) {
                    dialog->grab().save(path);
                    dialog->resize(640, 420);
                    dialog->grab().save(path + ".small.png");
                }
                dialog->reject();
            }
        });
        window.findChild<QAction *>("shortcuts")->trigger();
        QVERIFY(opened);
    }
    void dialogImportExportDraft() {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
        QAction action;
        action.setObjectName("new");
        action.setText("New");
        action.setShortcut(QKeySequence("Ctrl+N"));
        QString error;
        auto imported = dir.filePath("import.json");
        QVERIFY(Shortcuts::exportFile(imported, {&action}, {{"new", QKeySequence("Ctrl+Alt+N")}}, &error));
        ShortcutDialog dialog({&action}, settings);
        dialog.show();
        auto choose = [&](const QString &path) {
            QTimer::singleShot(0, &dialog, [&, path] {
                auto picker = dialog.findChild<QFileDialog *>();
                if (picker) {
                    picker->selectFile(path);
                    QMetaObject::invokeMethod(picker, "accept", Qt::QueuedConnection);
                }
            });
        };
        choose(imported);
        QTest::mouseClick(dialog.findChild<QPushButton *>("importShortcuts"), Qt::LeftButton);
        QCOMPARE(dialog.findChild<QKeySequenceEdit *>("shortcutRecorder")->keySequence(),
                 QKeySequence("Ctrl+Alt+N"));
        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+N"));
        auto exported = dir.filePath("export.json");
        choose(exported);
        QTest::mouseClick(dialog.findChild<QPushButton *>("exportShortcuts"), Qt::LeftButton);
        Shortcuts::Mapping loaded;
        QVERIFY(Shortcuts::importFile(exported, {&action}, &loaded, &error));
        QCOMPARE(loaded["new"], QKeySequence("Ctrl+Alt+N"));
        dialog.reject();
        QVERIFY(!settings.contains("shortcuts/v1"));
        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+N"));
    }
    void settingsFailureDoesNotApply() {
        QTemporaryDir dir;
        auto blocker = dir.filePath("blocker");
        {
            QFile file(blocker);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("file");
        }
        QSettings settings(blocker + "/settings.ini", QSettings::IniFormat);
        QAction action;
        action.setObjectName("new");
        action.setShortcut(QKeySequence("Ctrl+N"));
        QString error;
        QVERIFY(!Shortcuts::apply(settings, {&action}, {{"new", QKeySequence("Ctrl+Alt+N")}}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+N"));
    }

    void resetDraftAndSave() {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
        QAction action;
        action.setObjectName("new");
        action.setText("New");
        action.setProperty("defaultShortcut", QKeySequence("Ctrl+N"));
        action.setShortcut(QKeySequence("Ctrl+Alt+N"));
        ShortcutDialog dialog({&action}, settings);
        dialog.show();
        QTest::mouseClick(dialog.findChild<QPushButton *>("resetAllShortcuts"), Qt::LeftButton);
        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+Alt+N"));
        QTest::mouseClick(dialog.findChild<QPushButton *>("saveShortcuts"), Qt::LeftButton);
        QCOMPARE(action.shortcut(), QKeySequence("Ctrl+N"));
    }
};
QTEST_MAIN(ShortcutTests)
#include "test_shortcuts.moc"
