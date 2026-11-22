#include "core/persistence.h"
#include "core/recovery.h"
#include "documentview.h"
#include "editor/codeeditor.h"
#include "editor/highlighter.h"
#include "gitpanel.h"
#include "mainwindow.h"
#include "projectsearch.h"
#include <QJsonArray>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QtConcurrent>
#include <QtWidgets>

void MainWindow::recoverySnapshot() {
    if (preserveRecovery || recoveryBlocked || !settings.value("recovery", true).toBool())
        return;
    QJsonArray buffers;
    for (int i = 0; i < tabs->count(); ++i) {
        auto d = qobject_cast<DocumentView *>(tabs->widget(i))->model;
        if (d->isModified())
            buffers.append(d->snapshot());
    }
    QString error;
    if (!Recovery::save(Persistence::dataPath("recovery.json"), {{"buffers", buffers}}, &error))
        statusBar()->showMessage(tr("Recovery snapshot failed: %1. Save your documents.").arg(error), 10000);
}
void MainWindow::saveSession() {
    QJsonArray documents;
    for (int i = 0; i < tabs->count(); ++i) {
        auto v = qobject_cast<DocumentView *>(tabs->widget(i));
        if (!v->model->path().isEmpty())
            documents.append(QJsonObject{{"path", v->model->path()}, {"view", v->viewState()}});
    }
    QString error;
    const bool saved = preserveRecovery || Persistence::write(Persistence::dataPath("session.json"),
                       {{"documents", documents},
                        {"active", current() ? current()->model->path() : ""},
                        {"project", project}}, &error);
    settings.setValue("geometry", saveGeometry());
    settings.setValue("layout", focusMode ? focusLayout : saveState());
    settings.sync();
    if (!saved || settings.status() != QSettings::NoError)
        QMessageBox::warning(this, tr("Workspace save failed"),
                             tr("The workspace or window settings could not be saved. %1").arg(error));
}
void MainWindow::restoreSession() {
    const auto recoveryPath = Persistence::dataPath("recovery.json");
    auto result = Recovery::load(recoveryPath);
    if (!result.error.isEmpty()) {
        QString error;
        const auto preserved = Recovery::preserveInvalid(recoveryPath, &error);
        recoveryBlocked = preserved.isEmpty();
        QMessageBox::warning(this, tr("Recovery data could not be read"),
                             result.error + "\n" +
                                 (recoveryBlocked ? tr("The original was left untouched. Recovery snapshots "
                                                       "are disabled for this session: %1")
                                                        .arg(error)
                                                  : tr("The original was preserved at %1").arg(preserved)));
    }
    if (result.missing || !result.error.isEmpty()) {
        for (const auto &path : Recovery::backupPaths(recoveryPath)) {
            const auto backup = Recovery::load(path);
            if (backup.error.isEmpty() && !backup.value["buffers"].toArray().isEmpty()) {
                result.value = backup.value;
                break;
            }
        }
    }
    auto recovery = result.value["buffers"].toArray();
    QSet<QString> recovered;
    if (!recovery.isEmpty()) {
        auto answer = QMessageBox::question(this, tr("Recover documents"),
                                            tr("%1 unsaved document snapshot(s) found. Restore them? "
                                               "Choosing No keeps the snapshots in recovery-backup.json.")
                                                .arg(recovery.size()));
        bool restoreBuffers = answer == QMessageBox::Yes;
        if (!restoreBuffers &&
            !Persistence::write(Persistence::dataPath("recovery-backup.json"), {{"buffers", recovery}})) {
            QMessageBox::warning(this, tr("Recovery backup failed"),
                                 tr("The backup could not be written. The snapshots will be restored so you "
                                    "can save them safely."));
            restoreBuffers = true;
        }
        if (restoreBuffers) {
            for (auto value : recovery) {
                auto o = value.toObject();
                int mode = o["mode"].toInt();
                if (mode < 0 || mode > 2)
                    continue;
                auto doc = new Document(Document::Mode(mode), this);
                doc->restore(o);
                attach(doc);
                recovered.insert(doc->path());
            }
        }
    }
    if (!settings.value("restore", true).toBool())
        return;
    const auto storedSession = Persistence::readChecked(Persistence::dataPath("session.json"));
    if (!storedSession.error.isEmpty())
        QMessageBox::warning(this, tr("Session could not be restored"), storedSession.error);
    const auto session = storedSession.value;
    setProject(session["project"].toString());

    pendingActive = session["active"].toString();
    for (auto value : session["documents"].toArray()) {
        auto o = value.toObject();
        auto path = o["path"].toString();
        if (recovered.contains(path) || !QFileInfo::exists(path))
            continue;
        pendingViews.insert(QFileInfo(path).canonicalFilePath(), o["view"].toObject());
        openFile(path);
    }
}
