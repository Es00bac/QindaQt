// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_input/controllers_model.h"
#include "qindaqt/controllers/controller_policy.h"
#include <QJsonArray>
#include <QKeySequence>

namespace QindaQt::Apps::SettingsInput {
namespace {
QString label(const QString &id, const QString &family) {
    const bool ps = family == "playstation", nintendo = family == "nintendo";
    if (id == "south") return ps ? "Cross (south)" : nintendo ? "B (south)" : "A (south)";
    if (id == "east") return ps ? "Circle (east)" : nintendo ? "A (east)" : "B (east)";
    if (id == "west") return ps ? "Square (west)" : nintendo ? "Y (west)" : "X (west)";
    if (id == "north") return ps ? "Triangle (north)" : nintendo ? "X (north)" : "Y (north)";
    const QMap<QString, QString> labels{{"back", ps ? "Share / Create" : nintendo ? "Minus" : "View / Back"},
        {"start", ps ? "Options" : nintendo ? "Plus" : "Menu / Start"},
        {"guide", ps ? "PS button" : nintendo ? "Home" : "Xbox / Guide"},
        {"leftstick", "Left stick click"}, {"rightstick", "Right stick click"},
        {"leftshoulder", ps ? "L1" : "Left shoulder"}, {"rightshoulder", ps ? "R1" : "Right shoulder"},
        {"lefttrigger", ps ? "L2" : "Left trigger"}, {"righttrigger", ps ? "R2" : "Right trigger"},
        {"dpup", "D-pad up"}, {"dpdown", "D-pad down"}, {"dpleft", "D-pad left"}, {"dpright", "D-pad right"},
        {"misc1", ps ? "Microphone / extra button" : nintendo ? "Capture" : "Share / extra button"},
        {"touchpad", "Touchpad click"}, {"rightpaddle1", "Right paddle 1"}, {"leftpaddle1", "Left paddle 1"},
        {"rightpaddle2", "Right paddle 2"}, {"leftpaddle2", "Left paddle 2"}};
    return labels.value(id, id);
}
}
ControllersModel::ControllersModel(ControllerPort &port, QObject *parent) : QObject(parent), m_port(port) {
    connect(&port, &ControllerPort::snapshotReceived, this, &ControllersModel::receive);
    connect(&port, &ControllerPort::unavailable, this, [this] {
        m_available = false; m_busy = false; m_rows = {}; m_revision = 0;
        m_error = tr("Controller settings cannot reach the desktop yet."); Q_EMIT viewChanged();
    });
    connect(&port, &ControllerPort::completed, this, [this](bool ok, const QString &reason) {
        m_busy = false;
        m_error = ok ? QString{} : reason == "revision-stale" ? tr("The controller changed. Try the edit again.")
            : reason == "config-unsaved" ? tr("The controller settings could not be saved.")
            : tr("The controller change could not be applied.");
        Q_EMIT viewChanged();
    });
}
void ControllersModel::receive(const QJsonObject &snapshot) {
    bool validRevision = false;
    const auto revision = snapshot.value("revision").toString().toULongLong(&validRevision);
    if (snapshot.value("schemaVersion").toInt() != 1 || !validRevision || revision == 0
        || !snapshot.value("controllers").isArray() || snapshot.value("controllers").toArray().size() > 64) {
        m_available = false; m_error = tr("The desktop returned invalid controller settings."); Q_EMIT viewChanged(); return;
    }
    auto rows = snapshot.value("controllers").toArray();
    QSet<QString> ids;
    for (const auto &entry : rows) {
        const auto row = entry.toObject();
        const auto id = row.value("id").toString();
        Controllers::Profile profile = Controllers::defaultProfile(); QString reason;
        if (id.isEmpty() || id.size() > 160 || ids.contains(id) || row.value("name").toString().size() > 256
            || !row.value("config").isObject() || !Controllers::applyPatch(row.value("config").toObject(), profile, reason)) {
            m_available = false; m_error = tr("The desktop returned invalid controller settings."); Q_EMIT viewChanged(); return;
        }
        ids.insert(id);
    }
    const bool wasConnected = selected().value("connected").toBool();
    bool selectedConnected = false;
    QString connectedId;
    for (qsizetype i = 0; i < rows.size(); ++i) {
        auto row = rows[i].toObject();
        const bool connected = row.value("connected").toBool();
        if (connected && connectedId.isEmpty()) connectedId = row.value("id").toString();
        if (row.value("id").toString() == m_selected) selectedConnected = connected;
        row["displayName"] = row.value("name").toString() + (row.value("template").toBool() ? QString{}
            : connected ? tr(" (connected)") : tr(" (disconnected)"));
        rows[i] = row;
    }
    if (wasConnected && !selectedConnected) m_autoSelect = true;
    if (m_autoSelect && !connectedId.isEmpty()) m_selected = connectedId;
    m_rows = rows; m_revision = revision; m_available = true; m_steam = snapshot.value("steam").toBool();
    if (!ids.contains(m_selected)) m_selected = rows.isEmpty() ? QString{} : rows[0].toObject().value("id").toString();
    if (!m_busy) m_error.clear();
    if (!snapshot.value("error").toString().isEmpty()) m_error = tr("The controller backend needs attention.");
    Q_EMIT viewChanged();
}
QVariantList ControllersModel::controllers() const { return m_rows.toVariantList(); }
QVariantMap ControllersModel::selected() const {
    for (const auto &row : m_rows) if (row.toObject().value("id").toString() == m_selected) return row.toObject().toVariantMap();
    return {};
}
QVariantList ControllersModel::buttons() const {
    const auto row = selected(); const auto family = row.value("family").toString();
    QStringList ids;
    const auto actual = row.value("buttons").toList();
    if (!row.value("template").toBool() && !actual.isEmpty())
        for (const auto &button : actual) ids.append(button.toMap().value("id").toString());
    else {
        ids = Controllers::buttonIds();
        for (const auto &id : QStringList{"misc2", "misc3", "misc4", "misc5", "misc6"}) ids.removeAll(id);
        if (family != "playstation") ids.removeAll("touchpad");
    }
    const auto bindings = row.value("config").toMap().value("bindings").toMap();
    QVariantList result;
    for (const auto &id : ids) {
        const auto binding = bindings.value(id).toMap();
        result.append(QVariantMap{{"id", id}, {"label", label(id, family)},
            {"action", binding.value("action", "none")}, {"shortcut", binding.value("shortcut")}});
    }
    return result;
}
QVariantList ControllersModel::actions() const {
    const QMap<QString, QString> labels{{"none", tr("No desktop action")}, {"dictate", tr("Hold to dictate")},
        {"left-click", tr("Left mouse button")}, {"right-click", tr("Right mouse button")}, {"middle-click", tr("Middle mouse button")},
        {"accept", tr("Enter / accept")}, {"back", tr("Escape / back")}, {"up", tr("Up arrow")}, {"down", tr("Down arrow")},
        {"left", tr("Left arrow")}, {"right", tr("Right arrow")}, {"overview", tr("Window overview")},
        {"launcher", tr("Application launcher")}, {"next-window", tr("Next window")}, {"previous-window", tr("Previous window")},
        {"maximize", tr("Maximize / restore window")}, {"close-window", tr("Close window")},
        {"desktop-left", tr("Previous workspace")}, {"desktop-right", tr("Next workspace")},
        {"show-desktop", tr("Show desktop")}, {"screenshot", tr("Screenshot")}, {"shortcut", tr("Keyboard shortcut…")}};
    QVariantList result;
    for (const auto &id : Controllers::actionIds()) result.append(QVariantMap{{"value", id}, {"label", labels.value(id)}});
    return result;
}
QString ControllersModel::statusText() const {
    if (!m_available) return tr("Controller desktop controls are unavailable.");
    if (m_steam) return tr("Steam has control. Desktop controller input is paused.");
    const auto row = selected(); const auto reason = row.value("reason").toString();
    if (row.value("template").toBool()) return tr("Defaults apply when a new controller of this family connects.");
    if (reason == "game") return tr("A game or another app is using this controller. Desktop input is paused.");
    if (reason == "fullscreen") return tr("Desktop controller input is paused while an app is fullscreen.");
    if (reason == "release-controls") return tr("Release the buttons and center the sticks to resume desktop control.");
    if (reason == "disabled") return tr("Desktop control is disabled for this controller.");
    if (reason == "active") return tr("Ready for desktop control. Hold the assigned dictation button to speak.");
    return row.value("connected").toBool() ? tr("Desktop input is paused.") : tr("Connect this controller to use its bindings.");
}
void ControllersModel::refresh() { m_port.refresh(); }
void ControllersModel::select(const QString &id) {
    for (const auto &row : m_rows) if (row.toObject().value("id").toString() == id) { m_selected = id; m_autoSelect = false; Q_EMIT viewChanged(); return; }
}
bool ControllersModel::submit(const QJsonObject &patch) {
    if (!m_available || m_busy || m_selected.isEmpty()) return false;
    auto profile = Controllers::defaultProfile(); QString reason;
    if (!Controllers::applyPatch(patch, profile, reason)) return false;
    m_busy = true; m_error.clear(); Q_EMIT viewChanged(); m_port.apply(m_selected, patch, m_revision); return true;
}
bool ControllersModel::setOption(const QString &key, const QVariant &value) { return submit({{key, QJsonValue::fromVariant(value)}}); }
bool ControllersModel::setBinding(const QString &button, const QString &action) {
    return submit({{"bindings", QJsonObject{{button, QJsonObject{{"action", action}}}}}});
}
bool ControllersModel::setShortcut(const QString &button, int key) {
    return submit({{"bindings", QJsonObject{{button, QJsonObject{{"action", "shortcut"}, {"shortcut", QKeySequence(key).toString(QKeySequence::PortableText)}}}}}});
}
void ControllersModel::reset() { if (!m_available || m_busy) return; m_busy = true; Q_EMIT viewChanged(); m_port.reset(m_selected, m_revision); }
QString ControllersModel::sequenceText(int key) const { return QKeySequence(key).toString(QKeySequence::NativeText); }
int ControllersModel::sequenceKey(const QString &text) const { const auto seq = QKeySequence::fromString(text, QKeySequence::PortableText); return seq.isEmpty() ? 0 : seq[0].toCombined(); }
} // namespace QindaQt::Apps::SettingsInput
