#include "core/persistence.h"
#include "editor/codeeditor.h"
#include "ui/documentview.h"
#include "ui/mainwindow.h"
#include <QtTest>
#include <QtWidgets>

class UsabilityTests : public QObject {
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
    void init() {
        QSettings settings("StrataEditor", "StrataEditor");
        settings.clear();
        settings.setValue("fontSize", 12);
        settings.setValue("sidebarFontSize", 10);
        settings.sync();
    }
    void lastTabAndCloseAllRemainUsable() {
        MainWindow window(false);
        auto tabs = window.findChild<QTabWidget *>("documentTabs");
        window.findChild<QAction *>("close")->trigger();
        QCOMPARE(tabs->count(), 1);
        QVERIFY(window.current());
        QVERIFY(window.current()->model->sourceText().isEmpty());
        QVERIFY(!window.current()->model->isModified());
        window.newDocument();
        window.newDocument();
        window.findChild<QAction *>("closeAll")->trigger();
        QCOMPARE(tabs->count(), 1);
        QTest::keyClicks(window.current()->code, "still editable");
        QCOMPARE(window.current()->model->sourceText(), QString("still editable"));
    }
    void splitClosePreservesDocument_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("text") << 0;
        QTest::newRow("markdown") << 1;
        QTest::newRow("rich-text") << 2;
    }
    void splitClosePreservesDocument() {
        QFETCH(int, mode);
        Document document{Document::Mode(mode)};
        document.text()->setPlainText("keep this text");
        DocumentView view(&document);
        view.resize(800, 500);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.split(Qt::Horizontal);
        const auto modified = document.isModified();
        QWidget *primary = view.code ? static_cast<QWidget *>(view.code) : view.rich;
        QPointer<QWidget> old = primary;
        auto close = primary->parentWidget()->findChild<QToolButton *>("closeSplitView");
        QVERIFY(close);
        QVERIFY(close->isVisible());
        close->click();
        QVERIFY(old.isNull());
        QCOMPARE(document.sourceText(), QString("keep this text"));
        QCOMPARE(document.isModified(), modified);
        QCOMPARE(view.viewState()["views"].toArray().size(), 1);
        auto cursor = view.cursor();
        cursor.insertText("X");
        document.text()->undo();
        QCOMPARE(document.sourceText(), QString("keep this text"));
        view.split(Qt::Vertical);
        view.split(Qt::Vertical);
        view.closeExtraSplits();
        QCOMPARE(view.viewState()["views"].toArray().size(), 1);
        QVERIFY(view.findChild<QWidget *>("splitHeader")->isHidden());
        if (mode == 1)
            QVERIFY(view.preview);
    }
    void focusHasVisibleExitAndRestoresPanels() {
        MainWindow window(false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto project = window.findChild<QDockWidget *>("explorerDock");
        auto git = window.findChild<QDockWidget *>("gitDock");
        project->hide();
        git->show();
        const auto layout = window.saveState();
        auto action = window.findChild<QAction *>("focus");
        action->trigger();
        QVERIFY(window.menuBar()->isHidden());
        QVERIFY(project->isHidden());
        QVERIFY(git->isHidden());
        auto exit = window.findChild<QToolButton *>("exitFocusMode");
        QVERIFY(exit->isVisible());
        window.newDocument(Document::Mode::Html);
        for (auto bar : window.findChildren<QToolBar *>())
            if (bar->objectName() != "focusExitBar")
                QVERIFY(bar->isHidden());
        window.saveSession();
        QSettings settings("StrataEditor", "StrataEditor");
        QCOMPARE(settings.value("layout").toByteArray(), layout);
        exit->click();
        QVERIFY(window.menuBar()->isVisible());
        QVERIFY(!exit->isVisible());
        QVERIFY(project->isHidden());
        QVERIFY(git->isVisible());
        QVERIFY(!action->isChecked());
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        window.current()->rich->setFocus();
        QTRY_VERIFY(window.current()->rich->hasFocus());
        QTest::keyClick(window.current()->rich, Qt::Key_F11);
        QVERIFY(window.menuBar()->isHidden());
        QTest::keyClick(window.current()->rich, Qt::Key_F11);
        QVERIFY(window.menuBar()->isVisible());
    }
    void fontShortcutsRespectFocusedPaneAndPersist() {
        {
            MainWindow window(false);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            window.activateWindow();
            auto editor = window.current()->code;
            editor->setFocus();
            QTRY_VERIFY(editor->hasFocus());
            QTest::keyClick(editor, Qt::Key_Plus, Qt::ControlModifier);
            QCOMPARE(editor->font().pointSize(), 13);
            QTest::keyClick(editor, Qt::Key_Minus, Qt::ControlModifier);
            QCOMPARE(editor->font().pointSize(), 12);
            QVERIFY(window.current()->model->sourceText().isEmpty());
            QVERIFY(!window.current()->model->isModified());
            auto tree = window.findChild<QTreeView *>("projectTree");
            tree->setFocus();
            QTRY_VERIFY(tree->hasFocus());
            QTest::keyClick(tree, Qt::Key_Plus, Qt::ControlModifier);
            QCOMPARE(tree->font().pointSize(), 11);
            QCOMPARE(editor->font().pointSize(), 12);
        }
        MainWindow reopened(false);
        QCOMPARE(reopened.findChild<QTreeView *>("projectTree")->font().pointSize(), 11);
        QCOMPARE(reopened.current()->code->font().pointSize(), 12);
    }
    void sidebarPreferencesApplyWithoutChangingEditor() {
        MainWindow window(false);
        bool found = false;
        QTimer::singleShot(0, [&] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto dialog = qobject_cast<QDialog *>(widget))
                    if (auto size = dialog->findChild<QSpinBox *>("sidebarFontSize")) {
                        found = true;
                        size->setValue(18);
                        dialog->accept();
                    }
        });
        window.findChild<QAction *>("settings")->trigger();
        QVERIFY(found);
        QCOMPARE(window.findChild<QTreeView *>("projectTree")->font().pointSize(), 18);
        QCOMPARE(window.current()->code->font().pointSize(), 12);
    }
};
QTEST_MAIN(UsabilityTests)
#include "test_usability.moc"
