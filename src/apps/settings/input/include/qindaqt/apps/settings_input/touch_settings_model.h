// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/settings_client/settings_client.h>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <optional>

namespace QindaQt::Apps::SettingsInput {

// Settings → Input → Touch (ADR-0205). Truth is the purpose-scoped Settings1
// snapshot of input.touch.*. A retained snapshot supplies last-known values
// while authority is unavailable; only a current Ready snapshot admits edits.
// Successful writes reconcile through a post-commit snapshot before another
// write, and uncertain commits are never replayed.
class TouchSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool hasLastKnown READ hasLastKnown NOTIFY changed)
    Q_PROPERTY(bool editable READ editable NOTIFY changed)
    Q_PROPERTY(bool longPressQueueable READ longPressQueueable NOTIFY changed)
    Q_PROPERTY(bool touchscreenEnabled READ touchscreenEnabled NOTIFY changed)
    Q_PROPERTY(int longPressMs READ longPressMs NOTIFY changed)
    Q_PROPERTY(int longPressDisplayMs READ longPressDisplayMs NOTIFY changed)
    Q_PROPERTY(QString onScreenKeyboard READ onScreenKeyboard NOTIFY changed)
    Q_PROPERTY(QString edgeLeft READ edgeLeft NOTIFY changed)
    Q_PROPERTY(QString edgeTop READ edgeTop NOTIFY changed)
    Q_PROPERTY(QString edgeRight READ edgeRight NOTIFY changed)
    Q_PROPERTY(QString edgeBottom READ edgeBottom NOTIFY changed)
    Q_PROPERTY(QVariantList keyboardChoices READ keyboardChoices CONSTANT)
    Q_PROPERTY(QVariantList edgeActionChoices READ edgeActionChoices CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)

public:
    explicit TouchSettingsModel(Services::SettingsClient::SettingsClient &client, QObject *parent = nullptr);
    ~TouchSettingsModel() override;

    [[nodiscard]] static QStringList settingsKeys();
    [[nodiscard]] static QStringList edgeNames();

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool hasLastKnown() const;
    [[nodiscard]] bool editable() const;
    [[nodiscard]] bool longPressQueueable() const;
    [[nodiscard]] bool touchscreenEnabled() const;
    [[nodiscard]] int longPressMs() const;
    [[nodiscard]] int longPressDisplayMs() const;
    [[nodiscard]] QString onScreenKeyboard() const;
    [[nodiscard]] QString edgeLeft() const { return edgeAction(QStringLiteral("left")); }
    [[nodiscard]] QString edgeTop() const { return edgeAction(QStringLiteral("top")); }
    [[nodiscard]] QString edgeRight() const { return edgeAction(QStringLiteral("right")); }
    [[nodiscard]] QString edgeBottom() const { return edgeAction(QStringLiteral("bottom")); }
    // Q_INVOKABLE: the Touch section's edge-row Repeater binds this per edge.
    Q_INVOKABLE [[nodiscard]] QString edgeAction(const QString &edge) const;
    [[nodiscard]] QVariantList keyboardChoices() const;
    [[nodiscard]] QVariantList edgeActionChoices() const;
    [[nodiscard]] bool busy() const noexcept { return m_busy; }
    [[nodiscard]] const QString &statusText() const noexcept { return m_statusText; }
    [[nodiscard]] const QString &errorText() const noexcept { return m_errorText; }

    // Starts the scoped client the first time a route shows the rows; a
    // later call only refreshes the snapshot.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setTouchscreenEnabled(bool enabled);
    Q_INVOKABLE bool setLongPressMs(int milliseconds);
    Q_INVOKABLE bool setOnScreenKeyboard(const QString &mode);
    Q_INVOKABLE bool setEdgeAction(const QString &edge, const QString &action);
    Q_INVOKABLE int choiceIndex(const QVariantList &choices, const QString &value) const;

Q_SIGNALS:
    void changed();

private:
    [[nodiscard]] QVariant value(const QString &key) const;
    [[nodiscard]] bool submit(const QString &key, const QVariant &value);
    void handleSnapshot();
    void handleAuthorityChange();
    void abandonWrite(const QString &message);
    void publishStatus();

    Services::SettingsClient::SettingsClient &m_client;
    QString m_statusText;
    QString m_errorText;
    bool m_started = false;
    bool m_busy = false;
    bool m_awaitingSnapshot = false;
    QString m_writeKey;
    QString m_writeOwner;
    QString m_writeEpoch;
    QVariant m_writeValue;
    quint64 m_resultRevision = 0;
    std::optional<int> m_queuedLongPress;
};

} // namespace QindaQt::Apps::SettingsInput
