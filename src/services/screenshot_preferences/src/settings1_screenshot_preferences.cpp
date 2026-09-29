// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/screenshot_preferences/settings1_screenshot_preferences.h>

#include <qindaqt/services/screenshot_preferences/file_name_pattern.h>

#include <QCoreApplication>
#include <QDateTime>

namespace QindaQt::Services::ScreenshotPreferences {
namespace {

// AGENT-CONTRACT: these names, types, defaults and allowed values mirror
// data/settings/schema-v2.json; tst_screenshot_preferences reads the schema
// and fails when the two drift.
constexpr auto kFolderKey = "services.screenshotFolder";
constexpr auto kPatternKey = "services.screenshotFileNamePattern";
constexpr auto kModeKey = "services.screenshotDefaultMode";
constexpr auto kDelayKey = "services.screenshotDelaySeconds";
constexpr auto kShowResultKey = "services.screenshotShowResult";
constexpr auto kRecordFinishKey = "services.screenshotRecordFinish";
constexpr int kMaxDelaySeconds = 60;
constexpr int kReadbackPollMilliseconds = 100;
constexpr int kReadbackDeadlineMilliseconds = 3'000;

QString stringIn(const QVariant &value, const QStringList &allowed, const QString &fallback)
{
    const QString text = value.typeId() == QMetaType::QString ? value.toString() : QString();
    return allowed.contains(text) ? text : fallback;
}

} // namespace

const QStringList &Settings1ScreenshotPreferences::scopedKeys()
{
    static const QStringList keys{QLatin1String(kFolderKey),       QLatin1String(kPatternKey),
                                  QLatin1String(kModeKey),         QLatin1String(kDelayKey),
                                  QLatin1String(kShowResultKey),   QLatin1String(kRecordFinishKey)};
    return keys;
}

const QStringList &Settings1ScreenshotPreferences::modeIds()
{
    static const QStringList ids{QStringLiteral("region"), QStringLiteral("all-screens"),
                                 QStringLiteral("current-screen"), QStringLiteral("active-window"),
                                 QStringLiteral("window-under-pointer")};
    return ids;
}

const QStringList &Settings1ScreenshotPreferences::recordFinishIds()
{
    static const QStringList ids{QStringLiteral("notify"), QStringLiteral("show-in-folder"),
                                 QStringLiteral("quiet")};
    return ids;
}

Settings1ScreenshotPreferences::Settings1ScreenshotPreferences(SettingsClient::SettingsClient &client,
                                                               QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_pattern(QString::fromLatin1(kDefaultFileNamePattern))
    , m_mode(modeIds().constFirst())
    , m_recordFinish(recordFinishIds().constFirst())
{
    connect(&m_client, &SettingsClient::SettingsClient::snapshotChanged, this,
            &Settings1ScreenshotPreferences::onSnapshotChanged);
    connect(&m_client, &SettingsClient::SettingsClient::stateChanged, this,
            &Settings1ScreenshotPreferences::onStateChanged);
    connect(&m_client, &SettingsClient::SettingsClient::commitFinished, this,
            &Settings1ScreenshotPreferences::onCommitFinished);
    connect(&m_client, &SettingsClient::SettingsClient::commitUncertain, this,
            &Settings1ScreenshotPreferences::onCommitUncertain);
    m_readbackTimer.setInterval(kReadbackPollMilliseconds);
    connect(&m_readbackTimer, &QTimer::timeout, this, &Settings1ScreenshotPreferences::onReadbackTick);
    onSnapshotChanged();
}

bool Settings1ScreenshotPreferences::isLoaded() const
{
    const auto &snapshot = m_client.snapshot();
    // A commit's readback briefly re-enters Authenticating for the SAME
    // owner; the last confirmed values stay usable there.
    return m_loaded && m_client.state() != SettingsClient::ClientState::Unavailable
           && m_client.state() != SettingsClient::ClientState::Degraded && snapshot
           && !m_client.currentOwner().isEmpty() && snapshot->owner == m_client.currentOwner();
}

QString Settings1ScreenshotPreferences::effectiveFolder() const
{
    return resolveScreenshotFolder(m_folder);
}

QString Settings1ScreenshotPreferences::fileNamePreview(const QString &pattern) const
{
    if (!isValidFileNamePattern(pattern))
        return {};
    return expandFileNamePattern(pattern, QDateTime::currentDateTime(), m_mode);
}

void Settings1ScreenshotPreferences::onSnapshotChanged()
{
    const auto &snapshot = m_client.snapshot();
    if (!snapshot || snapshot->owner != m_client.currentOwner()
        || m_client.state() != SettingsClient::ClientState::Ready)
        return;
    const QVariantMap &values = snapshot->values;
    const QVariant folder = values.value(QLatin1String(kFolderKey));
    const QVariant pattern = values.value(QLatin1String(kPatternKey));
    const QVariant delay = values.value(QLatin1String(kDelayKey));
    const QVariant showResult = values.value(QLatin1String(kShowResultKey));
    const QString nextFolder = folder.typeId() == QMetaType::QString ? folder.toString() : QString();
    const QString nextPattern = pattern.typeId() == QMetaType::QString
                                        && isValidFileNamePattern(pattern.toString())
                                    ? pattern.toString()
                                    : QString::fromLatin1(kDefaultFileNamePattern);
    const QString nextMode = stringIn(values.value(QLatin1String(kModeKey)), modeIds(),
                                      modeIds().constFirst());
    const int nextDelay = delay.canConvert<int>() && delay.toInt() >= 0
                                  && delay.toInt() <= kMaxDelaySeconds
                              ? delay.toInt()
                              : 0;
    const bool nextShowResult = showResult.typeId() == QMetaType::Bool ? showResult.toBool() : true;
    const QString nextFinish = stringIn(values.value(QLatin1String(kRecordFinishKey)),
                                        recordFinishIds(), recordFinishIds().constFirst());
    const bool changed = !m_loaded || nextFolder != m_folder || nextPattern != m_pattern
                         || nextMode != m_mode || nextDelay != m_delay
                         || nextShowResult != m_showResult || nextFinish != m_recordFinish;
    m_loaded = true;
    m_folder = nextFolder;
    m_pattern = nextPattern;
    m_mode = nextMode;
    m_delay = nextDelay;
    m_showResult = nextShowResult;
    m_recordFinish = nextFinish;
    if (changed)
        Q_EMIT preferencesChanged();
    if (m_awaitingReadback) {
        // AGENT-GUARD: only the original owner/epoch at or beyond the
        // commit's revisionAfter can confirm a write (ADR-0249 lineage).
        if (snapshot->owner != m_pendingOwner || snapshot->epoch != m_pendingEpoch) {
            clearPending();
            setWriteStatus(tr("The change was not confirmed after the settings service changed."));
            return;
        }
        if (snapshot->revision < m_pendingRevisionFloor)
            return;
        const bool matched = snapshot->values.value(m_pendingKey) == m_pendingValue;
        clearPending();
        setWriteStatus(matched ? tr("Saved.")
                               : tr("The saved setting did not match the request. Check its current value."));
    }
}

void Settings1ScreenshotPreferences::onStateChanged()
{
    const bool loaded = isLoaded();
    if (!loaded && m_awaitingReadback) {
        clearPending();
        setWriteStatus(tr("The change was accepted but its saved value was not confirmed. Check the "
                          "current value before retrying."));
    }
    if (!loaded && m_loaded) {
        m_loaded = false;
        Q_EMIT preferencesChanged();
    }
    if (!m_loaded)
        onSnapshotChanged();
}

void Settings1ScreenshotPreferences::onReadbackTick()
{
    if (!m_awaitingReadback)
        return;
    if (m_client.currentOwner() != m_pendingOwner) {
        clearPending();
        setWriteStatus(tr("The change was not confirmed after the settings service changed."));
        if (m_loaded) {
            m_loaded = false;
            Q_EMIT preferencesChanged();
        }
        return;
    }
    if (m_readbackAge.elapsed() >= kReadbackDeadlineMilliseconds) {
        clearPending();
        setWriteStatus(tr("The change was accepted but its saved value was not confirmed. Check the "
                          "current value before retrying."));
        return;
    }
    // Refetch only; never replay the write.
    if (m_client.state() == SettingsClient::ClientState::Ready)
        m_client.refresh();
}

void Settings1ScreenshotPreferences::onCommitFinished(const SettingsClient::CommitOutcome &outcome)
{
    if (m_pendingKey.isEmpty())
        return;
    if (outcome.status == SettingsProtocol::SettingsWireStatus::Applied) {
        const auto &baseline = m_client.snapshot();
        if (!baseline || baseline->owner != m_pendingOwner || baseline->epoch != m_pendingEpoch) {
            clearPending();
            setWriteStatus(tr("The change was accepted but its settings service lineage was not "
                              "confirmed."));
            return;
        }
        m_pendingRevisionFloor = outcome.revisionAfter;
        m_awaitingReadback = true;
        m_readbackAge.start();
        m_readbackTimer.start();
        setWriteStatus(tr("Checking the saved setting…"));
        return;
    }
    clearPending();
    setWriteStatus(outcome.message.isEmpty() ? tr("Settings rejected the change.") : outcome.message);
}

void Settings1ScreenshotPreferences::onCommitUncertain(const QString &message)
{
    if (m_pendingKey.isEmpty())
        return;
    clearPending();
    setWriteStatus(message.isEmpty()
                       ? tr("The change was not confirmed. Check the current value before retrying.")
                       : tr("The change was not confirmed: %1").arg(message));
}

bool Settings1ScreenshotPreferences::request(const QString &key, const QVariant &value)
{
    if (!isLoaded() || !m_pendingKey.isEmpty()) {
        setWriteStatus(tr("Settings are unavailable or another change is pending."));
        return false;
    }
    QString error;
    if (!m_client.setUserValue(key, value, &error)) {
        setWriteStatus(error.isEmpty() ? tr("Settings could not accept the change.") : error);
        return false;
    }
    m_pendingKey = key;
    m_pendingValue = value;
    const auto &baseline = m_client.snapshot();
    m_pendingOwner = m_client.currentOwner();
    m_pendingEpoch = baseline ? baseline->epoch : QString();
    m_pendingRevisionFloor = 0;
    m_awaitingReadback = false;
    setWriteStatus(tr("Saving setting…"));
    return true;
}

bool Settings1ScreenshotPreferences::setFolder(const QString &folder)
{
    const QString trimmed = folder.trimmed();
    if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1Char('/')) && !trimmed.startsWith(QLatin1Char('~'))) {
        setWriteStatus(tr("Choose a folder by its full path."));
        return false;
    }
    return request(QLatin1String(kFolderKey), trimmed);
}

