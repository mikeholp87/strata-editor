#pragma once
#include <QString>
class QDialog;
class QSettings;
namespace WindowState {
void rememberDialog(QDialog &dialog, QSettings &settings, const QString &name);
}
