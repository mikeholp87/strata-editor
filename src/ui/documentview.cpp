#include "documentview.h"
#include "editor/codeeditor.h"
#include <QAction>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
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
    layout->addWidget(panes);
    if (d->mode() == Document::Mode::Html) {
        rich = new QTextEdit;
        rich->setObjectName("richEditor");
        rich->installEventFilter(this);
        rich->setDocument(d->text());
        panes->addWidget(rich);
        connect(rich, &QTextEdit::cursorPositionChanged, this, &DocumentView::cursorChanged);
    } else {
        code = new CodeEditor;
        code->installEventFilter(this);
        code->setDocument(d->text());
        panes->addWidget(code);
        connect(code, &QPlainTextEdit::cursorPositionChanged, this, &DocumentView::cursorChanged);
    }
    if (d->mode() == Document::Mode::Markdown) {
        preview = new QTextBrowser;
        preview->setDocument(new SafeDocument(preview));
        preview->setOpenLinks(false);
        preview->setOpenExternalLinks(false);
        panes->addWidget(preview);
        connect(preview, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
            if (url.toString().startsWith('#'))
                preview->scrollToAnchor(url.fragment());
        });
        auto timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(200);
        connect(d->text(), &QTextDocument::contentsChanged, timer, qOverload<>(&QTimer::start));
        connect(timer, &QTimer::timeout, this, &DocumentView::updatePreview);
        updatePreview();
    }
}
void DocumentView::updatePreview() {
    if (!preview)
        return;
    int pos = preview->verticalScrollBar()->value();
    preview->document()->setMarkdown(model->sourceText(),
                                     QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) |
                                         QTextDocument::MarkdownNoHTML);
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
QTextCursor DocumentView::cursor() const {
    if (code)
        return activeCode()->textCursor();
    return activeRich()->textCursor();
}
void DocumentView::setCursor(const QTextCursor &c) {
    if (code)
        activeCode()->setTextCursor(c);
    else
        activeRich()->setTextCursor(c);
}
void DocumentView::togglePreview() {
    if (preview)
        preview->setVisible(!preview->isVisible());
}
void DocumentView::split(Qt::Orientation orientation) {
    panes->setOrientation(orientation);
    if (code) {
        auto e = new CodeEditor;
        e->installEventFilter(this);
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
        e->installEventFilter(this);
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
            {"preview", preview && !preview->isHidden()}};
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
    if (preview)
        preview->setVisible(state["preview"].toBool(true));
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
