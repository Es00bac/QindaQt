// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/settings_client/settings_client.h>

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariant>

namespace QindaQt::Services::ScreenshotPreferences {

// The screenshot tool's preferences, as confirmed Settings1 values.
//
// AGENT-CONTRACT (ADR-0289): six `services.screenshot*` keys defined in
// data/settings/schema-v2.json. Readers (the tool) get schema defaults until
// an exact-owner snapshot is confirmed; writers (Settings → Streaming's
// "Screenshots and recording" section) follow the Settings1StreamingPreferences
// write contract: one write in flight, no optimistic publication, a write is
// "Saved." only after readback at or beyond the commit's revision, and an
// uncertain write is never replayed. A stored value outside its allowed set
// reads as the default instead of propagating.
//
// Borrows a purpose-scoped SettingsClient (scopedKeys()); same-thread only.
class Settings1ScreenshotPreferences final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY preferencesChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY preferencesChanged)
    Q_PROPERTY(QString effectiveFolder READ effectiveFolder NOTIFY preferencesChanged)
    Q_PROPERTY(QString fileNamePattern READ fileNamePattern NOTIFY preferencesChanged)
    Q_PROPERTY(QString defaultMode READ defaultMode NOTIFY preferencesChanged)
    Q_PROPERTY(int delaySeconds READ delaySeconds NOTIFY preferencesChanged)
    Q_PROPERTY(bool showResultWindow READ showResultWindow NOTIFY preferencesChanged)
    Q_PROPERTY(QString recordFinish READ recordFinish NOTIFY preferencesChanged)
    Q_PROPERTY(bool writePending READ writePending NOTIFY writeStatusChanged)
    Q_PROPERTY(QString writeStatusText READ writeStatusText NOTIFY writeStatusChanged)

public:
    static const QStringList &scopedKeys();
    static const QStringList &modeIds();
    static const QStringList &recordFinishIds();

    explicit Settings1ScreenshotPreferences(SettingsClient::SettingsClient &client,
                                            QObject *parent = nullptr);

    [[nodiscard]] bool isLoaded() const;
    // Empty means the default, XDG_PICTURES_DIR/Screenshots.
    [[nodiscard]] QString folder() const { return m_folder; }
    [[nodiscard]] QString effectiveFolder() const;
    [[nodiscard]] QString fileNamePattern() const { return m_pattern; }
    [[nodiscard]] QString defaultMode() const { return m_mode; }
    [[nodiscard]] int delaySeconds() const { return m_delay; }
    [[nodiscard]] bool showResultWindow() const { return m_showResult; }
    // notify | show-in-folder | quiet: what happens when OBS finishes a file.
    [[nodiscard]] QString recordFinish() const { return m_recordFinish; }
    [[nodiscard]] bool writePending() const { return !m_pendingKey.isEmpty(); }
    [[nodiscard]] QString writeStatusText() const { return m_writeStatus; }

    Q_INVOKABLE bool setFolder(const QString &folder);
    Q_INVOKABLE bool setFileNamePattern(const QString &pattern);
    Q_INVOKABLE bool setDefaultMode(const QString &modeId);
    Q_INVOKABLE bool setDelaySeconds(int seconds);
    Q_INVOKABLE bool setShowResultWindow(bool show);
    Q_INVOKABLE bool setRecordFinish(const QString &finish);
    // The name a capture taken now would get with `pattern`.
    Q_INVOKABLE QString fileNamePreview(const QString &pattern) const;

Q_SIGNALS:
    void preferencesChanged();
    void writeStatusChanged();

private:
    bool request(const QString &key, const QVariant &value);
    void onSnapshotChanged();
    void onStateChanged();
    void onCommitFinished(const SettingsClient::CommitOutcome &outcome);
    void onCommitUncertain(const QString &message);
    void onReadbackTick();
    void setWriteStatus(const QString &status);
    void clearPending();

    SettingsClient::SettingsClient &m_client;
    QString m_folder;
    QString m_pattern;
    QString m_mode;
    int m_delay = 0;
    bool m_showResult = true;
    QString m_recordFinish;
    bool m_loaded = false;
    bool m_awaitingReadback = false;
    QString m_pendingKey;
    QVariant m_pendingValue;
    QString m_pendingOwner;
    QString m_pendingEpoch;
    quint64 m_pendingRevisionFloor = 0;
    QElapsedTimer m_readbackAge;
    QTimer m_readbackTimer;
    QString m_writeStatus;
};

} // namespace QindaQt::Services::ScreenshotPreferences
