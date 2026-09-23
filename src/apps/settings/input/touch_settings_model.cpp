// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/touch_settings_model.h>

#include <QVariantMap>
#include <algorithm>

namespace QindaQt::Apps::SettingsInput {
namespace {

constexpr auto EnabledKey = "input.touch.enabled";
constexpr auto LongPressKey = "input.touch.longPressMs";
constexpr auto KeyboardKey = "input.touch.onScreenKeyboard";
constexpr int MinimumLongPressMs = 200;
constexpr int MaximumLongPressMs = 1500;

QString edgeKey(const QString &edge)
{
    if (edge == QLatin1String("left")) return QStringLiteral("input.touch.edgeLeft");
    if (edge == QLatin1String("top")) return QStringLiteral("input.touch.edgeTop");
    if (edge == QLatin1String("right")) return QStringLiteral("input.touch.edgeRight");
    if (edge == QLatin1String("bottom")) return QStringLiteral("input.touch.edgeBottom");
    return QString();
}

QString edgeDefault(const QString &edge)
{
    if (edge == QLatin1String("left")) return QStringLiteral("overview");
    if (edge == QLatin1String("top")) return QStringLiteral("notifications");
    if (edge == QLatin1String("bottom")) return QStringLiteral("task-switcher");
    return QStringLiteral("none");
}

// The keyboard has no "always": the compositor's panel gate is its own.
const QStringList &keyboardModes()
{
    static const QStringList values{QStringLiteral("auto"), QStringLiteral("off")};
    return values;
}

const QStringList &edgeActions()
{
    static const QStringList values{QStringLiteral("none"), QStringLiteral("overview"),
                                    QStringLiteral("notifications"), QStringLiteral("task-switcher")};
    return values;
}

} // namespace

TouchSettingsModel::TouchSettingsModel(Services::SettingsClient::SettingsClient &client, QObject *parent)
    : QObject(parent), m_client(client)
{
    connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &TouchSettingsModel::handleSnapshot);
    connect(&m_client, &Services::SettingsClient::SettingsClient::stateChanged,
            this, &TouchSettingsModel::handleAuthorityChange);
    connect(&m_client, &Services::SettingsClient::SettingsClient::commitFinished, this,
            [this](const Services::SettingsClient::CommitOutcome &outcome) {
                if (!m_busy || m_awaitingSnapshot) return;
                if (outcome.status != Services::SettingsProtocol::SettingsWireStatus::Applied) {
                    abandonWrite(tr("The touch preference could not be applied: %1")
                                     .arg(outcome.message.isEmpty() ? tr("request refused") : outcome.message));
                    return;
                }
                // AGENT-GUARD: SettingsClient emits this before it rereads authority.
                // A cached pre-reply snapshot cannot be the base for queued input.
                m_awaitingSnapshot = true;
                m_resultRevision = outcome.revisionAfter;
                publishStatus();
            });
    connect(&m_client, &Services::SettingsClient::SettingsClient::commitUncertain, this,
            [this](const QString &message) {
                if (!m_busy) return;
                abandonWrite(tr("The touch preference may not have been applied: %1")
                                 .arg(message.isEmpty() ? tr("unknown reason") : message));
            });
    publishStatus();
}

TouchSettingsModel::~TouchSettingsModel() = default;

QStringList TouchSettingsModel::settingsKeys()
{
    // AGENT-GUARD (ADR-0205 §4): input.touch.mode has no consumer yet, so the
    // page neither scopes nor offers it; first-party applications keep the
    // automatic touch-sizing rule. Offering "always"/"never" would be a lie.
    QStringList keys{QString::fromLatin1(EnabledKey), QString::fromLatin1(LongPressKey),
                     QString::fromLatin1(KeyboardKey)};
    for (const QString &edge : edgeNames()) {
        keys.append(edgeKey(edge));
    }
    return keys;
}

QStringList TouchSettingsModel::edgeNames()
{
    return {QStringLiteral("left"), QStringLiteral("top"), QStringLiteral("right"), QStringLiteral("bottom")};
}

QVariant TouchSettingsModel::value(const QString &key) const
{
    const auto &snapshot = m_client.snapshot();
    return snapshot ? snapshot->values.value(key) : QVariant();
}

