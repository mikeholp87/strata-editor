#include "editor/codeeditor.h"
#include "editor/completion.h"
#include <QCompleter>
#include <QtTest>
#include <QtWidgets>

class EditingTests : public QObject {
    Q_OBJECT
    static void select(CodeEditor &e, int from, int to) {
        auto c = e.textCursor();
        c.setPosition(from);
        c.setPosition(to, QTextCursor::KeepAnchor);
        e.setTextCursor(c);
    }
    static void caret(CodeEditor &e, int pos) {
        auto c = e.textCursor();
        c.setPosition(pos);
        e.setTextCursor(c);
    }
    static void md(CodeEditor &e, const QString &text) {
        e.setProperty("language", "Markdown");
        e.setPlainText(text);
    }
  private slots:
    void candidatesSkipTheWordBeingTypedAndAddKeywords() {
        const QString text = "alphabet beta alp gamma_ray alphabet";
        auto words = Completion::candidates(text, 14, 17, "Python"); // "alp" is being typed
        QVERIFY(words.contains("alphabet"));
        QVERIFY(words.contains("gamma_ray"));
        QVERIFY(words.contains("def")); // Python keyword
        QVERIFY(!words.contains("alp"));
        QCOMPARE(words.count("alphabet"), 1);
        QVERIFY(!Completion::candidates(text, 0, 0, "Plain text").contains("def"));
    }
    void inlineMarksToggleAndStack() {
        CodeEditor e;
        md(e, "hello world");
        select(e, 0, 5);
        e.toggleInline("**");
        QCOMPARE(e.toPlainText(), QString("**hello** world"));
        QCOMPARE(e.textCursor().selectedText(), QString("hello"));
        e.toggleInline("*");
        QCOMPARE(e.toPlainText(), QString("***hello*** world"));
        e.toggleInline("**");
        QCOMPARE(e.toPlainText(), QString("*hello* world"));
        e.toggleInline("*");
        QCOMPARE(e.toPlainText(), QString("hello world"));
        // marks inside the selection are removed too
        md(e, "**hello** world");
        select(e, 0, 9);
        e.toggleInline("**");
        QCOMPARE(e.toPlainText(), QString("hello world"));
    }
    void inlineMarkWithoutSelection() {
        CodeEditor e;
        md(e, "one two");
        caret(e, 5); // inside "two"
        e.toggleInline("`");
        QCOMPARE(e.toPlainText(), QString("one `two`"));
        QCOMPARE(e.textCursor().position(), 6);
        e.toggleInline("`");
        QCOMPARE(e.toPlainText(), QString("one two"));
        md(e, "x ");
        caret(e, 2);
        e.toggleInline("**");
        QCOMPARE(e.toPlainText(), QString("x ****"));
        QCOMPARE(e.textCursor().position(), 4);
        e.toggleInline("**"); // caret between empty marks removes them
        QCOMPARE(e.toPlainText(), QString("x "));
    }
    void inlineMarksApplyPerLineAndSkipPadding() {
        CodeEditor e;
        md(e, "  one \n\ntwo");
        select(e, 0, e.toPlainText().size());
        e.toggleInline("*");
        QCOMPARE(e.toPlainText(), QString("  *one* \n\n*two*"));
        e.toggleInline("*");
        QCOMPARE(e.toPlainText(), QString("  one \n\ntwo"));
    }
    void inlineMarksIgnoreNonMarkdown() {
        CodeEditor e;
        e.setPlainText("plain");
        select(e, 0, 5);
        e.toggleInline("**");
        e.setHeading(1);
        QCOMPARE(e.toPlainText(), QString("plain"));
    }
    void headings() {
        CodeEditor e;
        md(e, "title\n## other\n\nbody");
        caret(e, 2);
        e.setHeading(1);
        QCOMPARE(e.document()->findBlockByNumber(0).text(), QString("# title"));
        e.setHeading(1); // same level again clears it
        QCOMPARE(e.document()->findBlockByNumber(0).text(), QString("title"));
        select(e, 0, e.toPlainText().size());
        e.setHeading(3);
        QCOMPARE(e.toPlainText(), QString("### title\n### other\n\n### body"));
        e.setHeading(0);
        QCOMPARE(e.toPlainText(), QString("title\nother\n\nbody"));
    }
    void listsQuotesAndTasks() {
        CodeEditor e;
        md(e, "a\n\n  b\nc");
        select(e, 0, e.toPlainText().size());
        e.toggleLineKind(CodeEditor::LineKind::Bullet);
        QCOMPARE(e.toPlainText(), QString("- a\n\n  - b\n- c"));
        e.toggleLineKind(CodeEditor::LineKind::Number); // switch kind, keep indent
        QCOMPARE(e.toPlainText(), QString("1. a\n\n  2. b\n3. c"));
        e.toggleLineKind(CodeEditor::LineKind::Number); // toggle off
        QCOMPARE(e.toPlainText(), QString("a\n\n  b\nc"));
        e.toggleLineKind(CodeEditor::LineKind::Task);
        QCOMPARE(e.toPlainText(), QString("- [ ] a\n\n  - [ ] b\n- [ ] c"));
        e.toggleLineKind(CodeEditor::LineKind::Bullet); // tasks are not plain bullets
        QCOMPARE(e.toPlainText(), QString("- a\n\n  - b\n- c"));
        e.toggleLineKind(CodeEditor::LineKind::Bullet);
        e.toggleLineKind(CodeEditor::LineKind::Quote);
        QCOMPARE(e.toPlainText(), QString("> a\n\n>   b\n> c"));
        e.toggleLineKind(CodeEditor::LineKind::Quote);
        QCOMPARE(e.toPlainText(), QString("a\n\n  b\nc"));
    }
    void links() {
        CodeEditor e;
        md(e, "see docs and https://example.com/x");
        select(e, 4, 8);
        e.insertLink();
        QCOMPARE(e.toPlainText(), QString("see [docs](url) and https://example.com/x"));
        QCOMPARE(e.textCursor().selectedText(), QString("url"));
        md(e, "https://example.com/x");
        select(e, 0, e.toPlainText().size());
        e.insertLink();
        QCOMPARE(e.toPlainText(), QString("[](https://example.com/x)"));
        QCOMPARE(e.textCursor().position(), 1);
        md(e, "");
        e.insertLink();
        QCOMPARE(e.toPlainText(), QString("[text](url)"));
        QCOMPARE(e.textCursor().selectedText(), QString("text"));
    }
    void codeBlocks() {
        CodeEditor e;
        md(e, "one\ntwo\nthree");
        select(e, 2, 6); // spans the first two lines
        e.insertCodeBlock();
        QCOMPARE(e.toPlainText(), QString("```\none\ntwo\n```\nthree"));
        md(e, "intro");
        caret(e, 5);
        e.insertCodeBlock();
        QCOMPARE(e.toPlainText(), QString("intro\n```\n\n```"));
        QCOMPARE(e.textCursor().blockNumber(), 2);
    }
    void enterContinuesLists() {
        CodeEditor e;
        md(e, "- one");
        caret(e, 5);
        QTest::keyClick(&e, Qt::Key_Return);
        QCOMPARE(e.toPlainText(), QString("- one\n- "));
        QTest::keyClick(&e, Qt::Key_Return); // empty item ends the list
        QCOMPARE(e.toPlainText(), QString("- one\n"));
        md(e, "  9. nine");
        caret(e, 9);
        QTest::keyClick(&e, Qt::Key_Return);
        QCOMPARE(e.toPlainText(), QString("  9. nine\n  10. "));
        md(e, "- [x] done");
        caret(e, 10);
        QTest::keyClick(&e, Qt::Key_Return);
        QCOMPARE(e.toPlainText(), QString("- [x] done\n- [ ] "));
        md(e, "> quoted");
        caret(e, 8);
        QTest::keyClick(&e, Qt::Key_Return);
        QCOMPARE(e.toPlainText(), QString("> quoted\n> "));
        md(e, "- split here");
        caret(e, 7);
        QTest::keyClick(&e, Qt::Key_Return);
        QCOMPARE(e.toPlainText(), QString("- split\n-  here"));
    }
    void enterDoesNotContinueInsideFencesOrPlainText() {
        CodeEditor e;
        md(e, "```\n- not a list");
        caret(e, e.toPlainText().size());
        QTest::keyClick(&e, Qt::Key_Return);
        QCOMPARE(e.toPlainText(), QString("```\n- not a list\n"));
        e.setProperty("language", "Plain text");
        e.setPlainText("- item");
        caret(e, 6);
        QTest::keyClick(&e, Qt::Key_Return);
        QCOMPARE(e.toPlainText(), QString("- item\n"));
    }
    void completionPopupFollowsTyping() {
        CodeEditor e;
        e.setProperty("language", "Plain text");
        e.resize(400, 200);
        e.show();
        QVERIFY(QTest::qWaitForWindowExposed(&e));
        QTest::keyClicks(&e, "alphabet beta alp");
        auto popup = e.findChild<QCompleter *>()->popup();
        QVERIFY(popup->isVisible());
        QCOMPARE(popup->model()->rowCount(), 1);
        QTest::keyClick(popup, Qt::Key_Tab); // accept
        QCOMPARE(e.toPlainText(), QString("alphabet beta alphabet"));
        QVERIFY(!popup->isVisible());
        QTest::keyClicks(&e, " alp");
        QVERIFY(popup->isVisible());
        QTest::keyClick(popup, Qt::Key_Escape);
        QVERIFY(!popup->isVisible());
        QCOMPARE(e.toPlainText(), QString("alphabet beta alphabet alp"));
        e.completion = false;
        QTest::keyClicks(&e, "h");
        QVERIFY(!popup->isVisible());
    }
    void forcedCompletionAcceptsShortPrefix() {
        CodeEditor e;
        e.setProperty("language", "Plain text");
        e.show();
        QVERIFY(QTest::qWaitForWindowExposed(&e));
        QTest::keyClicks(&e, "banana b");
        auto popup = e.findChild<QCompleter *>()->popup();
        QVERIFY(!popup->isVisible()); // below the automatic 3-letter threshold
        e.showCompletion(true);
        QVERIFY(popup->isVisible());
        popup->hide();
    }
};
QTEST_MAIN(EditingTests)
#include "test_editing.moc"
