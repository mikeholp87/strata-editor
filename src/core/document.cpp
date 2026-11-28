#include "document.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QPlainTextDocumentLayout>
#include <QSaveFile>
#include <QStringConverter>

static QByteArray digest(const QByteArray &bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}
Document::Document(Mode mode, QObject *parent)
    : QObject(parent), m_text(new SafeDocument(this)), m_mode(mode) {
    if (mode != Mode::Html)
        m_text->setDocumentLayout(new QPlainTextDocumentLayout(m_text));
    m_text->setUndoRedoEnabled(true);
}
Document::Mode Document::modeForPath(const QString &p) {
    auto ext = QFileInfo(p).suffix().toLower();
    if (ext == "md" || ext == "markdown")
        return Mode::Markdown;
    if (ext == "html" || ext == "htm")
        return Mode::Html;
    return Mode::Text;
}
QString Document::title() const {
    return m_path.isEmpty() ? (m_mode == Mode::Html       ? tr("Untitled.html")
                               : m_mode == Mode::Markdown ? tr("Untitled.md")
                                                          : tr("Untitled"))
                            : QFileInfo(m_path).fileName();
}
FileData Document::read(const QString &path) {
    FileData d;
    d.path = QFileInfo(path).canonicalFilePath();
    const QFileInfo info(path);
    if (info.exists() && !info.isFile()) {
        d.error = tr("Only regular files can be opened as text; pipes and devices are not supported.");
        return d;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        d.error = f.errorString();
        return d;
    }
    if (f.size() > 8 * 1024 * 1024) {
        d.error = tr("Files above 8 MiB need the planned large-file viewer. This file has not been opened.");
        return d;
    }
    auto bytes = f.read(8 * 1024 * 1024 + 1);
    if (f.error() != QFileDevice::NoError) {
        d.error = f.errorString();
        return d;
    }
    if (bytes.size() > 8 * 1024 * 1024) {
        d.error = tr("The file grew beyond the 8 MiB opening limit.");
        return d;
    }
    d.digest = digest(bytes);
    QStringConverter::Encoding codec = QStringConverter::Utf8;
    if (bytes.startsWith("\xEF\xBB\xBF")) {
        d.encoding = "UTF-8 BOM";
        bytes.remove(0, 3);
    } else if (bytes.startsWith("\xFF\xFE")) {
        codec = QStringConverter::Utf16LE;
        d.encoding = "UTF-16 LE";
        bytes.remove(0, 2);
    } else if (bytes.startsWith("\xFE\xFF")) {
        codec = QStringConverter::Utf16BE;
        d.encoding = "UTF-16 BE";
        bytes.remove(0, 2);
    } else if (bytes.contains(char(0))) {
        d.error = tr("Binary or BOM-less UTF-16 file; opening was refused.");
        return d;
    }
    if ((codec != QStringConverter::Utf8 && bytes.size() % 2 != 0) ||
        (codec == QStringConverter::Utf8 && !bytes.isValidUtf8())) {
        d.error = tr("Truncated or invalid text encoding.");
        return d;
    }
    QStringDecoder decoder(codec,
                           QStringConverter::Flag::Stateless | QStringConverter::Flag::ConvertInitialBom);
    d.text = decoder(bytes);
    if (decoder.hasError() || (d.text.contains(QChar(0)) || d.text.contains(QChar::ParagraphSeparator))) {
        d.error = tr("Invalid encoding, NUL, or unsupported U+2029 paragraph separator. Expected UTF-8 or "
                     "BOM-tagged UTF-16.");
        return d;
    }
    auto crlf = d.text.count("\r\n"), lf = d.text.count('\n') - crlf, cr = d.text.count('\r') - crlf;
    d.mixed = (int(crlf > 0) + int(lf > 0) + int(cr > 0)) > 1;
    d.newline = crlf > lf && crlf >= cr ? "CRLF" : cr > lf ? "CR" : "LF";
    d.text.replace("\r\n", "\n");
    d.text.replace('\r', '\n');
    return d;
}
void Document::apply(const FileData &d) {
    m_path = d.path;
    m_digest = d.digest;
    m_encoding = d.encoding;
    m_newline = d.newline;
    m_savedNewline = m_newline;
    m_mixed = d.mixed;
    m_foreignHtml = m_mode == Mode::Html && !d.text.trimmed().isEmpty() && !d.text.contains("qrichtext");
    if (m_mode == Mode::Html)
        m_text->setHtml(d.text);
    else
        m_text->setPlainText(d.text);
    m_text->setModified(false);
    emit metadataChanged();
}
void Document::setNewline(const QString &v) {
    if (v != m_newline) {
        m_newline = v;
        emit metadataChanged();
    }
}
bool Document::changedOnDisk() const {
    if (m_path.isEmpty())
        return false;
    if (!QFileInfo(m_path).isFile())
        return true;
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 8 * 1024 * 1024)
        return true;
    const auto bytes = f.read(8 * 1024 * 1024 + 1);
    return f.error() != QFileDevice::NoError || digest(bytes) != m_digest;
}
bool Document::save(const QString &destination, QString *error) {
    const auto absolute = QFileInfo(destination).absoluteFilePath();
    const bool same =
        !m_path.isEmpty() && (absolute == m_path || QFileInfo(absolute).canonicalFilePath() == m_path);
    if (same && changedOnDisk()) {
        *error = tr("The file changed or disappeared on disk. Reload it or save a separate copy.");
        return false;
    }
    QFileInfo info(absolute);
    if (info.isSymLink()) {
        *error = tr("Save through the resolved target path, rather than replacing a symbolic link.");
        return false;
    }
    if (info.exists() && !(info.permissions() &
                           (QFileDevice::WriteOwner | QFileDevice::WriteGroup | QFileDevice::WriteOther))) {
        *error = tr("The file is read-only.");
        return false;
    }
    QString content = m_mode == Mode::Html ? m_text->toHtml() : sourceText();
    if (m_newline == "CRLF")
        content.replace("\n", "\r\n");
    else if (m_newline == "CR")
        content.replace('\n', '\r');
    QStringEncoder utf8(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    const QByteArray utf8Bytes = utf8(content);
    if (utf8.hasError() || content.contains(QChar(0))) {
        *error = tr("The document contains invalid Unicode or a NUL character. The file was not changed.");
        return false;
    }
    QByteArray bytes;
    if (m_encoding.startsWith("UTF-16")) {
        bool le = m_encoding.endsWith("LE");
        QStringEncoder enc(le ? QStringConverter::Utf16LE : QStringConverter::Utf16BE);
        bytes = le ? QByteArray::fromHex("fffe") : QByteArray::fromHex("feff");
        bytes += enc(content);
    } else {
        if (m_encoding == "UTF-8 BOM")
            bytes = QByteArray::fromHex("efbbbf");
        bytes += utf8Bytes;
    }
    if (bytes.size() > 8 * 1024 * 1024) {
        *error = tr("The encoded document exceeds the 8 MiB opening limit. Reduce its size or save it using another editor.");
        return false;
    }
    QSaveFile f(absolute);
    f.setDirectWriteFallback(false);
    if (!f.open(QIODevice::WriteOnly)) {
        *error = f.errorString();
        return false;
    }
    if (info.exists() && !f.setPermissions(info.permissions())) {
        *error = tr("Could not preserve file permissions.");
        return false;
    }
    if (f.write(bytes) != bytes.size()) {
        *error = f.errorString();
        return false;
    }
    // shortcut: optimistic disk check; non-cooperating writers still need an OS-level locking protocol.
    if (same && changedOnDisk()) {
        f.cancelWriting();
        *error = tr("The file changed while saving. Reload it or save a separate copy.");
        return false;
    }
    if (!f.commit()) {
        *error = f.errorString();
        return false;
    }
    m_path = QFileInfo(absolute).canonicalFilePath();
    m_digest = digest(bytes);
    m_mixed = false;
    m_foreignHtml = false;
    m_savedNewline = m_newline;
    m_text->setModified(false);
    emit metadataChanged();
    return true;
}
QJsonObject Document::snapshot() const {
    return {{"path", m_path},
            {"mode", int(m_mode)},
            {"text", m_mode == Mode::Html ? m_text->toHtml() : sourceText()},
            {"encoding", m_encoding},
            {"newline", m_newline},
            {"digest", QString::fromLatin1(m_digest.toHex())}};
}
void Document::restore(const QJsonObject &o) {
    m_path = o["path"].toString();
    m_encoding = o["encoding"].toString("UTF-8");
    m_newline = o["newline"].toString("LF");
    m_digest = QByteArray::fromHex(o["digest"].toString().toLatin1());
    if (m_mode == Mode::Html)
        m_text->setHtml(o["text"].toString());
    else
        m_text->setPlainText(o["text"].toString());
    m_text->setModified(true);
    emit metadataChanged();
}
