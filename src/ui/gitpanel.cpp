#include "gitpanel.h"
#include <QInputDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextCursor>
#include <QVBoxLayout>
GitPanel::GitPanel(QWidget *p) : QWidget(p) {
    auto l = new QVBoxLayout(this);
    l->addWidget(new QLabel(tr("Git • explicit actions only")));
    output = new QPlainTextEdit;
    output->setReadOnly(true);
    output->setPlainText(
        tr("Open a project folder, then choose Refresh status to inspect its Git repository."));
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    output->setMaximumBlockCount(10000);
    l->addWidget(output);
    auto button = [&](const QString &label, auto fn) {
        auto b = new QPushButton(label);
        l->addWidget(b);
        connect(b, &QPushButton::clicked, this, fn);
    };
    button(tr("Refresh status"), [this] { refresh(); });
    button(tr("View diff"), [this] { run({"diff", "--no-ext-diff", "--no-textconv"}); });
    button(tr("Recent commits"), [this] { run({"log", "--no-show-signature", "-15", "--oneline"}); });
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
    cancelButton = new QPushButton(tr("Cancel read-only command"));
    cancelButton->setEnabled(false);
    l->addWidget(cancelButton);
    connect(cancelButton, &QPushButton::clicked, this, &GitPanel::cancel);
    killTimer.setSingleShot(true);
    killTimer.setInterval(1000);
    connect(&killTimer, &QTimer::timeout, &process, &QProcess::kill);
    connect(&process, &QProcess::started, this, [this] {
        if (cancelPending) {
            process.terminate();
            killTimer.start();
        }
    });
    connect(&process, &QProcess::stateChanged, this,
            [this] { cancelButton->setEnabled(isBusy() && readOnlyCommand && !cancelPending); });
    connect(&process, &QProcess::readyReadStandardOutput, this,
            [this] { receive(QProcess::StandardOutput, stdoutDecoder); });
    connect(&process, &QProcess::readyReadStandardError, this,
            [this] { receive(QProcess::StandardError, stderrDecoder); });
    connect(&process, &QProcess::errorOccurred, this,
            [this] { output->appendPlainText(process.errorString()); });
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
                receive(QProcess::StandardOutput, stdoutDecoder);
                receive(QProcess::StandardError, stderrDecoder);
                killTimer.stop();
                output->appendPlainText(cancelPending ? tr("Git command cancelled.")
                                                      : tr("Git exited with code %1").arg(code));
                cancelPending = false;
            });
}
GitPanel::~GitPanel() {
    disconnect(&process, nullptr, this, nullptr);
}
void GitPanel::refresh() {
    run({"status", "--short", "--branch"});
}
void GitPanel::run(const QStringList &args) {
    if (isBusy()) {
        output->appendPlainText(tr("Git is already running. Wait or cancel the read-only command."));
        return;
    }
    if (root.isEmpty()) {
        output->setPlainText(tr("Open a project folder first."));
        return;
    }
    output->setPlainText(tr("Running Git in %1…\n").arg(root));
    const auto executable = QStandardPaths::findExecutable("git");
    if (executable.isEmpty()) {
        output->setPlainText(tr("Git was not found on PATH."));
        return;
    }
    outputBytes = 0;
    truncated = cancelPending = false;
    stdoutDecoder.resetState();
    stderrDecoder.resetState();
    readOnlyCommand = args[0] == "status" || args[0] == "diff" || args[0] == "log";
    process.setWorkingDirectory(root);
    process.start(executable, QStringList{"--no-optional-locks", "-c", "core.fsmonitor=false"} + args);
}

bool GitPanel::isBusy() const {
    return process.state() != QProcess::NotRunning;
}
void GitPanel::cancel() {
    if (!isBusy() || !readOnlyCommand || cancelPending)
        return;
    cancelPending = true;
    cancelButton->setEnabled(false);
    if (process.state() == QProcess::Running) {
        process.terminate();
        killTimer.start();
    }
}
void GitPanel::receive(QProcess::ProcessChannel channel, QStringDecoder &decoder) {
    constexpr qsizetype limit = 1024 * 1024;
    process.setReadChannel(channel);
    while (process.bytesAvailable() > 0) {
        const auto bytes = process.read(64 * 1024);
        const auto allowed = qMin(limit - outputBytes, bytes.size());
        if (allowed > 0) {
            auto cursor = output->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(decoder(QByteArrayView(bytes.constData(), allowed)));
            outputBytes += allowed;
        }
        if (allowed < bytes.size() && !truncated) {
            output->appendPlainText(
                tr("\nOutput truncated at 1 MiB. Use an external Git client for the full output."));
            truncated = true;
        }
    }
}
