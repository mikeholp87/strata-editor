#include "core/export.h"
#include "core/persistence.h"
#include "documentview.h"
#include "editor/codeeditor.h"
#include "editor/highlighter.h"
#include "gitpanel.h"
#include "mainwindow.h"
#include "projectsearch.h"
#include <QJsonArray>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QtConcurrent>
#include <QtWidgets>

void MainWindow::buildFormatBar() {
    formatBar = addToolBar(tr("Rich-text formatting"));
    formatBar->setObjectName("formatBar");
    formatBar->setMovable(false);
    auto format = [this](QString name, QKeySequence key, auto fn) {
        auto a = command("format" + name, name, key, [this, fn] {
            if (!current() || !current()->rich)
                return;
            auto c = current()->cursor();
            c.beginEditBlock();
            fn(c);
            c.endEditBlock();
            current()->setCursor(c);
        });
        formatBar->addAction(a);
    };
    format(tr("Bold"), QKeySequence("Ctrl+B"), [](QTextCursor &c) {
        auto f = c.charFormat();
        f.setFontWeight(f.fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
        c.mergeCharFormat(f);
    });
    format(tr("Italic"), QKeySequence("Ctrl+I"), [](QTextCursor &c) {
        QTextCharFormat f;
        f.setFontItalic(!c.charFormat().fontItalic());
        c.mergeCharFormat(f);
    });
    format(tr("Underline"), QKeySequence("Ctrl+U"), [](QTextCursor &c) {
        QTextCharFormat f;
        f.setFontUnderline(!c.charFormat().fontUnderline());
        c.mergeCharFormat(f);
    });
    format(tr("Strike"), {}, [](QTextCursor &c) {
        QTextCharFormat f;
        f.setFontStrikeOut(!c.charFormat().fontStrikeOut());
        c.mergeCharFormat(f);
    });
    auto styles = new QComboBox;
    styles->addItems({tr("Paragraph"), tr("Heading 1"), tr("Heading 2"), tr("Heading 3")});
    formatBar->addWidget(styles);
    connect(styles, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        if (!current() || !current()->rich)
            return;
        auto c = current()->cursor();
        c.beginEditBlock();
        auto b = c.blockFormat();
        b.setHeadingLevel(index);
        c.setBlockFormat(b);
        QTextCharFormat f;
        f.setFontPointSize(index ? 24 - index * 3 : 12);
        f.setFontWeight(index ? QFont::Bold : QFont::Normal);
        c.select(QTextCursor::BlockUnderCursor);
        c.mergeCharFormat(f);
        c.endEditBlock();
    });
    auto fonts = new QFontComboBox;
    fonts->setMaximumWidth(160);
    formatBar->addWidget(fonts);
    connect(fonts, &QFontComboBox::currentFontChanged, this, [this](QFont font) {
        if (!current() || !current()->rich)
            return;
        auto c = current()->cursor();
        QTextCharFormat f;
        f.setFontFamilies({font.family()});
        c.mergeCharFormat(f);
        current()->setCursor(c);
    });
    auto size = new QSpinBox;
    size->setRange(6, 96);
    size->setValue(12);
    formatBar->addWidget(size);
    connect(size, qOverload<int>(&QSpinBox::valueChanged), this, [this](int n) {
        if (!current() || !current()->rich)
            return;
        auto c = current()->cursor();
        QTextCharFormat f;
        f.setFontPointSize(n);
        c.mergeCharFormat(f);
        current()->setCursor(c);
    });
    format(tr("Color"), {}, [this](QTextCursor &c) {
        auto color = QColorDialog::getColor(Qt::white, this);
        if (color.isValid()) {
            QTextCharFormat f;
            f.setForeground(color);
            c.mergeCharFormat(f);
        }
    });
    format(tr("Highlight"), {}, [this](QTextCursor &c) {
        auto color = QColorDialog::getColor(Qt::yellow, this);
        if (color.isValid()) {
            QTextCharFormat f;
            f.setBackground(color);
            c.mergeCharFormat(f);
        }
    });
    format(tr("Bullets"), {}, [](QTextCursor &c) {
        QTextListFormat f;
        f.setStyle(QTextListFormat::ListDisc);
        c.createList(f);
    });
    format(tr("Numbers"), {}, [](QTextCursor &c) {
        QTextListFormat f;
        f.setStyle(QTextListFormat::ListDecimal);
        c.createList(f);
    });
    for (auto pair :
         {qMakePair(QString("Left"), Qt::AlignLeft), qMakePair(QString("Center"), Qt::AlignHCenter),
          qMakePair(QString("Right"), Qt::AlignRight)})
        format(pair.first, {}, [pair](QTextCursor &c) {
            auto f = c.blockFormat();
            f.setAlignment(pair.second);
            c.setBlockFormat(f);
        });
    format(tr("Link"), {}, [this](QTextCursor &c) {
        bool ok;
        auto link = QInputDialog::getText(this, tr("Insert link"), tr("https:// URL"), QLineEdit::Normal,
                                          "https://", &ok);
        if (!ok || QUrl(link).scheme() != "https")
            return;
        QTextCharFormat f;
        f.setAnchor(true);
        f.setAnchorHref(link);
        f.setFontUnderline(true);
        if (c.hasSelection())
            c.mergeCharFormat(f);
        else
            c.insertText(link, f);
    });
    format(tr("Rule"), {}, [](QTextCursor &c) { c.insertHtml("<hr>"); });
    format(tr("Table"), {}, [](QTextCursor &c) { c.insertTable(3, 3); });
}
void MainWindow::findNext(bool backwards) {
    auto v = current();
    if (!v || findText->text().isEmpty())
        return;
    QTextDocument::FindFlags flags;
    if (matchCase->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    if (wholeWord->isChecked())
        flags |= QTextDocument::FindWholeWords;
    if (backwards)
        flags |= QTextDocument::FindBackward;
    auto search = [&](QTextCursor start) {
        if (regex->isChecked()) {
            QRegularExpression exp(findText->text(), matchCase->isChecked()
                                                         ? QRegularExpression::NoPatternOption
                                                         : QRegularExpression::CaseInsensitiveOption);
            if (!exp.isValid())
                return QTextCursor();
            return v->model->text()->find(exp, start, flags);
        }
        return v->model->text()->find(findText->text(), start, flags);
    };
    auto hit = search(v->cursor());
    if (hit.isNull()) {
        QTextCursor start(v->model->text());
        if (backwards)
            start.movePosition(QTextCursor::End);
        hit = search(start);
    }
    if (hit.isNull())
        statusBar()->showMessage(tr("No match (or invalid regular expression)"), 4000);
    else
        v->setCursor(hit);
}
void MainWindow::replace(bool all) {
    auto v = current();
    if (!v || findText->text().isEmpty())
        return;
    if (v->preview && v->markdownView() == DocumentView::MarkdownView::Preview)
        v->setMarkdownView(DocumentView::MarkdownView::Split);
    if (!all) {
        auto c = v->cursor();
        bool matches =
            regex->isChecked()
                ? QRegularExpression(QRegularExpression::anchoredPattern(findText->text()),
                                     matchCase->isChecked() ? QRegularExpression::NoPatternOption
                                                            : QRegularExpression::CaseInsensitiveOption)
                      .match(c.selectedText())
                      .hasMatch()
                : c.selectedText().compare(findText->text(), matchCase->isChecked()
                                                                 ? Qt::CaseSensitive
                                                                 : Qt::CaseInsensitive) == 0;
        if (c.hasSelection() && matches) {
            c.insertText(replaceText->text());
            v->setCursor(c);
        }
        findNext();
        return;
    }
    QTextDocument::FindFlags flags;
    if (matchCase->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    if (wholeWord->isChecked())
        flags |= QTextDocument::FindWholeWords;
    QRegularExpression exp(regex->isChecked() ? findText->text()
                                              : QRegularExpression::escape(findText->text()),
                           matchCase->isChecked() ? QRegularExpression::NoPatternOption
                                                  : QRegularExpression::CaseInsensitiveOption);
    if (!exp.isValid())
        return;
    QTextCursor transaction(v->model->text());
    transaction.beginEditBlock();
    QTextCursor c(v->model->text());
    int count = 0;
    while (true) {
        auto hit = v->model->text()->find(exp, c, flags);
        if (hit.isNull())
            break;
        if (!hit.hasSelection()) {
            c = hit;
            if (!c.movePosition(QTextCursor::NextCharacter))
                break;
            continue;
        }
        hit.insertText(replaceText->text());
        c = hit;
        ++count;
    }
    transaction.endEditBlock();
    statusBar()->showMessage(tr("Replaced %1 matches; replacement is literal text.").arg(count), 4000);
}
void MainWindow::exportPdf() {
    if (!current())
        return;
    auto path = QFileDialog::getSaveFileName(this, tr("Export PDF"), {}, tr("PDF (*.pdf)"));
    if (path.isEmpty())
        return;
    QString error;
    for (int i = 0; i < tabs->count(); ++i) {
        auto doc = qobject_cast<DocumentView *>(tabs->widget(i))->model;
        if (!doc->path().isEmpty() && QFileInfo(path).canonicalFilePath() == doc->path()) {
            QMessageBox::warning(this, tr("Export"), tr("Choose a file that is not open in the editor."));
            return;
        }
    }
    if (!writeExport(*current()->model, path, true, &error))
        QMessageBox::warning(this, tr("Export failed"), error);
}
void MainWindow::exportHtml() {
    if (!current())
        return;
    auto path = QFileDialog::getSaveFileName(this, tr("Export HTML"), {}, tr("HTML (*.html)"));
    if (path.isEmpty())
        return;
    QString error;
    for (int i = 0; i < tabs->count(); ++i) {
        auto doc = qobject_cast<DocumentView *>(tabs->widget(i))->model;
        if (!doc->path().isEmpty() && QFileInfo(path).canonicalFilePath() == doc->path()) {
            QMessageBox::warning(this, tr("Export"), tr("Choose a file that is not open in the editor."));
            return;
        }
    }
    if (!writeExport(*current()->model, path, false, &error))
        QMessageBox::warning(this, tr("Export failed"), error);
}
