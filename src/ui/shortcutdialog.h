#pragma once
#include "core/shortcuts.h"
#include <QDialog>
class QSettings;
class QTreeWidget;
class QTreeWidgetItem;
class QKeySequenceEdit;
class QLabel;
class QPushButton;
class ShortcutDialog : public QDialog {
    Q_OBJECT
  public:
    ShortcutDialog(const QList<QAction *> &actions, QSettings &settings, QWidget *parent = nullptr);

  private:
    QList<QAction *> actions;
    QSettings &settings;
    Shortcuts::Mapping draft, defaults;
    QTreeWidget *list;
    QKeySequenceEdit *recorder;
    QLabel *message;
    QPushButton *save, *exportButton;
    void selectCommand();
    void refresh();
};
