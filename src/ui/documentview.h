#pragma once
#include "core/document.h"
#include <QPointer>
#include <QWidget>
class CodeEditor;
class QTextEdit;
class QTextBrowser;
class QSplitter;
class DocumentView : public QWidget {
    Q_OBJECT
  public:
    explicit DocumentView(Document *document, QWidget *parent = nullptr);
    Document *model;
    CodeEditor *code = nullptr;
    QTextEdit *rich = nullptr;
    QTextBrowser *preview = nullptr;
    CodeEditor *activeCode() const;
    QTextEdit *activeRich() const;
    QTextCursor cursor() const;
    void setCursor(const QTextCursor &cursor);
    QJsonObject viewState() const;
    void restoreViewState(const QJsonObject &state);
    void togglePreview();
    void split(Qt::Orientation orientation);
    void configure(const QFont &font, bool wrap, int tabWidth, bool spaces, bool numbers);
  signals:
    void cursorChanged();

  protected:
    bool eventFilter(QObject *object, QEvent *event) override;

  private:
    QSplitter *panes;
    QPointer<CodeEditor> focusedCode;
    QPointer<QTextEdit> focusedRich;
    void updatePreview();
};
