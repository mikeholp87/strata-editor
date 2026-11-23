// Markdown source editing for CodeEditor: inline marks, headings, lists, links, fences
// and list continuation on Enter.
#include "codeeditor.h"
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>
#include <utility>

namespace {
// Count consecutive `ch` characters ending just before `pos` (dir < 0) or starting at it (dir > 0).
int run(QTextDocument *d, int pos, QChar ch, int dir) {
    int n = 0;
    for (;;) {
        const int i = dir > 0 ? pos + n : pos - 1 - n;
        if (i < 0 || i >= d->characterCount() || d->characterAt(i) != ch)
            return n;
        ++n;
    }
}
// Is a mark of `n` characters present when `count` of them surround the text? "***x***" is both
// bold and italic, so a lone `*` is present on an odd run and `**` on a run of two or more.
bool present(QChar ch, int n, int count) {
    return ch == QLatin1Char('*') && n == 1 ? count % 2 == 1 : count >= n;
}
struct Toggled {
    int start, end, delta;
};
// Toggle `mark` around [s, e) within one block.
Toggled toggleRange(QTextDocument *d, const QString &mark, int s, int e) {
    const QChar ch = mark[0];
    const int n = mark.size();
    QTextCursor w(d);
    if (present(ch, n, qMin(run(d, s, ch, -1), run(d, e, ch, +1)))) {
        w.setPosition(e);
        w.setPosition(e + n, QTextCursor::KeepAnchor);
        w.removeSelectedText();
        w.setPosition(s - n);
        w.setPosition(s, QTextCursor::KeepAnchor);
        w.removeSelectedText();
        return {s - n, e - n, -2 * n};
    }
    w.setPosition(s);
    w.setPosition(e, QTextCursor::KeepAnchor);
    const auto text = w.selectedText();
    int lead = 0, trail = 0;
    while (lead < text.size() && text[lead] == ch)
        ++lead;
    while (trail < text.size() && text[text.size() - 1 - trail] == ch)
        ++trail;
    if (lead < text.size() && present(ch, n, qMin(lead, trail))) {
        const auto inner = text.mid(n, text.size() - 2 * n);
        w.insertText(inner);
        return {s, s + int(inner.size()), -2 * n};
    }
    w.setPosition(e);
    w.insertText(mark);
    w.setPosition(s);
    w.insertText(mark);
    return {s + n, e + n, 2 * n};
}
const QRegularExpression &headingRe() {
    static const QRegularExpression re(QStringLiteral("^ {0,3}(#{1,6})\\s+"));
    return re;
}
const QRegularExpression &taskRe() {
    static const QRegularExpression re(QStringLiteral("^(\\s*)[-*+]\\s+\\[[ xX]\\]\\s+"));
    return re;
}
const QRegularExpression &bulletRe() {
    static const QRegularExpression re(QStringLiteral("^(\\s*)[-*+]\\s+"));
    return re;
}
const QRegularExpression &numberRe() {
    static const QRegularExpression re(QStringLiteral("^(\\s*)\\d+[.)]\\s+"));
    return re;
}
const QRegularExpression &quoteRe() {
    static const QRegularExpression re(QStringLiteral("^(\\s{0,3})>\\s?"));
    return re;
}
bool hasKind(CodeEditor::LineKind kind, const QString &text) {
    switch (kind) {
    case CodeEditor::LineKind::Task:
        return taskRe().match(text).hasMatch();
    case CodeEditor::LineKind::Bullet:
        return !taskRe().match(text).hasMatch() && bulletRe().match(text).hasMatch();
    case CodeEditor::LineKind::Number:
        return numberRe().match(text).hasMatch();
    case CodeEditor::LineKind::Quote:
        return quoteRe().match(text).hasMatch();
    }
    return false;
}
void replaceBlockText(const QTextBlock &b, const QString &text) {
    if (b.text() == text)
        return;
    QTextCursor t(b);
    t.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    t.insertText(text);
}
// Blocks touched by the selection; a selection ending at a line start excludes that line.
QList<QTextBlock> selectedBlocks(const QTextCursor &c) {
    QList<QTextBlock> blocks;
    const auto d = c.document();
    const int last = qMax(c.selectionStart(), c.selectionEnd() - (c.hasSelection() ? 1 : 0));
    for (auto b = d->findBlock(c.selectionStart()); b.isValid() && b.position() <= last; b = b.next())
        blocks << b;
    return blocks;
}
bool inFence(const QTextBlock &block) {
    bool open = false;
    for (auto b = block.previous(); b.isValid(); b = b.previous()) {
        const auto t = b.text().trimmed();
        if (t.startsWith(QStringLiteral("```")) || t.startsWith(QStringLiteral("~~~")))
            open = !open;
    }
    return open;
}
} // namespace

