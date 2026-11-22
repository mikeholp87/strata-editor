#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <cstdio>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QFile modeFile(".fake-git-mode");
    if (!modeFile.open(QIODevice::ReadOnly))
        return 2;
    const auto mode = modeFile.readAll().trimmed();
    if (mode == "flood") {
        QByteArray chunk(4096, 'a');
        for (int i = 79; i < chunk.size(); i += 80)
            chunk[i] = '\n';
        for (int i = 0; i < 768; ++i) {
            std::fwrite(chunk.constData(), 1, chunk.size(), stdout);
            std::fwrite(chunk.constData(), 1, chunk.size(), stderr);
        }
        return 0;
    }
    if (mode == "unicode") {
        std::fwrite("\xF0", 1, 1, stdout);
        std::fflush(stdout);
        QTimer::singleShot(50, &app, [&] {
            std::fwrite("\x9F\x99\x82", 1, 3, stdout);
            std::fflush(stdout);
            app.quit();
        });
        return app.exec();
    }
    if (mode == "delay") {
        QTimer::singleShot(250, &app, &QCoreApplication::quit);
        return app.exec();
    }
    if (mode == "wait") {
        std::puts("started");
        std::fflush(stdout);
        QTimer::singleShot(30000, &app, &QCoreApplication::quit);
        return app.exec();
    }
    std::puts("## main\n?? example.txt");
    return 0;
}
