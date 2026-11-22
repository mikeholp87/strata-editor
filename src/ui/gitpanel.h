#pragma once
#include <QProcess>
#include <QStringDecoder>
#include <QTimer>
#include <QWidget>
class QPlainTextEdit;
class QPushButton;
class GitPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GitPanel(QWidget *parent = nullptr);
    ~GitPanel() override;
    QString root;
    void refresh();
    bool isBusy() const;
    void cancel();

  private:
    QProcess process;
    QPlainTextEdit *output;
    QPushButton *cancelButton;
    QTimer killTimer;
    QStringDecoder stdoutDecoder{QStringDecoder::Utf8}, stderrDecoder{QStringDecoder::Utf8};
    qsizetype outputBytes = 0;
    bool truncated = false, readOnlyCommand = false, cancelPending = false;
    void receive(QProcess::ProcessChannel channel, QStringDecoder &decoder);
    void run(const QStringList &args);
};
