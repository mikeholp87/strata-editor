#include "shortcuts.h"
#include <QAction>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <iterator>

namespace {
constexpr auto settingsKey = "shortcuts/v1";
constexpr qint64 maxBytes = 256 * 1024;
bool usable(const QKeySequence &sequence) {
    for (int i = 0; i < sequence.count(); ++i) {
        const auto key = sequence[i].key();
        const auto modifiers = sequence[i].keyboardModifiers();
        if (key == Qt::Key_unknown || key == 0 || key == Qt::Key_Control || key == Qt::Key_Shift ||
            key == Qt::Key_Meta || key == Qt::Key_Alt)
            return false;
        if (!(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) &&
            !(key >= Qt::Key_F1 && key <= Qt::Key_F35))
            return false;
    }
    return true;
}
void updateActions(const QList<QAction *> &actions, const Shortcuts::Mapping &map) {
    for (auto action : actions)
        action->setShortcut(map.value(action->objectName()));
}
} // namespace
Shortcuts::Mapping Shortcuts::defaults(const QList<QAction *> &actions) {
    Mapping map;
    for (auto action : actions)
        map.insert(action->objectName(), action->property("defaultShortcut").isValid()
                                             ? action->property("defaultShortcut").value<QKeySequence>()
                                             : action->shortcut());
    return map;
}
Shortcuts::Mapping Shortcuts::current(const QList<QAction *> &actions) {
    Mapping map;
    for (auto action : actions)
        map.insert(action->objectName(), action->shortcut());
    return map;
}
bool Shortcuts::validate(const QList<QAction *> &actions, const Mapping &map, QString *error) {
    const auto known = defaults(actions);
    if (known.size() != actions.size() || known.contains(QString()) || map.keys() != known.keys()) {
        *error = QObject::tr("Shortcut commands do not match the registered application commands.");
        return false;
    }
    for (auto i = map.cbegin(); i != map.cend(); ++i) {
        if (!usable(i.value())) {
            *error =
                QObject::tr("%1: use Ctrl, Alt, Meta, or a function key; ordinary typing keys are reserved.")
                    .arg(i.key());
            return false;
        }
        if (i.value().isEmpty())
            continue;
        for (auto j = std::next(i); j != map.cend(); ++j) {
            if (j.value().isEmpty())
                continue;
            if (i.value().matches(j.value()) != QKeySequence::NoMatch ||
                j.value().matches(i.value()) != QKeySequence::NoMatch) {
                *error = QObject::tr("Conflicting shortcuts: %1 (%2) and %3 (%4). Clear or change one, "
                                     "including any chord prefix.")
                             .arg(i.key(), i.value().toString(QKeySequence::NativeText), j.key(),
                                  j.value().toString(QKeySequence::NativeText));
                return false;
            }
        }
    }
    error->clear();
    return true;
}
QByteArray Shortcuts::encode(const Mapping &map) {
    QJsonObject bindings;
    for (auto i = map.cbegin(); i != map.cend(); ++i)
        bindings.insert(i.key(), i.value().toString(QKeySequence::PortableText));
    return QJsonDocument(QJsonObject{{"format", "strata-shortcuts"}, {"version", 1}, {"bindings", bindings}})
        .toJson();
}
bool Shortcuts::decode(const QByteArray &bytes, const QList<QAction *> &actions, Mapping *map,
                       QString *error) {
    if (bytes.size() > maxBytes) {
        *error = QObject::tr("Shortcut configuration exceeds 256 KiB.");
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    const auto root = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        root["format"] != "strata-shortcuts" || root["version"] != QJsonValue(1) ||
        !root["bindings"].isObject()) {
        *error = QObject::tr("Expected a Strata shortcuts JSON file with version 1 and a bindings object.");
        return false;
    }
    auto draft = defaults(actions);
    const auto bindings = root["bindings"].toObject();
    for (auto i = bindings.begin(); i != bindings.end(); ++i) {
        if (!draft.contains(i.key()) || !i.value().isString()) {
            *error = QObject::tr("Unknown command or invalid shortcut value: %1").arg(i.key());
            return false;
        }
        const auto text = i.value().toString();
        const auto key = QKeySequence::fromString(text, QKeySequence::PortableText);
        if (key.toString(QKeySequence::PortableText) != text || !usable(key)) {
            *error = QObject::tr("Invalid portable shortcut for %1. Example: Ctrl+Alt+N").arg(i.key());
            return false;
        }
        draft[i.key()] = key;
    }
    if (!validate(actions, draft, error))
        return false;
    *map = draft;
    return true;
}
bool Shortcuts::importFile(const QString &path, const QList<QAction *> &actions, Mapping *map,
                           QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return false;
    }
    const auto bytes = file.read(maxBytes + 1);
    if (file.error() != QFileDevice::NoError) {
        *error = file.errorString();
        return false;
    }
    return decode(bytes, actions, map, error);
}
bool Shortcuts::exportFile(const QString &path, const QList<QAction *> &actions, const Mapping &map,
                           QString *error) {
    if (!validate(actions, map, error))
        return false;
    const auto bytes = encode(map);
    if (bytes.size() > maxBytes) {
        *error = QObject::tr("Shortcut configuration exceeds 256 KiB.");
        return false;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}
bool Shortcuts::apply(QSettings &settings, const QList<QAction *> &actions, const Mapping &map,
                      QString *error) {
    if (!validate(actions, map, error))
        return false;
    const auto old = settings.value(settingsKey);
    settings.setValue(settingsKey, encode(map));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (old.isValid())
            settings.setValue(settingsKey, old);
        else
            settings.remove(settingsKey);
        *error = QObject::tr("Could not save shortcuts. Active mappings have not changed.");
        return false;
    }
    updateActions(actions, map);
    return true;
}
bool Shortcuts::load(QSettings &settings, const QList<QAction *> &actions, QString *error) {
    if (!settings.contains(settingsKey))
        return true;
    Mapping map;
    if (!decode(settings.value(settingsKey).toByteArray(), actions, &map, error))
        return false;
    updateActions(actions, map);
    return true;
}
