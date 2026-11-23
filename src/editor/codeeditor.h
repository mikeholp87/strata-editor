#pragma once
#include <QPlainTextEdit>
class Gutter;
class QCompleter;
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
  public:
    explicit CodeEditor(QWidget *parent = nullptr);
    int gutterWidth() const;
    void paintGutter(QPaintEvent *event);
    void indent(bool reverse = false);
    void toggleComment();
    void duplicateLine();
    int tabWidth = 4;
    bool spaces = true, autoIndent = true, pairs = true, numbers = true, completion = true;
    // Word and keyword completion popup. `force` (Ctrl+Space) also accepts short prefixes.
    void showCompletion(bool force = false);
    // Markdown source editing. All are no-ops unless the "language" property is Markdown.
    enum class LineKind { Bullet, Number, Task, Quote };
    bool isMarkdown() const;
    void toggleInline(const QString &mark);
    void setHeading(int level);
    void toggleLineKind(LineKind kind);
    void insertLink();
    void insertCodeBlock();
    void refreshOptions();

  protected:
    void resizeEvent(QResizeEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

  private:
    QWidget *gutter;
    QCompleter *completer;
    void highlightLine();
    void handleKey(QKeyEvent *);
    bool continueMarkdownLine();
    void insertCompletion(const QString &text);
};