bool CodeEditor::isMarkdown() const {
    return property("language").toString() == QLatin1String("Markdown");
}

void CodeEditor::toggleInline(const QString &mark) {
    if (isReadOnly() || !isMarkdown() || mark.isEmpty())
        return;
    auto c = textCursor();
    auto d = document();
    const int n = mark.size();
    QList<std::pair<int, int>> ranges;
    int caret = -1;
    if (c.hasSelection()) {
        const int a = c.selectionStart(), b = c.selectionEnd();
        for (auto blk = d->findBlock(a); blk.isValid() && blk.position() < b; blk = blk.next()) {
            const auto text = blk.text();
            int s = qMax(a, blk.position()), e = qMin(b, blk.position() + int(text.size()));
            while (s < e && text[s - blk.position()].isSpace())
                ++s;
            while (e > s && text[e - 1 - blk.position()].isSpace())
                --e;
            if (s < e)
                ranges << std::make_pair(s, e);
        }
    } else {
        caret = c.position();
        const auto text = c.block().text();
        int s = c.positionInBlock(), e = s;
        while (s > 0 && (text[s - 1].isLetterOrNumber() || text[s - 1] == QLatin1Char('_')))
            --s;
        while (e < text.size() && (text[e].isLetterOrNumber() || text[e] == QLatin1Char('_')))
            ++e;
        const int base = c.block().position();
        if (s < e)
            ranges << std::make_pair(base + s, base + e);
    }
    c.beginEditBlock();
    if (ranges.isEmpty()) {
        const QChar ch = mark[0];
        const int pos = c.hasSelection() ? c.selectionStart() : c.position();
        c.clearSelection();
        c.setPosition(pos);
        if (caret >= 0 && present(ch, n, qMin(run(d, pos, ch, -1), run(d, pos, ch, +1)))) {
            c.setPosition(pos + n);
            c.setPosition(pos, QTextCursor::KeepAnchor);
            c.removeSelectedText();
            c.setPosition(pos - n);
            c.setPosition(pos, QTextCursor::KeepAnchor);
            c.removeSelectedText();
        } else {
            c.insertText(mark + mark);
            c.setPosition(pos + n);
        }
        c.endEditBlock();
        setTextCursor(c);
        return;
    }
    int shift = 0, first = 0, last = 0, delta = 0;
    for (int i = ranges.size() - 1; i >= 0; --i) { // back to front keeps earlier offsets valid
        const auto r = toggleRange(d, mark, ranges[i].first, ranges[i].second);
        if (i == ranges.size() - 1)
            last = r.end;
        else
            shift += r.delta;
        if (i == 0)
            first = r.start;
        delta = r.delta;
    }
    c.endEditBlock();
    if (caret >= 0) // no selection: keep the caret inside the word
        c.setPosition(qBound(first, caret + (delta > 0 ? n : -n), last));
    else {
        // Several lines: select the marks too so the same command toggles them back off.
        const int outer = ranges.size() > 1 && delta > 0 ? n : 0;
        c.setPosition(first - outer);
        c.setPosition(last + shift + outer, QTextCursor::KeepAnchor);
    }
    setTextCursor(c);
}
void CodeEditor::setHeading(int level) {
    if (isReadOnly() || !isMarkdown())
        return;
    auto c = textCursor();
    const auto blocks = selectedBlocks(c);
    bool all = level > 0, any = false;
    for (const auto &b : blocks) {
        if (b.text().trimmed().isEmpty() && blocks.size() > 1)
            continue;
        any = true;
        const auto m = headingRe().match(b.text());
        if (!m.hasMatch() || m.captured(1).size() != level)
            all = false;
    }
    c.beginEditBlock();
    for (const auto &b : blocks) {
        if (b.text().trimmed().isEmpty() && blocks.size() > 1)
            continue;
        const auto m = headingRe().match(b.text());
        const auto body = m.hasMatch() ? b.text().mid(m.capturedLength()) : b.text();
        replaceBlockText(b, level > 0 && !(all && any) ? QString(level, QLatin1Char('#')) + ' ' + body : body);
    }
    c.endEditBlock();
}
void CodeEditor::toggleLineKind(LineKind kind) {
    if (isReadOnly() || !isMarkdown())
        return;
    auto c = textCursor();
    const auto blocks = selectedBlocks(c);
    bool all = true;
    for (const auto &b : blocks)
        if (!b.text().trimmed().isEmpty() || blocks.size() == 1)
            all = all && hasKind(kind, b.text());
    c.beginEditBlock();
    int number = 1;
    for (const auto &b : blocks) {
        auto text = b.text();
        if (text.trimmed().isEmpty() && blocks.size() > 1)
            continue;
        if (all) { // remove this kind's marker, keep the indentation
            for (const auto *re : {&taskRe(), &bulletRe(), &numberRe(), &quoteRe()})
                if (re == (kind == LineKind::Task     ? &taskRe()
                           : kind == LineKind::Bullet ? &bulletRe()
                           : kind == LineKind::Number ? &numberRe()
                                                      : &quoteRe())) {
                    const auto m = re->match(text);
                    text = m.captured(1) + text.mid(m.capturedLength());
                }
        } else if (kind == LineKind::Quote) {
            if (!hasKind(kind, text))
                text = QStringLiteral("> ") + text;
        } else {
            QString indent;
            bool stripped = false;
            for (const auto *re : {&taskRe(), &bulletRe(), &numberRe()}) {
                const auto m = re->match(text);
                if (m.hasMatch()) {
                    indent = m.captured(1);
                    text = text.mid(m.capturedLength());
                    stripped = true;
                    break;
                }
            }
            if (!stripped) {
                indent = QRegularExpression(QStringLiteral("^\\s*")).match(text).captured();
                text = text.mid(indent.size());
            }
            text = indent +
                   (kind == LineKind::Bullet   ? QStringLiteral("- ")
                    : kind == LineKind::Task   ? QStringLiteral("- [ ] ")
                                               : QString::number(number++) + QStringLiteral(". ")) +
                   text;
        }
        replaceBlockText(b, text);
    }
    c.endEditBlock();
}
void CodeEditor::insertLink() {
    if (isReadOnly() || !isMarkdown())
        return;
    auto c = textCursor();
    const auto selected = c.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    static const QRegularExpression url(QStringLiteral("^https?://\\S+$"));
    const int start = c.selectionStart();
    c.beginEditBlock();
    if (selected.isEmpty()) {
        c.insertText(QStringLiteral("[text](url)"));
        c.setPosition(start + 1);
        c.setPosition(start + 5, QTextCursor::KeepAnchor);
    } else if (url.match(selected).hasMatch()) {
        c.insertText('[' + QString() + "](" + selected + ')');
        c.setPosition(start + 1);
    } else {
        c.insertText('[' + selected + QStringLiteral("](url)"));
        c.setPosition(start + selected.size() + 3);
        c.setPosition(start + selected.size() + 6, QTextCursor::KeepAnchor);
    }
    c.endEditBlock();
    setTextCursor(c);
}
void CodeEditor::insertCodeBlock() {
    if (isReadOnly() || !isMarkdown())
        return;
    auto c = textCursor();
    auto d = document();
    c.beginEditBlock();
    if (c.hasSelection()) {
        const auto first = d->findBlock(c.selectionStart());
        const auto last = d->findBlock(qMax(c.selectionStart(), c.selectionEnd() - 1));
        const int s = first.position();
        c.setPosition(s);
        c.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
        const auto body = c.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
        c.insertText(QStringLiteral("```\n") + body + QStringLiteral("\n```"));
        c.setPosition(s + 3);
    } else {
        if (!c.block().text().isEmpty()) {
            c.movePosition(QTextCursor::EndOfBlock);
            c.insertText(QStringLiteral("\n"));
        }
        const int s = c.position();
        c.insertText(QStringLiteral("```\n\n```"));
        c.setPosition(s + 4);
    }
    c.endEditBlock();
    setTextCursor(c);
}
bool CodeEditor::continueMarkdownLine() {
    auto c = textCursor();
    if (c.hasSelection() || inFence(c.block()))
        return false;
    const auto text = c.block().text();
    const int pos = c.positionInBlock();
    static const QRegularExpression list(
        QStringLiteral("^(\\s*)(?:([-*+])|(\\d+)([.)]))(\\s+)(\\[[ xX]\\]\\s+)?"));
    static const QRegularExpression quote(QStringLiteral("^(\\s{0,3}(?:>\\s?)+)"));
    QString next;
    int prefixLength = 0;
    if (const auto m = list.match(text); m.hasMatch()) {
        prefixLength = m.capturedLength();
        next = m.captured(1) +
               (m.captured(2).isEmpty() ? QString::number(m.captured(3).toInt() + 1) + m.captured(4)
                                        : m.captured(2)) +
               m.captured(5) + (m.captured(6).isEmpty() ? QString() : QStringLiteral("[ ] "));
    } else if (const auto q = quote.match(text); q.hasMatch()) {
        prefixLength = q.capturedLength();
        next = q.captured(1).trimmed().isEmpty() ? QString() : q.captured(1);
        if (!next.endsWith(' '))
            next += ' ';
    } else
        return false;
    if (pos < prefixLength)
        return false;
    c.beginEditBlock();
    if (text.mid(prefixLength).trimmed().isEmpty()) { // empty item: end the list
        c.movePosition(QTextCursor::StartOfBlock);
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        c.removeSelectedText();
    } else
        c.insertText(QStringLiteral("\n") + next);
    c.endEditBlock();
    setTextCursor(c);
    return true;
}
