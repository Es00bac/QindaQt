// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_controller.h"
#include <QTimer>
#include <utility>

namespace QindaQt::Apps::RemovableMedia {
MediaController::MediaController(MediaBackend &backend, MediaPreferences &preferences,
                                 bool watchInsertions, QObject *parent)
    : QObject(parent), m_backend(backend), m_preferences(preferences), m_watchInsertions(watchInsertions)
{
    connect(&backend, &MediaBackend::changed, this, &MediaController::inventoryChanged);
    connect(&backend, &MediaBackend::finished, this, &MediaController::completed);
    m_volumes = backend.volumes();
    // Notification and window adapters attach immediately after construction.
    QTimer::singleShot(0, this, &MediaController::inventoryChanged);
}
const Volume *MediaController::find(const QString &token) const
{
    for (const auto &volume : m_volumes) if (volume.token == token) return &volume;
    return nullptr;
}
QVariantList MediaController::volumes() const
{
    QVariantList rows;
    for (const auto &v : m_volumes) rows.append(volumeMap(v, m_preferences.mode(v.preferenceKey)));
    return rows;
}
QVariantMap MediaController::selected() const
{
    const auto *volume = find(m_selected);
    return volume ? volumeMap(*volume, m_preferences.mode(volume->preferenceKey)) : QVariantMap{};
}
QVariantMap MediaController::formatTarget() const
{
    return m_formatTarget ? volumeMap(*m_formatTarget, QStringLiteral("ask")) : QVariantMap{};
}
QStringList MediaController::formatTypes() const { return m_backend.formatTypes(); }
bool MediaController::available() const { return m_backend.available(); }
bool MediaController::busy() const { return !m_pendingToken.isEmpty(); }
QString MediaController::status() const
{
    return available() ? m_status : m_backend.diagnostic();
}
void MediaController::select(const QString &token)
{
    m_selected = find(token) ? token : QString{};
    Q_EMIT changed();
}
void MediaController::show(const QString &token)
{
    if (!token.isEmpty()) select(token);
    else if (m_selected.isEmpty() && !m_volumes.isEmpty()) select(m_volumes.constFirst().token);
    Q_EMIT windowRequested(m_selected);
}
void MediaController::refresh() { m_backend.refresh(); }

void MediaController::inventoryChanged()
{
    m_volumes = m_backend.volumes();
    QSet<QString> attached;
    for (const auto &v : m_volumes) attached.insert(v.token);
    for (const auto &old : std::as_const(m_seen))
        if (!attached.contains(old)) Q_EMIT notificationWithdrawn(old);
    if (!find(m_selected)) m_selected = m_volumes.isEmpty() ? QString{} : m_volumes.constFirst().token;
    if (m_formatTarget) {
        const auto *current = find(m_formatTarget->token);
        if (!current || current->identity != m_formatTarget->identity) m_formatTarget.reset();
        else m_formatTarget = *current;
    }
    // Publish the complete inventory before signals may synchronously invoke
    // a consumer's action. Capture candidates so a callback cannot invalidate
    // the iterator over our current inventory.
    const auto candidates = m_volumes;
    const auto previous = m_seen;
    m_seen = attached;
    Q_EMIT changed();
    if (!m_watchInsertions) return;
    for (const auto &v : candidates) {
        if (previous.contains(v.token)) continue;
        const QString mode = m_preferences.mode(v.preferenceKey);
        if (mode == QStringLiteral("ignore")) continue;
        if (!v.mountPath.isEmpty()) { prompt(v); continue; }
        if (v.mountable && (mode == QStringLiteral("mount") || mode == QStringLiteral("read-only"))) {
            Request request;
            request.token = v.token;
            request.operation = (mode == QStringLiteral("read-only") || v.readOnly)
                ? Operation::MountReadOnly : Operation::Mount;
            m_automatic.enqueue(request);
        } else prompt(v);
    }
    nextAutomaticMount();
}
void MediaController::nextAutomaticMount()
{
    if (busy() || !available()) return;
    while (!m_automatic.isEmpty()) {
        auto request = m_automatic.dequeue();
        const auto *v = find(request.token);
        if (!v || !v->mountPath.isEmpty()) continue;
        submit(std::move(request));
        break;
    }
}
void MediaController::prompt(const Volume &v)
{
    QStringList actions{QStringLiteral("default"), QStringLiteral("More options")};
    QString body = QStringLiteral("What would you like to do with this media?");
    if (!v.mountPath.isEmpty()) {
        actions.append({QStringLiteral("open"), QStringLiteral("Open"), QStringLiteral("remove"),
                        v.optical ? QStringLiteral("Eject") : QStringLiteral("Safely remove")});
        body = QStringLiteral("Mounted at %1.").arg(v.mountPath);
    } else if (v.mountable) {
        actions.append({QStringLiteral("mount"), QStringLiteral("Mount and open")});
        if (v.canMountReadOnly) actions.append({QStringLiteral("read-only"), QStringLiteral("Mount read-only")});
        if (!v.preferenceKey.isEmpty()) actions.append({QStringLiteral("always"), QStringLiteral("Always mount this media")});
    } else if (v.encrypted) body = QStringLiteral("This volume is encrypted. Open More options to unlock it.");
    else if (v.optical) body = QStringLiteral("This disc has no mountable data filesystem. You can eject it from More options.");
    else body = QStringLiteral("No mountable filesystem was found. Open More options to format this media.");
    Q_EMIT notificationRequested(v.token, v.label, body, actions);
}
void MediaController::submit(Request request, bool openAfter, const QString &rememberMode)
{
    const auto *v = find(request.token);
    if (busy() || !available() || !v) {
        m_status = busy() ? QStringLiteral("Another media operation is still running.")
                         : QStringLiteral("This media is no longer available. Refresh and try again.");
        Q_EMIT changed();
        return;
    }
    m_pendingToken = request.token;
    m_openAfter = openAfter;
    m_rememberMode = rememberMode;
    m_rememberKey = v->preferenceKey;
    m_status = QStringLiteral("Working on %1…").arg(v->label);
    Q_EMIT changed();
    m_backend.execute(request);
}
void MediaController::mount(const QString &token, bool readOnly, bool openAfter)
{
    const auto *v = find(token);
    Request request;
    request.token = token;
    request.operation = readOnly || (v && v->readOnly) ? Operation::MountReadOnly : Operation::Mount;
    submit(request, openAfter);
}
void MediaController::open(const QString &token)
{
    const auto *v = find(token);
    if (!v) return;
    if (v->mountPath.isEmpty()) mount(token, v->readOnly, true);
    else Q_EMIT openPathRequested(v->mountPath);
}
void MediaController::unmount(const QString &token)
{
    Request request;
    request.token = token;
    request.operation = Operation::Unmount;
    submit(request);
}
void MediaController::remove(const QString &token)
{
    Request request;
    request.token = token;
    request.operation = Operation::Remove;
    submit(request);
}
void MediaController::remember(const QString &token, const QString &mode)
{
    const auto *v = find(token);
    QString error;
    if (!v || !m_preferences.save(v->preferenceKey, mode, &error))
        m_status = v ? error : QStringLiteral("This media is no longer available.");
    else m_status = QStringLiteral("Saved the choice for this media.");
    Q_EMIT changed();
}
void MediaController::requestFormat(const QString &token)
{
    const auto *v = find(token);
    if (!busy() && v && v->canFormat && !formatTypes().isEmpty()) {
        m_formatTarget = *v;
    } else m_status = QStringLiteral("Unmount writable media before formatting it.");
    Q_EMIT changed();
}
void MediaController::cancelFormat() { m_formatTarget.reset(); Q_EMIT changed(); }
void MediaController::confirmFormat(const QString &filesystem, const QString &label,
                                    const QString &typedDevice)
{
    if (!m_formatTarget) return;
    const auto target = *m_formatTarget;
    const auto *v = find(target.token);
    if (!v || v->identity != target.identity || !v->canFormat || !v->mountPath.isEmpty()
        || typedDevice != target.device || !formatTypes().contains(filesystem)) {
        m_status = QStringLiteral("The format confirmation no longer matches available, unmounted media.");
        cancelFormat();
        return;
    }
    Request request;
    request.token = target.token;
    request.operation = Operation::Format;
    request.filesystem = filesystem;
    request.label = label;
    m_formatTarget.reset();
    submit(request);
}
void MediaController::unlock(const QString &token, const QString &passphrase)
{
    Request request;
    request.token = token;
    request.operation = Operation::Unlock;
    request.passphrase = passphrase;
    submit(request);
}
void MediaController::notificationAction(const QString &token, const QString &action)
{
    if (!find(token)) return;
    if (action == QStringLiteral("mount")) mount(token, false, true);
    else if (action == QStringLiteral("read-only")) mount(token, true, true);
    else if (action == QStringLiteral("open")) open(token);
    else if (action == QStringLiteral("remove")) remove(token);
    else if (action == QStringLiteral("always")) {
        Request request;
        request.token = token;
        request.operation = find(token)->readOnly ? Operation::MountReadOnly : Operation::Mount;
        submit(request, true, QStringLiteral("mount"));
    } else show(token);
}
void MediaController::completed(const QString &token, bool success, const QString &message,
                                const QString &mountPath)
{
    if (token != m_pendingToken) return;
    const bool openAfter = m_openAfter;
    const QString rememberMode = m_rememberMode;
    const QString rememberKey = m_rememberKey;
    m_pendingToken.clear();
    m_rememberMode.clear();
    m_rememberKey.clear();
    m_openAfter = false;
    m_status = message;
    if (success && !rememberMode.isEmpty()) {
        QString error;
        if (!m_preferences.save(rememberKey, rememberMode, &error)) m_status += QLatin1Char(' ') + error;
    }
    Q_EMIT changed();
    QStringList actions{QStringLiteral("default"), QStringLiteral("More options")};
    const auto *current = find(token);
    if (!success && current && current->mountable && current->mountPath.isEmpty() && current->canMountReadOnly)
        actions.append({QStringLiteral("read-only"), QStringLiteral("Try read-only")});
    Q_EMIT notificationRequested(token, success ? QStringLiteral("Media ready") : QStringLiteral("Media operation failed"),
                                 m_status, actions);
    if (success && openAfter && find(token) && !mountPath.isEmpty()) Q_EMIT openPathRequested(mountPath);
    QTimer::singleShot(0, this, &MediaController::nextAutomaticMount);
}
} // namespace QindaQt::Apps::RemovableMedia
