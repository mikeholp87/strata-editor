#include "core/persistence.h"
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
    if (!settings.value("recovery", true).toBool())
        return;
    QJsonArray buffers;
    for (int i = 0; i < tabs->count(); ++i) {
        auto d = qobject_cast<DocumentView *>(tabs->widget(i))->model;
        if (d->text()->isModified())
            buffers.append(d->snapshot());
    }
    if (!Persistence::write(Persistence::dataPath("recovery.json"), {{"buffers", buffers}}))
        statusBar()->showMessage(tr("Could not write recovery snapshot. Save your documents."), 10000);
}
void MainWindow::saveSession() {
    QJsonArray documents;
    for (int i = 0; i < tabs->count(); ++i) {
        auto v = qobject_cast<DocumentView *>(tabs->widget(i));
        if (!v->model->path().isEmpty())
            documents.append(QJsonObject{{"path", v->model->path()}, {"view", v->viewState()}});
    }
    Persistence::write(Persistence::dataPath("session.json"),
                       {{"documents", documents},
                        {"active", current() ? current()->model->path() : ""},
                        {"project", project}});
    settings.setValue("geometry", saveGeometry());
    settings.setValue("layout", saveState());
    settings.sync();
}
void MainWindow::restoreSession() {
    auto recovery = Persistence::read(Persistence::dataPath("recovery.json"))["buffers"].toArray();
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
    auto session = Persistence::read(Persistence::dataPath("session.json"));
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
