#pragma once
#include <QPlainTextEdit>
class Gutter;
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
    bool spaces = true, autoIndent = true, pairs = true, numbers = true;
    void refreshOptions();

  protected:
    void resizeEvent(QResizeEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

  private:
    QWidget *gutter;
    void highlightLine();
};