bool TouchSettingsModel::available() const
{
    const auto &snapshot = m_client.snapshot();
    return m_client.state() == Services::SettingsClient::ClientState::Ready
           && snapshot && snapshot->owner == m_client.currentOwner();
}

bool TouchSettingsModel::hasLastKnown() const
{
    return m_client.snapshot().has_value();
}

bool TouchSettingsModel::editable() const
{
    return available() && !m_busy && !m_client.writeInFlight();
}

bool TouchSettingsModel::longPressQueueable() const
{
    return available() && m_busy && !m_awaitingSnapshot
           && m_writeKey == QLatin1String(LongPressKey);
}

bool TouchSettingsModel::touchscreenEnabled() const
{
    const QVariant v = value(QString::fromLatin1(EnabledKey));
    return v.isValid() ? v.toBool() : true;
}

int TouchSettingsModel::longPressMs() const
{
    const QVariant v = value(QString::fromLatin1(LongPressKey));
    bool ok = false;
    const int milliseconds = v.toInt(&ok);
    return ok ? std::clamp(milliseconds, MinimumLongPressMs, MaximumLongPressMs) : 500;
}

int TouchSettingsModel::longPressDisplayMs() const
{
    if (m_busy && m_writeKey == QLatin1String(LongPressKey)) {
        return m_queuedLongPress.value_or(m_writeValue.toInt());
    }
    return longPressMs();
}

QString TouchSettingsModel::onScreenKeyboard() const
{
    const QString v = value(QString::fromLatin1(KeyboardKey)).toString();
    return keyboardModes().contains(v) ? v : QStringLiteral("auto");
}

QString TouchSettingsModel::edgeAction(const QString &edge) const
{
    const QString v = value(edgeKey(edge)).toString();
    return edgeActions().contains(v) ? v : edgeDefault(edge);
}

QVariantList TouchSettingsModel::keyboardChoices() const
{
    return {QVariantMap{{QStringLiteral("value"), QStringLiteral("auto")},
                        {QStringLiteral("label"), tr("When a finger focuses a field")}},
            QVariantMap{{QStringLiteral("value"), QStringLiteral("off")}, {QStringLiteral("label"), tr("Never")}}};
}

QVariantList TouchSettingsModel::edgeActionChoices() const
{
    return {QVariantMap{{QStringLiteral("value"), QStringLiteral("none")}, {QStringLiteral("label"), tr("Nothing")}},
            QVariantMap{{QStringLiteral("value"), QStringLiteral("overview")}, {QStringLiteral("label"), tr("Overview")}},
            QVariantMap{{QStringLiteral("value"), QStringLiteral("notifications")},
                        {QStringLiteral("label"), tr("Notification center")}},
            QVariantMap{{QStringLiteral("value"), QStringLiteral("task-switcher")},
                        {QStringLiteral("label"), tr("Task switcher")}}};
}

int TouchSettingsModel::choiceIndex(const QVariantList &choices, const QString &value) const
{
    for (qsizetype index = 0; index < choices.size(); ++index) {
        if (choices.at(index).toMap().value(QStringLiteral("value")).toString() == value) {
            return static_cast<int>(index);
        }
    }
    return 0;
}

void TouchSettingsModel::refresh()
{
    if (!m_started) {
        QString error;
        m_started = m_client.start(&error);
        if (!m_started) {
            m_errorText = error.isEmpty() ? tr("Touch preferences are unavailable right now.") : error;
        }
    }
    if (m_started) {
        m_client.refresh();
    }
    publishStatus();
}

bool TouchSettingsModel::setTouchscreenEnabled(bool enabled)
{
    return submit(QString::fromLatin1(EnabledKey), enabled);
}

bool TouchSettingsModel::setLongPressMs(int milliseconds)
{
    if (milliseconds < MinimumLongPressMs || milliseconds > MaximumLongPressMs) {
        m_errorText = tr("Choose a hold time between %1 and %2 ms.").arg(MinimumLongPressMs).arg(MaximumLongPressMs);
        Q_EMIT changed();
        return false;
    }
    if (longPressQueueable()) {
        // A gesture can replace its own in-flight final value, but it never
        // creates a second write until the first is confirmed by a fresh read.
        m_queuedLongPress = milliseconds;
        publishStatus();
        return true;
    }
    return submit(QString::fromLatin1(LongPressKey), static_cast<qint64>(milliseconds));
}

