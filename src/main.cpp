#include "core/instancebridge.h"
#include "core/persistence.h"
#include "ui/mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>
#include <QTimer>
#include <memory>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("strata-editor");
    app.setApplicationDisplayName(STRATA_PRODUCT_NAME);
    app.setOrganizationName("StrataEditor");
    app.setApplicationVersion(STRATA_VERSION);
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
    const auto requestDirectory = Persistence::dataPath("open-requests");
    const bool primary = parser.isSet("smoke-test") || parser.isSet("screenshot") || instanceLock.tryLock(0);
    if (!primary) {
        // Another instance owns the recovery and session files: ask it to open the files instead.
        QString error;
        if (InstanceBridge::send(requestDirectory, parser.positionalArguments(), &error))
            return 0;
        QMessageBox::warning(nullptr, STRATA_PRODUCT_NAME,
                             "Another instance is running and could not be reached, or the application data "
                             "directory is not writable. " + error);
        return 1;
    }
    MainWindow window(!parser.isSet("no-session") && !parser.isSet("smoke-test") &&
                      !parser.isSet("screenshot"), parser.isSet("no-session") || parser.isSet("smoke-test") ||
                      parser.isSet("screenshot"));
    auto open = [&](const QString &path) {
        if (QFileInfo(path).isDir())
            window.setProject(path);
        else
            window.openFile(path);
    };
    for (const auto &path : parser.positionalArguments())
        open(path);
    // Only the real instance (holding the lock) serves requests; smoke and screenshot runs must not steal them.
    std::unique_ptr<InstanceBridge> bridge;
    if (instanceLock.isLocked()) {
        bridge = std::make_unique<InstanceBridge>(requestDirectory);
        QObject::connect(bridge.get(), &InstanceBridge::requested, &window, [&](const QStringList &paths) {
            for (const auto &path : paths)
                open(path);
            window.setWindowState(window.windowState() & ~Qt::WindowMinimized);
            window.show();
            window.raise();
            window.activateWindow();
        });
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
