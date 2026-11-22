#include "core/persistence.h"
#include "ui/mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>
#include <QTimer>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("strata-editor");
    app.setApplicationDisplayName(STRATA_PRODUCT_NAME);
    app.setOrganizationName("StrataEditor");
    app.setApplicationVersion("0.1.0");
    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/strata/strata-editor.svg"));
    QCommandLineParser parser;
    parser.setApplicationDescription("Native text, Markdown and rich-text editor");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"no-session", "Start without restoring a session"});
    parser.addOption({"screenshot", "Save a window screenshot and exit", "path"});
    parser.addOption({"smoke-test", "Launch and exit after 500 ms"});
    parser.addPositionalArgument("files", "Files or a project folder to open", "[files…]");
    parser.process(app);
    QLockFile instanceLock(Persistence::dataPath("instance.lock"));
    if (!parser.isSet("smoke-test") && !parser.isSet("screenshot") && !instanceLock.tryLock(0)) {
        QMessageBox::warning(
            nullptr, STRATA_PRODUCT_NAME,
            "Another instance is running, or the application data directory is not writable. "
            "Use its Open action to protect shared recovery and session files.");
        return 1;
    }
    MainWindow window(!parser.isSet("no-session") && !parser.isSet("smoke-test") &&
                      !parser.isSet("screenshot"), parser.isSet("no-session") || parser.isSet("smoke-test") ||
                      parser.isSet("screenshot"));
    for (auto path : parser.positionalArguments()) {
        if (QFileInfo(path).isDir())
            window.setProject(path);
        else
            window.openFile(path);
    }
    window.show();
    if (parser.isSet("screenshot"))
        QTimer::singleShot(800, &window, [&] {
            window.grab().save(parser.value("screenshot"));
            app.quit();
        });
    if (parser.isSet("smoke-test"))
        QTimer::singleShot(500, &app, &QApplication::quit);
    return app.exec();
}
