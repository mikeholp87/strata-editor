#include "codeeditor.h"
#include "completion.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStringListModel>
#include <QPainter>
#include <QRegularExpression>
#include <QTextBlock>
class Gutter : public QWidget {
    CodeEditor *editor;

  public:
    explicit Gutter(CodeEditor *e) : QWidget(e), editor(e) {}
    QSize sizeHint() const override {
        return {editor->gutterWidth(), 0};
    }
    void paintEvent(QPaintEvent *e) override {
        editor->paintGutter(e);
    }
};
CodeEditor::CodeEditor(QWidget *p)
    : QPlainTextEdit(p), gutter(new Gutter(this)), completer(new QCompleter(new QStringListModel, this)) {
    completer->model()->setParent(completer);
    completer->setWidget(this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setMaxVisibleItems(10);
    connect(completer, QOverload<const QString &>::of(&QCompleter::activated), this,
            &CodeEditor::insertCompletion);
    setObjectName("codeEditor");
    setAccessibleName(tr("Source editor"));
    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(NoWrap);
    connect(this, &QPlainTextEdit::blockCountChanged, this, [this] { refreshOptions(); });
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &r, int dy) {
        if (dy)
            gutter->scroll(0, dy);
        else
            gutter->update(0, r.y(), gutter->width(), r.height());
    });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightLine);
    refreshOptions();
    highlightLine();
}
int CodeEditor::gutterWidth() const {
    return numbers ? 18 + fontMetrics().horizontalAdvance(QLatin1Char('9')) *
                              QString::number(qMax(1, blockCount())).size()
                   : 0;
}
void CodeEditor::refreshOptions() {
    setViewportMargins(gutterWidth(), 0, 0, 0);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * tabWidth);
    gutter->setVisible(numbers);
    gutter->setGeometry(0, 0, gutterWidth(), height());
    highlightLine();
}
void CodeEditor::resizeEvent(QResizeEvent *e) {
    QPlainTextEdit::resizeEvent(e);
    gutter->setGeometry(0, 0, gutterWidth(), height());
}
void CodeEditor::paintGutter(QPaintEvent *e) {
    QPainter p(gutter);
    p.fillRect(e->rect(), palette().color(QPalette::Window));
    auto b = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(b).translated(contentOffset()).top());
    while (b.isValid() && top <= e->rect().bottom()) {
        int h = qRound(blockBoundingRect(b).height());
        if (b.isVisible() && top + h >= e->rect().top()) {
            p.setPen(b.blockNumber() == textCursor().blockNumber()
                         ? palette().color(QPalette::Link)
                         : palette().color(QPalette::PlaceholderText));
            p.drawText(0, top, gutter->width() - 8, fontMetrics().height(), Qt::AlignRight,
                       QString::number(b.blockNumber() + 1));
        }
        top += h;
        b = b.next();
    }
}
void CodeEditor::highlightLine() {
    QTextEdit::ExtraSelection s;
    s.format.setBackground(palette().color(QPalette::AlternateBase));
    s.format.setProperty(QTextFormat::FullWidthSelection, true);
    s.cursor = textCursor();
    s.cursor.clearSelection();
    setExtraSelections({s});
    gutter->update();
}
void CodeEditor::indent(bool reverse) {
    if (isReadOnly())
        return;
    auto c = textCursor();
    int first = document()->findBlock(c.selectionStart()).blockNumber();
    int last = document()->findBlock(qMax(c.selectionStart(), c.selectionEnd() - 1)).blockNumber();
    c.beginEditBlock();
    for (int n = last; n >= first; --n) {
        auto b = document()->findBlockByNumber(n);
        QTextCursor line(b);
        if (!reverse)
            line.insertText(spaces ? QString(tabWidth, ' ') : "\t");
        else {
            int count = 0;
            auto s = b.text();
            if (s.startsWith('\t'))
                count = 1;
            else
                while (count < tabWidth && count < s.size() && s[count] == ' ')
                    ++count;
            line.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, count);
            line.removeSelectedText();
        }
    }
    c.endEditBlock();
}
void CodeEditor::toggleComment() {
    if (isReadOnly())
        return;
    auto c = textCursor();
    int first = document()->findBlock(c.selectionStart()).blockNumber(),
        last = document()->findBlock(qMax(c.selectionStart(), c.selectionEnd() - 1)).blockNumber();
    const QString language = property("language").toString();
    // Line marker, plus a closing marker for languages that only have block comments.
    QString open = "// ", close;
    if (language == "Python" || language == "Bash" || language == "YAML" || language == "TOML")
        open = "# ";
    else if (language == "SQL")
        open = "-- ";
    else if (language == "CSS")
        open = "/* ", close = " */";
    else if (language == "HTML" || language == "Markdown")
        open = "<!-- ", close = " -->";
    const QString head = open.trimmed(), tail = close.trimmed();
    auto blank = [](const QTextBlock &b) { return b.text().trimmed().isEmpty(); };
    bool remove = false;
    for (int n = first; n <= last; ++n) {
        const auto b = document()->findBlockByNumber(n);
        if (blank(b))
            continue;
        const auto text = b.text().trimmed();
        if (!text.startsWith(head) || !text.endsWith(tail)) {
            remove = false;
            break;
        }
        remove = true;
    }
    c.beginEditBlock();
    for (int n = last; n >= first; --n) {
        auto b = document()->findBlockByNumber(n);
        if (blank(b))
            continue; // never touch empty lines
        const auto text = b.text();
        const int indent = text.indexOf(QRegularExpression("\\S"));
        const int end = indent + text.trimmed().size(); // before trailing blanks
        QTextCursor t(b);
        if (remove) {
            if (!tail.isEmpty()) {
                const int tailStart = end - int(tail.size());
                const int from = tailStart > 0 && text[tailStart - 1] == ' ' ? tailStart - 1 : tailStart;
                t.setPosition(b.position() + from);
                t.setPosition(b.position() + end, QTextCursor::KeepAnchor);
                t.removeSelectedText();
            }
            t.setPosition(b.position() + indent);
            t.setPosition(b.position() + indent + head.size() + (text.mid(indent + head.size(), 1) == " " ? 1 : 0),
                          QTextCursor::KeepAnchor);
            t.removeSelectedText();
        } else {
            if (!close.isEmpty()) {
                t.setPosition(b.position() + end);
                t.insertText(close);
            }
            t.setPosition(b.position() + indent);
            t.insertText(open);
        }
    }
    c.endEditBlock();
}
void CodeEditor::duplicateLine() {
    if (isReadOnly())
        return;
    auto c = textCursor();
    c.beginEditBlock();
    c.select(QTextCursor::LineUnderCursor);
    auto s = c.selectedText();
    c.movePosition(QTextCursor::EndOfLine);
    c.insertText("\n" + s);
    c.endEditBlock();
    setTextCursor(c);
}
namespace {
bool wordChar(QChar c) {
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}
} // namespace
void CodeEditor::insertCompletion(const QString &text) {
    auto c = textCursor();
    c.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, completer->completionPrefix().size());
    c.insertText(text);
    setTextCursor(c);
}
void CodeEditor::showCompletion(bool force) {
    auto popup = completer->popup();
    auto c = textCursor();
    if (!completion || isReadOnly() || c.hasSelection()) {
        popup->hide();
        return;
    }
    const auto line = c.block().text();
    const int pos = c.positionInBlock();
    int start = pos, end = pos;
    while (start > 0 && wordChar(line[start - 1]))
        --start;
    while (end < line.size() && wordChar(line[end]))
        ++end;
    const auto prefix = line.mid(start, pos - start);
    if (prefix.size() < (force ? 1 : 3)) {
        popup->hide();
        return;
    }
    if (force || !popup->isVisible()) {
        // Copy only the scanned window; copying the whole document on every new word stalls typing.
        const int wordStart = c.block().position() + start, wordEnd = c.block().position() + end;
        const int lo = qMax(0, wordStart - Completion::window);
        const int hi = qMin(document()->characterCount() - 1, wordEnd + Completion::window);
        QTextCursor range(document());
        range.setPosition(lo);
        range.setPosition(hi, QTextCursor::KeepAnchor);
        static_cast<QStringListModel *>(completer->model())
            ->setStringList(Completion::candidates(range.selectedText(), wordStart - lo, wordEnd - lo,
                                                   property("language").toString()));
    }
    completer->setCompletionPrefix(prefix);
    auto model = completer->completionModel();
    if (model->rowCount() == 0 ||
        (model->rowCount() == 1 && model->index(0, 0).data().toString() == prefix)) {
        popup->hide();
        return;
    }
    popup->setCurrentIndex(model->index(0, 0));
    auto at = c;
    at.setPosition(c.block().position() + start);
    auto rect = cursorRect(at);
    rect.setWidth(popup->sizeHintForColumn(0) + popup->verticalScrollBar()->sizeHint().width() + 16);
    completer->complete(rect);
}
void CodeEditor::keyPressEvent(QKeyEvent *e) {
    if (completer->popup()->isVisible())
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore(); // the completion popup accepts or dismisses
            return;
        default:
            break;
        }
    handleKey(e);
    if (isReadOnly())
        return;
    const auto t = e->text();
    const bool typed = !t.isEmpty() && wordChar(t[0]) && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    if (typed || (e->key() == Qt::Key_Backspace && completer->popup()->isVisible()))
        showCompletion();
    else
        completer->popup()->hide();
}
void CodeEditor::handleKey(QKeyEvent *e) {
    if (isReadOnly()) {
        QPlainTextEdit::keyPressEvent(e);
        return;
    }
    if (e->key() == Qt::Key_Tab && !(e->modifiers() & Qt::ControlModifier)) {
        if (textCursor().hasSelection())
            indent();
        else
            insertPlainText(spaces ? QString(tabWidth, ' ') : "\t");
        return;
    }
    if (e->key() == Qt::Key_Backtab) {
        indent(true);
        return;
    }
    if (isMarkdown() && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && continueMarkdownLine())
        return;
    if (autoIndent && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) {
        auto c = textCursor();
        auto s = c.block().text();
        auto ws = QRegularExpression("^\\s*").match(s).captured();
        if (s.left(c.positionInBlock()).trimmed().endsWith('{'))
            ws += spaces ? QString(tabWidth, ' ') : "\t";
        c.beginEditBlock();
        c.insertText("\n" + ws);
        c.endEditBlock();
        setTextCursor(c);
        return;
    }
    if (pairs && e->text().size() == 1 && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
        QString opens = "([{", closes = ")]}", value = e->text();
        int i = opens.indexOf(value);
        if (i >= 0) {
            auto c = textCursor();
            auto selected = c.selectedText();
            c.beginEditBlock();
            c.insertText(value + selected + closes[i]);
            c.movePosition(QTextCursor::Left);
            c.endEditBlock();
            setTextCursor(c);
            return;
        }
        if (!textCursor().hasSelection() && closes.contains(value) &&
            document()->characterAt(textCursor().position()) == value[0]) {
            auto c = textCursor();
            c.movePosition(QTextCursor::Right);
            setTextCursor(c);
            return;
        }
    }
    QPlainTextEdit::keyPressEvent(e);
}
