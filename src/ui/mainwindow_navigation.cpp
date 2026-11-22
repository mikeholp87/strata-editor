#include "core/persistence.h"
#include "documentview.h"
#include "editor/codeeditor.h"
#include "editor/highlighter.h"
#include "gitpanel.h"
#include "mainwindow.h"
#include "projectsearch.h"
#include "shortcutdialog.h"
#include "windowstate.h"
#include <QJsonArray>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QtConcurrent>
#include <QtWidgets>

void MainWindow::setProject(const QString &p) {
    if (!QFileInfo(p).isDir())
        return;
    project = QFileInfo(p).canonicalFilePath();
    files->setRootPath(project);
    tree->setRootIndex(files->index(project));
    explorerDock->setWindowTitle(QFileInfo(project).fileName());
    projectSearch->setRoot(project);
    git->root = project;
    auto projects = settings.value("recentProjects").toStringList();
    projects.removeAll(project);
    projects.prepend(project);
    while (projects.size() > 12)
        projects.removeLast();
    settings.setValue("recentProjects", projects);
}
void MainWindow::palette(bool quick) {
    QDialog d(this);
    d.setWindowTitle(quick ? tr("Quick open • loaded project tree") : tr("Commands"));
    d.resize(600, 420);
    auto l = new QVBoxLayout(&d);
    auto input = new QLineEdit;
    input->setPlaceholderText(tr("Type to filter…"));
    auto list = new QListWidget;
    l->addWidget(input);
    l->addWidget(list);
    QStringList paths;
    if (quick) {
        std::function<void(QModelIndex)> collect = [&](QModelIndex parent) {
            for (int row = 0; row < files->rowCount(parent) && paths.size() < 5000; ++row) {
                auto i = files->index(row, 0, parent);
                if (files->isDir(i)) {
                    if (files->fileName(i) != ".git")
                        collect(i);
                } else
                    paths.append(files->filePath(i));
            }
        };
        collect(tree->rootIndex());
        for (auto path : paths) {
            auto item = new QListWidgetItem(QDir(project).relativeFilePath(path), list);
            item->setData(Qt::UserRole, path);
        }
    } else
        for (int i = 0; i < commands.size(); ++i) {
            auto item =
                new QListWidgetItem(commands[i]->text() + "    " + commands[i]->shortcut().toString(), list);
            item->setData(Qt::UserRole, i);
        }
    connect(input, &QLineEdit::textChanged, &d, [list](QString query) {
        for (int i = 0; i < list->count(); ++i) {
            auto item = list->item(i);
            int pos = 0;
            for (auto ch : query) {
                pos = item->text().indexOf(ch, pos, Qt::CaseInsensitive);
                if (pos < 0)
                    break;
                ++pos;
            }
            item->setHidden(pos < 0);
        }
        for (int i = 0; i < list->count(); ++i)
            if (!list->item(i)->isHidden()) {
                list->setCurrentRow(i);
                break;
            }
    });
    auto invoke = [&] {
        auto item = list->currentItem();
        if (!item || item->isHidden())
            return;
        d.accept();
        if (quick)
            openFile(item->data(Qt::UserRole).toString());
        else
            commands[item->data(Qt::UserRole).toInt()]->trigger();
    };
    connect(list, &QListWidget::itemActivated, &d, [&](QListWidgetItem *) { invoke(); });
    connect(input, &QLineEdit::returnPressed, &d, invoke);
    if (list->count())
        list->setCurrentRow(0);
    input->setFocus();
    WindowState::rememberDialog(d, settings, quick ? "quickOpen" : "commandPalette");
    d.exec();
}
void MainWindow::preferences() {
    QDialog d(this);
    d.setWindowTitle(tr("Preferences"));
    auto l = new QFormLayout(&d);
    auto font = new QFontComboBox;
    font->setCurrentFont(QFont(settings.value("font", "monospace").toString()));
    auto size = new QSpinBox;
    size->setObjectName("editorFontSize");
    size->setRange(7, 48);
    size->setValue(settings.value("fontSize", 12).toInt());
    auto sidebarSize = new QSpinBox;
    sidebarSize->setObjectName("sidebarFontSize");
    sidebarSize->setRange(7, 48);
    sidebarSize->setValue(tree->font().pointSize());
    auto width = new QSpinBox;
    width->setRange(1, 16);
    width->setValue(settings.value("tabWidth", 4).toInt());
    auto spaces = new QCheckBox;
    spaces->setChecked(settings.value("spaces", true).toBool());
    auto numbers = new QCheckBox;
    numbers->setChecked(settings.value("lineNumbers", true).toBool());
    auto restore = new QCheckBox;
    restore->setChecked(settings.value("restore", true).toBool());
    auto recovery = new QCheckBox;
    recovery->setChecked(settings.value("recovery", true).toBool());
    l->addRow(tr("Editor font"), font);
    l->addRow(tr("Editor font size"), size);
    l->addRow(tr("Project sidebar font size"), sidebarSize);
    l->addRow(tr("Tab width"), width);
    l->addRow(tr("Insert spaces"), spaces);
    l->addRow(tr("Line numbers"), numbers);
    l->addRow(tr("Restore session"), restore);
    l->addRow(tr("Recovery snapshots every 15 s"), recovery);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    l->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    WindowState::rememberDialog(d, settings, "preferences");
    if (d.exec() == QDialog::Accepted) {
        settings.setValue("font", font->currentFont().family());
        settings.setValue("fontSize", size->value());
        settings.setValue("sidebarFontSize", sidebarSize->value());
        settings.setValue("tabWidth", width->value());
        settings.setValue("spaces", spaces->isChecked());
        settings.setValue("lineNumbers", numbers->isChecked());
        settings.setValue("restore", restore->isChecked());
        settings.setValue("recovery", recovery->isChecked());
        settings.sync();
        configureViews();
    }
}

void MainWindow::editShortcuts() {
    ShortcutDialog dialog(commands, settings, this);
    dialog.exec();
}
