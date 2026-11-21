#pragma once
#include <QHash>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
class Highlighter : public QSyntaxHighlighter {
  public:
    explicit Highlighter(QTextDocument *doc, const QString &language);
    void setLanguage(const QString &value) {
        if (language == value)
            return;
        language = value;
        rehighlight();
    }
    static QString languageForPath(const QString &path);

  protected:
    void highlightBlock(const QString &text) override;

  private:
    QString language;
    QHash<QString, QRegularExpression> patterns;
};