bool Settings1ScreenshotPreferences::setFileNamePattern(const QString &pattern)
{
    if (!isValidFileNamePattern(pattern)) {
        setWriteStatus(tr("A file name cannot be empty or contain “/”."));
        return false;
    }
    return request(QLatin1String(kPatternKey), pattern);
}

bool Settings1ScreenshotPreferences::setDefaultMode(const QString &modeId)
{
    if (!modeIds().contains(modeId))
        return false;
    return request(QLatin1String(kModeKey), modeId);
}

bool Settings1ScreenshotPreferences::setDelaySeconds(int seconds)
{
    if (seconds < 0 || seconds > kMaxDelaySeconds) {
        setWriteStatus(tr("The delay must be between 0 and 60 seconds."));
        return false;
    }
    return request(QLatin1String(kDelayKey), seconds);
}

bool Settings1ScreenshotPreferences::setShowResultWindow(bool show)
{
    return request(QLatin1String(kShowResultKey), show);
}

bool Settings1ScreenshotPreferences::setRecordFinish(const QString &finish)
{
    if (!recordFinishIds().contains(finish))
        return false;
    return request(QLatin1String(kRecordFinishKey), finish);
}

void Settings1ScreenshotPreferences::setWriteStatus(const QString &status)
{
    if (m_writeStatus == status)
        return;
    m_writeStatus = status;
    Q_EMIT writeStatusChanged();
}

void Settings1ScreenshotPreferences::clearPending()
{
    const bool wasPending = !m_pendingKey.isEmpty();
    m_readbackTimer.stop();
    m_awaitingReadback = false;
    m_pendingKey.clear();
    m_pendingValue.clear();
    m_pendingOwner.clear();
    m_pendingEpoch.clear();
    m_pendingRevisionFloor = 0;
    if (wasPending)
        Q_EMIT writeStatusChanged();
}

} // namespace QindaQt::Services::ScreenshotPreferences
