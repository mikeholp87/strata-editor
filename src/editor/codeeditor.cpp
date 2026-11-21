#include "codeeditor.h"
#include <QKeyEvent>
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
CodeEditor::CodeEditor(QWidget *p) : QPlainTextEdit(p), gutter(new Gutter(this)) {
    setObjectName("codeEditor");
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
    gutter->update();
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
                         ? QColor("#4DA8FF")
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
    auto c = textCursor();
    int first = document()->findBlock(c.selectionStart()).blockNumber(),
        last = document()->findBlock(qMax(c.selectionStart(), c.selectionEnd() - 1)).blockNumber();
    QString language = property("language").toString();
    QString prefix =
        (language == "Python" || language == "Bash" || language == "YAML" || language == "TOML") ? "#" : "//";
    bool remove = true;
    for (int n = first; n <= last; ++n)
        if (!document()->findBlockByNumber(n).text().trimmed().startsWith(prefix))
            remove = false;
    c.beginEditBlock();
    for (int n = last; n >= first; --n) {
        auto b = document()->findBlockByNumber(n);
        QTextCursor t(b);
        int pos = b.text().indexOf(QRegularExpression("\\S"));
        t.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, qMax(0, pos));
        if (remove) {
            t.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                           prefix.size() + (b.text().mid(pos + prefix.size(), 1) == " " ? 1 : 0));
            t.removeSelectedText();
        } else
            t.insertText(prefix + " ");
    }
    c.endEditBlock();
}
void CodeEditor::duplicateLine() {
    auto c = textCursor();
    c.beginEditBlock();
    c.select(QTextCursor::LineUnderCursor);
    auto s = c.selectedText();
    c.movePosition(QTextCursor::EndOfLine);
    c.insertText("\n" + s);
    c.endEditBlock();
    setTextCursor(c);
}
void CodeEditor::keyPressEvent(QKeyEvent *e) {
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
        if (closes.contains(value) && document()->characterAt(textCursor().position()) == value[0]) {
            auto c = textCursor();
            c.movePosition(QTextCursor::Right);
            setTextCursor(c);
            return;
        }
    }
    QPlainTextEdit::keyPressEvent(e);
}
