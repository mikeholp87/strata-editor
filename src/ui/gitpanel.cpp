#include "gitpanel.h"
#include <QInputDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
GitPanel::GitPanel(QWidget *p) : QWidget(p) {
    auto l = new QVBoxLayout(this);
    l->addWidget(new QLabel(tr("Git • explicit actions only")));
    output = new QPlainTextEdit;
    output->setReadOnly(true);
    l->addWidget(output);
    auto button = [&](const QString &label, auto fn) {
        auto b = new QPushButton(label);
        l->addWidget(b);
        connect(b, &QPushButton::clicked, this, fn);
    };
    button(tr("Refresh status"), [this] { refresh(); });
    button(tr("View diff"), [this] { run({"diff", "--no-ext-diff", "--no-textconv"}); });
    button(tr("Recent commits"), [this] { run({"log", "-15", "--oneline"}); });
    button(tr("Stage file…"), [this] {
        bool ok;
        auto p = QInputDialog::getText(this, tr("Stage file"), tr("Project-relative path"), QLineEdit::Normal,
                                       {}, &ok);
        if (ok && !p.isEmpty())
            run({"add", "--", p});
    });
    button(tr("Unstage file…"), [this] {
        bool ok;
        auto p = QInputDialog::getText(this, tr("Unstage file"), tr("Project-relative path"),
                                       QLineEdit::Normal, {}, &ok);
        if (ok && !p.isEmpty())
            run({"restore", "--staged", "--", p});
    });
    button(tr("Commit staged changes…"), [this] {
        bool ok;
        auto message = QInputDialog::getText(this, tr("Commit staged changes"),
                                             tr("Commit message (configured Git hooks may run)"),
                                             QLineEdit::Normal, {}, &ok);
        if (ok && !message.trimmed().isEmpty())
            run({"commit", "-m", message});
    });
    connect(&process, &QProcess::readyReadStandardOutput, this,
            [this] { output->appendPlainText(QString::fromUtf8(process.readAllStandardOutput())); });
    connect(&process, &QProcess::readyReadStandardError, this,
            [this] { output->appendPlainText(QString::fromUtf8(process.readAllStandardError())); });
    connect(&process, &QProcess::errorOccurred, this,
            [this] { output->appendPlainText(process.errorString()); });
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
                output->appendPlainText(tr("Git exited with code %1").arg(code));
            });
}
void GitPanel::refresh() {
    run({"status", "--short", "--branch"});
}
void GitPanel::run(const QStringList &args) {
    if (root.isEmpty()) {
        output->setPlainText(tr("Open a project folder first."));
        return;
    }
    if (process.state() != QProcess::NotRunning)
        return;
    output->clear();
    process.setWorkingDirectory(root);
    process.start("git", args);
}
