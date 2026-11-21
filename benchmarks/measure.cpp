#include "core/document.h"
#include "editor/codeeditor.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdio>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir dir;
    QJsonArray results;
    for (int mib : {1, 10, 100}) {
        auto path = dir.filePath(QString::number(mib) + ".txt");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return 1;
        QByteArray block(1024 * 1024, 'x');
        for (int i = 79; i < block.size(); i += 80)
            block[i] = '\n';
        for (int i = 0; i < mib; ++i)
            if (file.write(block) != block.size())
                return 1;
        file.close();
        QElapsedTimer timer;
        timer.start();
        auto data = Document::read(path);
        double readMs = timer.nsecsElapsed() / 1e6;
        QJsonObject row{{"MiB", mib}, {"read_or_refusal_ms", readMs}, {"accepted", data.error.isEmpty()}};
        if (data.error.isEmpty()) {
            Document document;
            CodeEditor editor;
            editor.resize(1000, 700);
            timer.restart();
            document.apply(data);
            editor.setDocument(document.text());
            editor.show();
            app.processEvents();
            row["populate_and_first_layout_ms"] = timer.nsecsElapsed() / 1e6;
            timer.restart();
            editor.insertPlainText("typed");
            app.processEvents();
            row["one_insert_and_events_ms"] = timer.nsecsElapsed() / 1e6;
        } else
            row["reason"] = data.error;
        results.append(row);
    }
    auto bytes = QJsonDocument(QJsonObject{{"Qt", qVersion()}, {"measurements", results}}).toJson();
    std::fwrite(bytes.constData(), 1, bytes.size(), stdout);
}
