// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/voice_protocol/voice_types.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>

namespace QindaQt::Services::Voice {
class VoiceClient;
}

namespace QindaQt::Apps::Voice {

// The Voice console's whole model: live provider state, the session's
// dictation history, and one intent per control.
//
// AGENT-CONTRACT: the Voice1 client is borrowed and must outlive this object.
// QML receives bounded values only; the client, its owner, and the session bus
// never cross this boundary.
//
// AGENT-GUARD: the history is session-only and in memory. It is a convenience
// for "what did I just say", not a transcript store: nothing here is written
// to disk, and closing the window is what erases it. Persisting it would make
// the console a record of everything the user has ever dictated, which is a
// different product with different consent.
class VoiceConsoleModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString phase READ phase NOTIFY viewChanged)
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY viewChanged)
    Q_PROPERTY(bool canRetryConnection READ canRetryConnection NOTIFY viewChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged)
    Q_PROPERTY(QString stateLabel READ stateLabel NOTIFY viewChanged)
    Q_PROPERTY(QString stateVariant READ stateVariant NOTIFY viewChanged)
    Q_PROPERTY(bool capturing READ capturing NOTIFY viewChanged)
    Q_PROPERTY(bool commandMode READ commandMode NOTIFY viewChanged)
    Q_PROPERTY(bool shortcutsArmed READ shortcutsArmed NOTIFY viewChanged)
    Q_PROPERTY(int levelPercent READ levelPercent NOTIFY levelChanged)
    Q_PROPERTY(QString partialText READ partialText NOTIFY viewChanged)
    Q_PROPERTY(QString providerLabel READ providerLabel NOTIFY viewChanged)
    Q_PROPERTY(QString providerId READ providerId NOTIFY viewChanged)
    Q_PROPERTY(QString languageLabel READ languageLabel NOTIFY viewChanged)
    Q_PROPERTY(QString microphoneLabel READ microphoneLabel NOTIFY viewChanged)
    Q_PROPERTY(QString dictationShortcut READ dictationShortcut NOTIFY viewChanged)
    Q_PROPERTY(QString commandShortcut READ commandShortcut NOTIFY viewChanged)
    Q_PROPERTY(QString routeLabel READ routeLabel NOTIFY viewChanged)
    Q_PROPERTY(QVariantList providerRows READ providerRows NOTIFY viewChanged)
    Q_PROPERTY(QVariantList capabilityRows READ capabilityRows NOTIFY viewChanged)
    Q_PROPERTY(QVariantList historyRows READ historyRows NOTIFY historyChanged)
    Q_PROPERTY(int historyCount READ historyCount NOTIFY historyChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY historyChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY viewChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY viewChanged)
    Q_PROPERTY(QString feedbackVariant READ feedbackVariant NOTIFY viewChanged)
    Q_PROPERTY(bool canDictate READ canDictate NOTIFY viewChanged)
    Q_PROPERTY(bool canCommand READ canCommand NOTIFY viewChanged)
    Q_PROPERTY(bool canFinish READ canFinish NOTIFY viewChanged)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY viewChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY viewChanged)
    Q_PROPERTY(bool canCopy READ canCopy NOTIFY viewChanged)
    Q_PROPERTY(bool canChooseProvider READ canChooseProvider NOTIFY viewChanged)

public:
    explicit VoiceConsoleModel(Services::Voice::VoiceClient &client,
                               QObject *parent = nullptr);

    [[nodiscard]] QString phase() const;
    [[nodiscard]] bool serviceAvailable() const noexcept { return m_ready; }
    [[nodiscard]] bool canRetryConnection() const noexcept;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString stateLabel() const;
    [[nodiscard]] QString stateVariant() const;
    [[nodiscard]] bool capturing() const noexcept;
    [[nodiscard]] bool commandMode() const noexcept;
    [[nodiscard]] bool shortcutsArmed() const noexcept { return m_snapshot.enabled; }
    [[nodiscard]] int levelPercent() const noexcept { return static_cast<int>(m_level); }
    [[nodiscard]] QString partialText() const { return m_snapshot.partialText; }
    [[nodiscard]] QString providerLabel() const;
    [[nodiscard]] QString providerId() const { return m_snapshot.providerId; }
    [[nodiscard]] QString languageLabel() const { return m_snapshot.languageCode; }
    [[nodiscard]] QString microphoneLabel() const { return m_snapshot.microphoneLabel; }
    [[nodiscard]] QString dictationShortcut() const { return m_snapshot.dictationShortcut; }
    [[nodiscard]] QString commandShortcut() const { return m_snapshot.commandShortcut; }
    [[nodiscard]] QString routeLabel() const;
    [[nodiscard]] QVariantList providerRows() const;
    [[nodiscard]] QVariantList capabilityRows() const;
    [[nodiscard]] QVariantList historyRows() const;
    [[nodiscard]] int historyCount() const noexcept
    {
        return static_cast<int>(m_history.size());
    }
    [[nodiscard]] QString filterText() const { return m_filter; }
    void setFilterText(const QString &filter);
    [[nodiscard]] bool busy() const noexcept { return m_requestId != 0; }
    [[nodiscard]] QString feedback() const { return m_feedback; }
    [[nodiscard]] QString feedbackVariant() const { return m_feedbackVariant; }
    [[nodiscard]] bool canDictate() const noexcept;
    [[nodiscard]] bool canCommand() const noexcept;
    [[nodiscard]] bool canFinish() const noexcept;
    [[nodiscard]] bool canRetry() const noexcept;
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canCopy() const noexcept;
    [[nodiscard]] bool canChooseProvider() const noexcept;

    Q_INVOKABLE bool startDictation();
    Q_INVOKABLE bool startCommand();
    Q_INVOKABLE bool finish();
    Q_INVOKABLE bool cancel();
    Q_INVOKABLE bool retry();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool copyLast();
    Q_INVOKABLE bool setShortcutsArmed(bool armed);
    Q_INVOKABLE bool selectProvider(const QString &providerId);
    Q_INVOKABLE void retryConnection();
    Q_INVOKABLE void dismissFeedback();

    // History is the console's own state, so these never reach the provider.
    Q_INVOKABLE bool copyHistoryEntry(int entryId);
    Q_INVOKABLE void clearHistory();

Q_SIGNALS:
    void viewChanged();
    void levelChanged();
    void historyChanged();
    void connectionRetryRequested();

private:
    struct HistoryEntry {
        int id = 0;
        QString text;
        QString routeLabel;
        QString timeText;
        bool command = false;
    };

    void handleState();
    void handleSnapshot(const Services::Voice::Snapshot &snapshot);
    void handleLevel(quint32 level);
    void handleResult(quint64 requestId,
                      const Services::Voice::OperationResult &result);
    void recordDelivery(const Services::Voice::Snapshot &snapshot);
    void publishFeedback(QString text, QString variant);
    [[nodiscard]] bool submit(Services::Voice::OperationKind kind,
                              const QString &providerId, bool enable);

    Services::Voice::VoiceClient &m_client;
    Services::Voice::Snapshot m_snapshot;
    QList<HistoryEntry> m_history;
    QString m_filter;
    QString m_feedback;
    QString m_feedbackVariant = QStringLiteral("info");
    QString m_owner;
    QString m_recordedText;
    quint64 m_requestId = 0;
    quint32 m_level = 0;
    int m_nextEntryId = 1;
    bool m_ready = false;
};

} // namespace QindaQt::Apps::Voice
