#include "shortcutdialog.h"
#include "windowstate.h"
#include <QtWidgets>

ShortcutDialog::ShortcutDialog(const QList<QAction *> &commands, QSettings &preferences, QWidget *parent)
    : QDialog(parent), actions(commands), settings(preferences), draft(Shortcuts::current(commands)),
      defaults(Shortcuts::defaults(commands)) {
    setObjectName("shortcutDialog");
    setWindowTitle(tr("Keyboard shortcuts"));
    resize(720, 560);
    setMinimumSize(520, 360);
    auto layout = new QVBoxLayout(this);
    auto filter = new QLineEdit;
    filter->setObjectName("shortcutFilter");
    filter->setPlaceholderText(tr("Filter by command, ID, or shortcut…"));
    layout->addWidget(filter);
    list = new QTreeWidget;
    list->setObjectName("shortcutCommands");
    list->setHeaderLabels({tr("Command"), tr("Shortcut"), tr("Default")});
    list->setRootIsDecorated(false);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(list, 1);
    for (auto action : actions) {
        auto item = new QTreeWidgetItem(
            list, {action->text().remove('&'),
                   {},
                   defaults.value(action->objectName()).toString(QKeySequence::NativeText)});
        item->setData(0, Qt::UserRole, action->objectName());
        item->setToolTip(0, action->objectName());
    }
    auto edit = new QHBoxLayout;
    recorder = new QKeySequenceEdit;
    recorder->setObjectName("shortcutRecorder");
    recorder->setClearButtonEnabled(true);
    recorder->setAccessibleName(tr("Shortcut for selected command"));
    edit->addWidget(new QLabel(tr("New shortcut:")));
    edit->addWidget(recorder, 1);
    auto clear = new QPushButton(tr("Clear"));
    clear->setObjectName("clearShortcut");
    edit->addWidget(clear);
    auto reset = new QPushButton(tr("Reset selected"));
    reset->setObjectName("resetShortcut");
    edit->addWidget(reset);
    layout->addLayout(edit);
    auto hint =
        new QLabel(tr("Use Ctrl, Alt, Meta, or function keys. Chords may contain up to four combinations. "
                      "Unassigned text navigation and context-menu keys keep their platform behavior."));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    message = new QLabel;
    message->setObjectName("shortcutMessage");
    message->setWordWrap(true);
    layout->addWidget(message);
    auto bottom = new QHBoxLayout;
    auto importButton = new QPushButton(tr("Import…"));
    importButton->setObjectName("importShortcuts");
    exportButton = new QPushButton(tr("Export draft…"));
    exportButton->setObjectName("exportShortcuts");
    auto resetAll = new QPushButton(tr("Reset all"));
    resetAll->setObjectName("resetAllShortcuts");
    bottom->addWidget(importButton);
    bottom->addWidget(exportButton);
    bottom->addWidget(resetAll);
    bottom->addStretch();
    layout->addLayout(bottom);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    save = buttons->button(QDialogButtonBox::Save);
    save->setObjectName("saveShortcuts");
    layout->addWidget(buttons);
    connect(list, &QTreeWidget::currentItemChanged, this, [this] { selectCommand(); });
    connect(recorder, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &sequence) {
        auto item = list->currentItem();
        if (!item)
            return;
        draft[item->data(0, Qt::UserRole).toString()] = sequence;
        refresh();
    });
    connect(clear, &QPushButton::clicked, recorder, &QKeySequenceEdit::clear);
    connect(reset, &QPushButton::clicked, this, [this] {
        auto item = list->currentItem();
        if (!item)
            return;
        const auto id = item->data(0, Qt::UserRole).toString();
        draft[id] = defaults.value(id);
        selectCommand();
        refresh();
    });
    connect(resetAll, &QPushButton::clicked, this, [this] {
        draft = defaults;
        selectCommand();
        refresh();
    });
    connect(filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < list->topLevelItemCount(); ++i) {
            auto item = list->topLevelItem(i);
            item->setHidden(
                !(item->text(0) + " " + item->text(1) + " " + item->data(0, Qt::UserRole).toString())
                     .contains(text, Qt::CaseInsensitive));
        }
        auto selected = list->currentItem();
        if (selected && selected->isHidden())
            list->setCurrentItem(nullptr);
        if (!list->currentItem())
            for (int i = 0; i < list->topLevelItemCount(); ++i)
                if (!list->topLevelItem(i)->isHidden()) {
                    list->setCurrentItem(list->topLevelItem(i));
                    break;
                }
        selectCommand();
    });
    connect(importButton, &QPushButton::clicked, this, [this] {
        auto path = QFileDialog::getOpenFileName(this, tr("Import shortcuts"), {},
                                                 tr("Shortcut configuration (*.json)"));
        if (path.isEmpty())
            return;
        QString error;
        Shortcuts::Mapping imported;
        if (!Shortcuts::importFile(path, actions, &imported, &error)) {
            message->setText(error);
            return;
        }
        draft = imported;
        selectCommand();
        refresh();
        message->setText(
            tr("Imported into the draft. Save to apply, or Cancel to keep your current mappings."));
    });
    connect(exportButton, &QPushButton::clicked, this, [this] {
        auto path = QFileDialog::getSaveFileName(this, tr("Export draft shortcuts"), "strata-shortcuts.json",
                                                 tr("Shortcut configuration (*.json)"));
        if (path.isEmpty())
            return;
        QString error;
        if (!Shortcuts::exportFile(path, actions, draft, &error))
            message->setText(error);
        else
            message->setText(tr("Exported the draft. Active mappings change only when you Save."));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        QString error;
        if (!Shortcuts::apply(settings, actions, draft, &error)) {
            message->setText(error);
            return;
        }
        accept();
    });
    list->setCurrentItem(list->topLevelItem(0));
    selectCommand();
    refresh();
    WindowState::rememberDialog(*this, settings, "shortcuts");
}
void ShortcutDialog::selectCommand() {
    const QSignalBlocker blocker(recorder);
    auto item = list->currentItem();
    recorder->setEnabled(item != nullptr);
    recorder->setKeySequence(item ? draft.value(item->data(0, Qt::UserRole).toString()) : QKeySequence{});
}
void ShortcutDialog::refresh() {
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        auto item = list->topLevelItem(i);
        item->setText(1,
                      draft.value(item->data(0, Qt::UserRole).toString()).toString(QKeySequence::NativeText));
    }
    QString error;
    bool valid = Shortcuts::validate(actions, draft, &error);
    save->setEnabled(valid);
    exportButton->setEnabled(valid);
    message->setText(
        valid ? tr("Changes apply to menus, toolbar actions, and the command palette when you Save.")
              : error);
}
