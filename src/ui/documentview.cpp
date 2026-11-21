#include "documentview.h"
#include "editor/codeeditor.h"
#include <QApplication>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBlock>
#include <QToolButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollBar>
#include <QSplitter>
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
        rich->setDocument(d->text());
        panes->addWidget(rich);
        connect(rich, &QTextEdit::cursorPositionChanged, this, &DocumentView::cursorChanged);
    } else {
        code = new CodeEditor;
        code->setDocument(d->text());
        panes->addWidget(code);
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
    if (!preview)
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
QTextCursor DocumentView::cursor() const {
    if (code)
        return activeCode()->textCursor();
    return focusedRich ? focusedRich->textCursor() : rich->textCursor();
}
void DocumentView::setCursor(const QTextCursor &c) {
    if (code) {
        if (preview && mode == MarkdownView::Preview)
            setMarkdownView(MarkdownView::Split);
        activeCode()->setTextCursor(c);
    } else if (focusedRich)
        focusedRich->setTextCursor(c);
    else
        rich->setTextCursor(c);
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
    mode = value;
    for (int i = 0; i < panes->count(); ++i) {
        auto pane = panes->widget(i);
        pane->setVisible(pane == previewPane ? mode != MarkdownView::Source
                                            : mode != MarkdownView::Preview);
    }
    viewButtons->button(int(mode))->setChecked(true);
    if (mode == MarkdownView::Preview) {
        updatePreview();
        preview->setFocus();
    } else
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
        e->setDocument(model->text());
        e->setFont(code->font());
        e->tabWidth = code->tabWidth;
        e->spaces = code->spaces;
        e->setLineWrapMode(code->lineWrapMode());
        e->refreshOptions();
        e->setProperty("language", code->property("language"));
        panes->addWidget(e);
        connect(e, &QPlainTextEdit::cursorPositionChanged, this, &DocumentView::cursorChanged);
    } else {
        auto e = new QTextEdit;
        e->setDocument(model->text());
        panes->addWidget(e);
        connect(e, &QTextEdit::cursorPositionChanged, this, &DocumentView::cursorChanged);
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
        auto widget = panes->widget(i);
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
        auto widget = panes->widget(i);
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
