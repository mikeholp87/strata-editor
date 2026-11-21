#include "export.h"
#include <QFileInfo>
#include <QPrinter>
#include <QSaveFile>
#include <QTemporaryFile>
bool writeExport(const Document &document, const QString &path, bool pdf, QString *error) {
    if (!document.path().isEmpty() && QFileInfo(path).canonicalFilePath() == document.path()) {
        *error = QObject::tr("Choose a separate export file; the source document must be preserved.");
        return false;
    }
    SafeDocument rendered;
    if (document.mode() == Document::Mode::Markdown)
        rendered.setMarkdown(document.sourceText(),
                             QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) |
                                 QTextDocument::MarkdownNoHTML);
    else if (document.mode() == Document::Mode::Html)
        rendered.setHtml(document.text()->toHtml());
    else
        rendered.setPlainText(document.sourceText());
    QByteArray bytes;
    if (pdf) {
        QTemporaryFile temp(QFileInfo(path).absolutePath() + "/.strata-pdf-XXXXXX");
        if (!temp.open()) {
            *error = temp.errorString();
            return false;
        }
        auto tempPath = temp.fileName();
        temp.close();
        QPrinter printer;
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(tempPath);
        rendered.print(&printer);
        if (printer.printerState() == QPrinter::Error || !temp.open()) {
            *error = QObject::tr("PDF generation failed.");
            return false;
        }
        bytes = temp.readAll();
        if (!bytes.startsWith("%PDF-")) {
            *error = QObject::tr("PDF generation produced no document.");
            return false;
        }
    } else
        bytes = rendered.toHtml().toUtf8();
    QSaveFile output(path);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        *error = output.errorString();
        return false;
    }
    return true;
}