bool TouchSettingsModel::setOnScreenKeyboard(const QString &mode)
{
    if (!keyboardModes().contains(mode)) {
        return false;
    }
    return submit(QString::fromLatin1(KeyboardKey), mode);
}

bool TouchSettingsModel::setEdgeAction(const QString &edge, const QString &action)
{
    if (edgeKey(edge).isEmpty() || !edgeActions().contains(action)) {
        return false;
    }
    return submit(edgeKey(edge), action);
}

bool TouchSettingsModel::submit(const QString &key, const QVariant &value)
{
    if (!editable()) {
        if (m_errorText.isEmpty() && !m_busy) {
            m_errorText = tr("Touch preferences are unavailable or still refreshing.");
            publishStatus();
        }
        return false;
    }
    const auto &snapshot = *m_client.snapshot();
    QString error;
    if (!m_client.setUserValue(key, value, &error)) {
        m_errorText = error.isEmpty() ? tr("Could not save the touch preference.") : error;
        publishStatus();
        return false;
    }
    m_busy = true;
    m_awaitingSnapshot = false;
    m_writeKey = key;
    m_writeOwner = snapshot.owner;
    m_writeEpoch = snapshot.epoch;
    m_writeValue = value;
    m_resultRevision = 0;
    m_queuedLongPress.reset();
    m_errorText.clear();
    publishStatus();
    return true;
}

void TouchSettingsModel::abandonWrite(const QString &message)
{
    m_busy = false;
    m_awaitingSnapshot = false;
    m_queuedLongPress.reset();
    m_writeKey.clear();
    m_writeOwner.clear();
    m_writeEpoch.clear();
    m_writeValue.clear();
    m_resultRevision = 0;
    m_errorText = message;
    publishStatus();
}

void TouchSettingsModel::handleAuthorityChange()
{
    if (m_busy && (m_client.currentOwner() != m_writeOwner
                   || m_client.state() == Services::SettingsClient::ClientState::Unavailable
                   || m_client.state() == Services::SettingsClient::ClientState::Degraded)) {
        abandonWrite(tr("The touch preference may not have been applied because settings authority changed."));
        return;
    }
    publishStatus();
}

void TouchSettingsModel::handleSnapshot()
{
    if (!m_busy || !m_awaitingSnapshot) {
        // An unrelated refresh must not erase a refusal or finish a write.
        publishStatus();
        return;
    }
    const auto &snapshot = m_client.snapshot();
    if (!available() || !snapshot || snapshot->owner != m_writeOwner
        || snapshot->epoch != m_writeEpoch) {
        abandonWrite(tr("The touch preference may not have been applied because settings authority changed."));
        return;
    }
    if (snapshot->revision < m_resultRevision || snapshot->values.value(m_writeKey) != m_writeValue) {
        abandonWrite(tr("The touch preference could not be confirmed by the refreshed settings."));
        return;
    }
    const auto queued = m_queuedLongPress;
    m_busy = false;
    m_awaitingSnapshot = false;
    m_queuedLongPress.reset();
    m_writeKey.clear();
    m_writeOwner.clear();
    m_writeEpoch.clear();
    m_writeValue.clear();
    m_resultRevision = 0;
    m_errorText.clear();
    publishStatus();
    if (queued && *queued != longPressMs()) {
        // This call uses the just-received authoritative revision, not the
        // commit reply or a snapshot retained from before completion.
        (void)submit(QString::fromLatin1(LongPressKey), static_cast<qint64>(*queued));
    }
}

void TouchSettingsModel::publishStatus()
{
    if (m_busy && m_writeKey == QLatin1String(LongPressKey)) {
        m_statusText = tr("Saving hold time; last confirmed value is %1 ms.").arg(longPressMs());
    } else if (m_busy) {
        m_statusText = tr("Confirming the touch preference.");
    } else if (!hasLastKnown()) {
        m_statusText = tr("Touch preferences are not available yet.");
    } else if (!available()) {
        m_statusText = tr("Touch preferences are unavailable; showing last-known values.");
    } else if (!touchscreenEnabled()) {
        m_statusText = tr("The touchscreen is off.");
    } else {
        m_statusText = tr("A finger held for %1 ms opens the menu.").arg(longPressMs());
    }
    Q_EMIT changed();
}

} // namespace QindaQt::Apps::SettingsInput
