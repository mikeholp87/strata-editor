#include "windowstate.h"
#include <QDialog>
#include <QSettings>

void WindowState::rememberDialog(QDialog &dialog, QSettings &settings, const QString &name) {
    const auto key = QString("windows/%1/geometry").arg(name);
    dialog.restoreGeometry(settings.value(key).toByteArray());
    QObject::connect(&dialog, &QDialog::finished, &dialog, [&dialog, &settings, key] {
        settings.setValue(key, dialog.saveGeometry());
        settings.sync();
    });
}
