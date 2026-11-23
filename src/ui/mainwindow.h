#pragma once
#include "core/document.h"
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QMainWindow>
#include <QSettings>
class DocumentView;
class QTabWidget;
class QLineEdit;
class QCheckBox;
class QToolBar;
class QDockWidget;
class QFileSystemModel;
class QTreeView;
class QLabel;
class ProjectSearch;
class GitPanel;
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(bool restoreWorkspace = true, bool preserveRecovery = false);
    DocumentView *current() const;
    DocumentView *newDocument(Document::Mode mode = Document::Mode::Text);
    void openFile(const QString &path, int line = 0);
    bool saveCurrent(bool saveAs = false);
    void setProject(const QString &path);
    void findNext(bool backwards = false);
    void replace(bool all = false);
    void saveSession();

  protected:
    void closeEvent(QCloseEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;

  private:
    QSettings settings;
    QTabWidget *tabs;
    QToolBar *formatBar;
    QToolBar *markdownBar = nullptr;
    QToolBar *focusExitBar;
    QByteArray focusLayout;
    bool focusMode = false;
    QWidget *findBar, *replaceRow;
    QLabel *cursorStatus, *documentStatus;
    QLineEdit *findText, *replaceText;
    QCheckBox *matchCase, *wholeWord, *regex;
    QDockWidget *explorerDock, *searchDock, *gitDock;
    QFileSystemModel *files;
    QTreeView *tree;
    ProjectSearch *projectSearch;
    GitPanel *git;
    QFileSystemWatcher watcher;
    QString project;
    QStringList recent, closed;
    QList<QAction *> commands;
    QSet<QString> loading;
    QHash<QString, QJsonObject> pendingViews;
    QString pendingActive;
    bool shuttingDown = false;
    bool recoveryBlocked = false;
    bool preserveRecovery = false;
    DocumentView *attach(Document *doc);
    QAction *command(const QString &id, const QString &title, const QKeySequence &key,
                     const std::function<void()> &fn, QMenu *menu = nullptr);
    void buildCommands();
    void buildFormatBar();
    void updateUi();
    void updateTheme();
    void configureViews();
    bool closeTab(int index);
    bool saveView(DocumentView *view, bool saveAs = false);
    bool canClose(DocumentView *view);
    void palette(bool quickOpen = false);
    void preferences();
    void editShortcuts();
    void recoverySnapshot();
    void restoreSession();
    void reloadCurrent();
    void exportPdf();
    void exportHtml();
};
