#pragma once
#include <QHash>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT
  public:
    explicit Highlighter(QTextDocument *doc, const QString &language);
    void setLightTheme(bool value);
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
    bool light = false;
    QHash<QString, QRegularExpression> patterns;
};
