#pragma once
#include <QProcess>
#include <QWidget>
class QPlainTextEdit;
class GitPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GitPanel(QWidget *parent = nullptr);
    QString root;
    void refresh();

  private:
    QProcess process;
    QPlainTextEdit *output;
    void run(const QStringList &args);
};
