// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::Audio
{

// Private, same-worker observations of daemon globals. Payload PIDs, node-name
// prefixes and tokens alone are never producer evidence (ADR0361).
struct SendProducerIdentity {
    quint64 generation = 0;
    quint32 clientId = 0;
    quint64 clientSerial = 0;
    friend bool operator==(const SendProducerIdentity &,
                           const SendProducerIdentity &) = default;
    [[nodiscard]] bool valid() const noexcept;
};

enum class SendNodeRole { Capture, Playback };

struct ObservedSendNode {
    quint64 serial = 0;
    quint32 boundId = 0;
    quintptr objectIdentity = 0;
    quint32 clientId = 0;
    QString name;
    QString token;
    QString role;
    QString mediaClass;
    friend bool operator==(const ObservedSendNode &,
                           const ObservedSendNode &) = default;
};

struct SendModuleDeclaration {
    quint64 instance = 0;
    QString token;
    QString playbackName;
};

// Owns only observation admission, not modules or Wp objects. The worker's
// module record owns the corresponding instance until it retires/dies. All
// methods run on that same worker; callers refresh from current daemon globals
// before projection/writes. Borrowed observations are copied bounded values.
class ConsoleSendAdmission final
{
public:
    void reset(SendProducerIdentity producer);
    void revoke();
    [[nodiscard]] std::optional<SendModuleDeclaration> reserve(const QString &playbackName);
    // Both halves must match one exact actual producer before playback writes.
    // Once a bound object is lost/replaced it cannot be reacquired in this
    // instance, even if its token and name return. Worker retires that module.
    void observe(SendProducerIdentity producer, const QList<ObservedSendNode> &nodes);
    void retire(quint64 instance);
    [[nodiscard]] QList<quint64> invalidInstances() const;
    [[nodiscard]] std::optional<ObservedSendNode> playback(quint64 instance) const;
    // Retiring pinned objects remain invisible while removal is pending but
    // are non-writable. An object-removed observation clears them permanently.
    [[nodiscard]] bool excludes(quint64 serial, quintptr objectIdentity) const;
    [[nodiscard]] bool excludesBoundId(quint32 boundId) const;
    [[nodiscard]] qsizetype size() const noexcept { return m_records.size(); }

private:
    struct Pin {
        std::optional<ObservedSendNode> node;
        bool everBound = false;
        bool lost = false;
    };
    struct Record {
        SendModuleDeclaration declaration;
        Pin capture;
        Pin playback;
        bool retiring = false;
        bool invalid = false;
    };
    [[nodiscard]] bool matches(const ObservedSendNode &node, const Record &record,
                               SendNodeRole role) const;
    void observePin(Pin &pin, Record &record, SendNodeRole role,
                    const QList<ObservedSendNode> &nodes);

    SendProducerIdentity m_producer;
    quint64 m_nextInstance = 0;
    QHash<quint64, Record> m_records;
};

inline constexpr char kSendInstanceProperty[] = "qindaqt.console.send.instance";
inline constexpr char kSendRoleProperty[] = "qindaqt.console.send.role";

} // namespace QindaQt::Audio
