#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTextDocument>

class SafeDocument : public QTextDocument {
  public:
    explicit SafeDocument(QObject *parent = nullptr) : QTextDocument(parent) {}

  protected:
    QVariant loadResource(int, const QUrl &) override {
        return {};
    }
};

struct FileData {
    QString path, text, error;
    QByteArray digest;
    QString encoding = "UTF-8", newline = "LF";
    bool mixed = false;
};
class Document : public QObject {
    Q_OBJECT
  public:
    enum class Mode { Text, Markdown, Html };
    explicit Document(Mode mode = Mode::Text, QObject *parent = nullptr);
    SafeDocument *text() const {
        return m_text;
    }
    Mode mode() const {
        return m_mode;
    }
    QString path() const {
        return m_path;
    }
    QString title() const;
    QString sourceText() const {
        return m_text->toRawText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    }
    QString encoding() const {
        return m_encoding;
    }
    QString newline() const {
        return m_newline;
    }
    bool mixed() const {
        return m_mixed;
    }
    // True for an HTML file this editor did not write: saving it rewrites the markup through Qt.
    bool foreignHtml() const {
        return m_foreignHtml;
    }
    void setNewline(const QString &value);
    bool changedOnDisk() const;
    bool isModified() const {
        return m_text->isModified() || m_newline != m_savedNewline;
    }
    static Mode modeForPath(const QString &path);
    static FileData read(const QString &path);
    void apply(const FileData &data);
    bool save(const QString &path, QString *error);
    QJsonObject snapshot() const;
    void restore(const QJsonObject &object);
  signals:
    void metadataChanged();

  private:
    SafeDocument *m_text;
    Mode m_mode;
    QString m_path, m_encoding = "UTF-8", m_newline = "LF", m_savedNewline = "LF";
    QByteArray m_digest;
    bool m_mixed = false, m_foreignHtml = false;
};
