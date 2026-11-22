#include "documentview.h"
#include "editor/codeeditor.h"
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBlock>
#include <QToolButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QScrollBar>
#include <QSplitter>
#include <QStyle>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
DocumentView::DocumentView(Document *d, QWidget *p) : QWidget(p), model(d), panes(new QSplitter(this)) {
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (auto e = qobject_cast<CodeEditor *>(now); e && isAncestorOf(e))
            focusedCode = e;
        if (auto e = qobject_cast<QTextEdit *>(now); e && e != preview && isAncestorOf(e))
            focusedRich = e;
    });
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    externalWarning = new QWidget;
    externalWarning->setObjectName("externalWarning");
    auto warningLayout = new QHBoxLayout(externalWarning);
    auto warning = new QLabel(tr("File changed on disk. Reload it or save your edits as a copy."));
    warning->setWordWrap(true);
    warningLayout->addWidget(warning, 1);
    auto reload = new QPushButton(tr("Reload"));
    reload->setObjectName("reloadExternal");
    auto saveCopy = new QPushButton(tr("Save a copy…"));
    saveCopy->setObjectName("saveExternalCopy");
    warningLayout->addWidget(reload);
    warningLayout->addWidget(saveCopy);
    connect(reload, &QPushButton::clicked, this, &DocumentView::reloadRequested);
    connect(saveCopy, &QPushButton::clicked, this, &DocumentView::saveCopyRequested);
    layout->addWidget(externalWarning);
    externalWarning->hide();
    layout->addWidget(panes, 1);
    if (d->mode() == Document::Mode::Html) {
        rich = new QTextEdit;
        rich->setObjectName("richEditor");
        rich->installEventFilter(this);
        rich->setDocument(d->text());
        addEditorPane(rich);
        connect(rich, &QTextEdit::cursorPositionChanged, this, &DocumentView::cursorChanged);
    } else {
        code = new CodeEditor;
        code->installEventFilter(this);
        code->setDocument(d->text());
        addEditorPane(code);
        connect(code, &QPlainTextEdit::cursorPositionChanged, this, &DocumentView::cursorChanged);
    }
    if (d->mode() == Document::Mode::Markdown) {
        auto controls = new QWidget;
        controls->setObjectName("markdownControls");
        auto controlLayout = new QHBoxLayout(controls);
        controlLayout->setContentsMargins(8, 4, 8, 4);
        controlLayout->setSpacing(0);
        viewButtons = new QButtonGroup(this);
        for (auto view : {MarkdownView::Source, MarkdownView::Split, MarkdownView::Preview}) {
            auto button = new QToolButton;
            button->setText(view == MarkdownView::Source ? tr("Source")
                            : view == MarkdownView::Split ? tr("Split") : tr("Preview"));
            button->setObjectName(view == MarkdownView::Source ? "sourceView"
                                  : view == MarkdownView::Split ? "splitView" : "previewView");
            button->setAccessibleName(tr("Markdown view: %1").arg(button->text()));
            button->setCheckable(true);
            button->setFocusPolicy(Qt::StrongFocus);
            viewButtons->addButton(button, int(view));
            controlLayout->addWidget(button);
            connect(button, &QToolButton::clicked, this, [this, view] { setMarkdownView(view); });
        }
        controlLayout->addStretch();
        layout->insertWidget(1, controls);
        preview = new QTextBrowser;
        preview->setObjectName("markdownPreview");
        preview->setAccessibleName(tr("Markdown preview, read only"));
        preview->setFrameShape(QFrame::NoFrame);
        QFont prose = QApplication::font();
        prose.setPointSize(12);
        preview->setFont(prose);
        preview->setMinimumWidth(0);
        preview->setMaximumWidth(QFontMetrics(prose).horizontalAdvance(QLatin1Char('n')) * 70 + 48);
        previewPane = new QWidget;
        previewPane->setObjectName("previewPane");
        previewPane->setAutoFillBackground(true);
        previewPane->setBackgroundRole(QPalette::Base);
        auto previewLayout = new QHBoxLayout(previewPane);
        previewLayout->setContentsMargins(0, 0, 0, 0);
        previewLayout->addStretch();
        previewLayout->addWidget(preview, 1);
        previewLayout->addStretch();
        preview->setDocument(new SafeDocument(preview));
        preview->setOpenLinks(false);
        preview->setOpenExternalLinks(false);
        preview->document()->setDefaultFont(prose);
        preview->document()->setDocumentMargin(24);
        panes->addWidget(previewPane);
        panes->setChildrenCollapsible(false);
        setMarkdownView(MarkdownView::Split);
        connect(preview, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
            if (url.toString().startsWith('#'))
                preview->scrollToAnchor(url.fragment());
        });
        previewTimer = new QTimer(this);
        previewTimer->setSingleShot(true);
        previewTimer->setInterval(200);
        connect(d->text(), &QTextDocument::contentsChanged, previewTimer, qOverload<>(&QTimer::start));
        connect(previewTimer, &QTimer::timeout, this, &DocumentView::updatePreview);
        updatePreview();
    }
}
void DocumentView::updatePreview() {
    if (!preview || mode == MarkdownView::Source)
        return;
    if (previewTimer)
        previewTimer->stop();
    int pos = preview->verticalScrollBar()->value();
    preview->document()->setMarkdown(model->sourceText(),
                                     QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) |
                                         QTextDocument::MarkdownNoHTML);
    for (auto block = preview->document()->begin(); block.isValid(); block = block.next()) {
        QTextCursor c(block);
        auto format = block.blockFormat();
        format.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
        c.setBlockFormat(format);
    }
    QTextCursor cursor(preview->document());
    cursor.movePosition(QTextCursor::Start);
    preview->setTextCursor(cursor);
    QTimer::singleShot(0, preview, [this, pos] { preview->verticalScrollBar()->setValue(pos); });
}
CodeEditor *DocumentView::activeCode() const {
    return focusedCode ? focusedCode.data() : code;
}
QTextEdit *DocumentView::activeRich() const {
    return focusedRich ? focusedRich.data() : rich;
}
bool DocumentView::isReadOnly() const {
    return code ? activeCode()->isReadOnly() : activeRich()->isReadOnly();
}
QTextCursor DocumentView::cursor() const {
    if (code)
        return activeCode()->textCursor();
    return activeRich()->textCursor();
}
void DocumentView::setCursor(const QTextCursor &c) {
    if (code) {
        if (preview && mode == MarkdownView::Preview)
            setMarkdownView(MarkdownView::Split);
        activeCode()->setTextCursor(c);
    }
    else
        activeRich()->setTextCursor(c);
}
void DocumentView::setExternalChange(bool changed) {
    externalWarning->setVisible(changed);
}
bool DocumentView::externallyChanged() const {
    return !externalWarning->isHidden();
}
void DocumentView::setMarkdownView(MarkdownView value) {
    if (!preview)
        return;
    bool focusInDocument = isAncestorOf(QApplication::focusWidget());
    mode = value;
    for (int i = 0; i < panes->count(); ++i) {
        auto pane = panes->widget(i);
        pane->setVisible(pane == previewPane ? mode != MarkdownView::Source
                                            : mode != MarkdownView::Preview);
    }
    viewButtons->button(int(mode))->setChecked(true);
    if (mode != MarkdownView::Source)
        updatePreview();
    if (mode == MarkdownView::Preview) {
        if (focusInDocument)
            preview->setFocus();
    } else if (focusInDocument)
        activeCode()->setFocus();
}
void DocumentView::togglePreview() {
    setMarkdownView(mode == MarkdownView::Source ? MarkdownView::Split : MarkdownView::Source);
}
void DocumentView::split(Qt::Orientation orientation) {
    if (preview)
        setMarkdownView(MarkdownView::Split);
    panes->setOrientation(orientation);
    if (code) {
        auto e = new CodeEditor;
        e->installEventFilter(this);
        e->setDocument(model->text());
        e->setFont(code->font());
        e->tabWidth = code->tabWidth;
        e->spaces = code->spaces;
        e->numbers = code->numbers;
        e->setLineWrapMode(code->lineWrapMode());
        e->refreshOptions();
        e->setProperty("language", code->property("language"));
        e->setReadOnly(code->isReadOnly());
        addEditorPane(e);
        connect(e, &QPlainTextEdit::cursorPositionChanged, this, &DocumentView::cursorChanged);
    } else {
        auto e = new QTextEdit;
        e->installEventFilter(this);
        e->setDocument(model->text());
        e->setReadOnly(rich->isReadOnly());
        addEditorPane(e);
        connect(e, &QTextEdit::cursorPositionChanged, this, &DocumentView::cursorChanged);
    }
}
void DocumentView::addEditorPane(QWidget *editor) {
    auto pane = new QWidget;
    auto layout = new QVBoxLayout(pane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto header = new QWidget;
    header->setObjectName("splitHeader");
    auto row = new QHBoxLayout(header);
    row->setContentsMargins(8, 2, 4, 2);
    row->addWidget(new QLabel(tr("Editor")));
    row->addStretch();
    auto close = new QToolButton;
    close->setObjectName("closeSplitView");
    close->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    close->setToolTip(tr("Close this split view; keep the document open"));
    close->setAccessibleName(tr("Close split view"));
    close->setAutoRaise(true);
    row->addWidget(close);
    layout->addWidget(header);
    layout->addWidget(editor, 1);
    panes->addWidget(pane);
    connect(close, &QToolButton::clicked, this, [this, pane] { closePane(pane); });
    updatePaneControls();
}
QWidget *DocumentView::editorForPane(QWidget *pane) const {
    if (pane == previewPane)
        return nullptr;
    if (auto editor = pane->findChild<CodeEditor *>())
        return editor;
    return pane->findChild<QTextEdit *>();
}
void DocumentView::updatePaneControls() {
    const int count = panes->count() - (previewPane ? 1 : 0);
    for (int i = 0; i < panes->count(); ++i)
        if (auto header = panes->widget(i)->findChild<QWidget *>("splitHeader"))
            header->setVisible(count > 1);
}
void DocumentView::closePane(QWidget *pane) {
    if (pane == previewPane || panes->count() - (previewPane ? 1 : 0) <= 1)
        return;
    const bool primary = editorForPane(pane) == (code ? static_cast<QWidget *>(code) : rich);
    delete pane;
    if (primary) {
        for (int i = 0; i < panes->count(); ++i)
            if (auto editor = editorForPane(panes->widget(i))) {
                code = qobject_cast<CodeEditor *>(editor);
                rich = qobject_cast<QTextEdit *>(editor);
                break;
            }
    }
    focusedCode = code;
    focusedRich = rich;
    (code ? static_cast<QWidget *>(code) : rich)->setFocus();
    updatePaneControls();
    emit cursorChanged();
}
void DocumentView::closeExtraSplits() {
    for (int i = panes->count() - 1; i >= 0; --i) {
        auto pane = panes->widget(i);
        auto editor = editorForPane(pane);
        if (editor && editor != (code ? static_cast<QWidget *>(code) : rich))
            closePane(pane);
    }
}
void DocumentView::configure(const QFont &font, bool wrap, int tabs, bool spaces, bool numbers) {
    for (auto e : findChildren<CodeEditor *>()) {
        e->setFont(font);
        e->tabWidth = tabs;
        e->spaces = spaces;
        e->numbers = numbers;
        e->setLineWrapMode(wrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
        e->refreshOptions();
    }
}

QJsonObject DocumentView::viewState() const {
    QJsonArray views;
    for (int i = 0; i < panes->count(); ++i) {
        auto widget = editorForPane(panes->widget(i));
        QTextCursor c;
        int scroll = 0;
        if (auto e = qobject_cast<CodeEditor *>(widget)) {
            c = e->textCursor();
            scroll = e->verticalScrollBar()->value();
        } else if (auto e = qobject_cast<QTextEdit *>(widget); e && e != preview) {
            c = e->textCursor();
            scroll = e->verticalScrollBar()->value();
        } else
            continue;
        views.append(QJsonObject{{"cursor", c.position()}, {"scroll", scroll}});
    }
    return {{"orientation", int(panes->orientation())},
            {"views", views},
            {"preview", preview && mode != MarkdownView::Source},
            {"markdownView", int(mode)}};
}
void DocumentView::restoreViewState(const QJsonObject &state) {
    auto views = state["views"].toArray();
    auto orientation = state["orientation"].toInt() == int(Qt::Vertical) ? Qt::Vertical : Qt::Horizontal;
    for (int i = 1; i < qMin(views.size(), qsizetype(4)); ++i)
        split(orientation);
    int index = 0;
    for (int i = 0; i < panes->count() && index < views.size(); ++i) {
        auto widget = editorForPane(panes->widget(i));
        auto o = views[index].toObject();
        QTextCursor c(model->text());
        c.setPosition(qBound(0, o["cursor"].toInt(), model->text()->characterCount() - 1));
        if (auto e = qobject_cast<CodeEditor *>(widget)) {
            e->setTextCursor(c);
            e->verticalScrollBar()->setValue(o["scroll"].toInt());
            ++index;
        } else if (auto e = qobject_cast<QTextEdit *>(widget); e && e != preview) {
            e->setTextCursor(c);
            e->verticalScrollBar()->setValue(o["scroll"].toInt());
            ++index;
        }
    }
    if (preview) {
        int value = state["markdownView"].toInt(state["preview"].toBool(true)
                                                  ? int(MarkdownView::Split) : int(MarkdownView::Source));
        setMarkdownView(value >= int(MarkdownView::Source) && value <= int(MarkdownView::Preview)
                            ? MarkdownView(value) : MarkdownView::Split);
    }
}

bool DocumentView::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::ShortcutOverride) {
        const auto key = static_cast<QKeyEvent *>(event)->keyCombination();
        if (!(key.keyboardModifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) &&
            !(key.key() >= Qt::Key_F1 && key.key() <= Qt::Key_F35))
            return QWidget::eventFilter(object, event);
        for (auto action : window()->actions()) {
            if (!action->isEnabled() || !action->isVisible() ||
                !action->property("defaultShortcut").isValid())
                continue;
            for (const auto &sequence : action->shortcuts())
                for (int i = 0; i < sequence.count(); ++i)
                    if (sequence[i] == key) {
                        event->ignore();
                        return true;
                    }
        }
    }
    return QWidget::eventFilter(object, event);
}
