// SPDX-License-Identifier: GPL-3.0-or-later
#include "console_send_admission_p.h"

#include "wireplumber_routing_p.h"
#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QtCore/QUuid>

#include <algorithm>
#include <limits>

namespace QindaQt::Audio
{

bool SendProducerIdentity::valid() const noexcept
{
    return generation != 0 && clientSerial != 0 && clientId != 0
        && clientId != std::numeric_limits<quint32>::max();
}

void ConsoleSendAdmission::reset(const SendProducerIdentity producer)
{
    if (producer == m_producer) {
        return;
    }
    m_records.clear();
    m_producer = producer.valid() ? producer : SendProducerIdentity{};
}

void ConsoleSendAdmission::revoke()
{
    m_records.clear();
    m_producer = {};
}

std::optional<SendModuleDeclaration> ConsoleSendAdmission::reserve(
    const QString &playbackName)
{
    constexpr qsizetype maxRecords = kMaxStrips * kMaxBuses * 2;
    if (!m_producer.valid() || !routingNameIsEmbeddable(playbackName)
        || !routingNameIsEmbeddable(playbackName + QStringLiteral(".capture"))
        || m_records.size() >= maxRecords
        || m_nextInstance == std::numeric_limits<quint64>::max()) {
        return std::nullopt;
    }
    SendModuleDeclaration value{
        .instance = ++m_nextInstance,
        .token = QUuid::createUuid().toString(QUuid::Id128),
        .playbackName = playbackName};
    m_records.insert(value.instance, Record{.declaration = value, .capture = {}, .playback = {},
                                          .retiring = false, .invalid = false});
    return value;
}

bool ConsoleSendAdmission::matches(const ObservedSendNode &node,
                                   const Record &record,
                                   const SendNodeRole role) const
{
    const bool playback = role == SendNodeRole::Playback;
    return node.serial != 0 && node.boundId != 0
        && node.boundId != std::numeric_limits<quint32>::max()
        && node.objectIdentity != 0 && node.clientId == m_producer.clientId
        && node.token == record.declaration.token
        && node.name == (playback ? record.declaration.playbackName
                                 : record.declaration.playbackName + QStringLiteral(".capture"))
        && node.role == (playback ? QStringLiteral("playback") : QStringLiteral("capture"))
        && node.mediaClass == (playback ? QStringLiteral("Stream/Output/Audio")
                                       : QStringLiteral("Stream/Input/Audio"));
}

void ConsoleSendAdmission::observePin(Pin &pin, Record &record,
                                     const SendNodeRole role,
                                     const QList<ObservedSendNode> &nodes)
{
    const ObservedSendNode *candidate = nullptr;
    bool ambiguous = false;
    for (const ObservedSendNode &node : nodes) {
        if (matches(node, record, role)) {
            if (candidate != nullptr) {
                ambiguous = true;
                break;
            }
            candidate = &node;
        }
    }
    if (ambiguous) {
        pin.node.reset();
        pin.lost = true;
        record.invalid = true;
        return;
    }
    if (pin.node.has_value()) {
        if (candidate == nullptr || *candidate != *pin.node) {
            pin.node.reset();
            pin.lost = true;
            record.invalid = true;
        }
        return;
    }
    // Retiring or lost roles never acquire an object, including one with a
    // recycled node ID or the complete former token/role/name tuple.
    if (pin.lost || pin.everBound || record.retiring || record.invalid) {
        return;
    }
    if (candidate != nullptr) {
        pin.node = *candidate;
        pin.everBound = true;
    }
}

void ConsoleSendAdmission::observe(const SendProducerIdentity producer,
                                   const QList<ObservedSendNode> &nodes)
{
    if (!producer.valid() || producer != m_producer) {
        revoke();
        return;
    }
    for (auto it = m_records.begin(); it != m_records.end();) {
        Record &record = it.value();
        observePin(record.capture, record, SendNodeRole::Capture, nodes);
        observePin(record.playback, record, SendNodeRole::Playback, nodes);
        if (record.retiring && !record.capture.node && !record.playback.node) {
            it = m_records.erase(it);
        } else {
            ++it;
        }
    }
}

void ConsoleSendAdmission::retire(const quint64 instance)
{
    const auto it = m_records.find(instance);
    if (it == m_records.end()) {
        return;
    }
    it->retiring = true;
    if (!it->capture.node && !it->playback.node) {
        m_records.erase(it);
    }
}

QList<quint64> ConsoleSendAdmission::invalidInstances() const
{
    QList<quint64> result;
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        if (it->invalid && !it->retiring) {
            result.append(it.key());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::optional<ObservedSendNode> ConsoleSendAdmission::playback(
    const quint64 instance) const
{
    const auto it = m_records.constFind(instance);
    if (!m_producer.valid() || it == m_records.cend() || it->retiring
        || it->invalid || !it->capture.node || !it->playback.node) {
        return std::nullopt;
    }
    return it->playback.node;
}

bool ConsoleSendAdmission::excludes(const quint64 serial,
                                   const quintptr objectIdentity) const
{
    if (!m_producer.valid()) {
        return false;
    }
    for (const Record &record : m_records) {
        // A malformed active declaration must be retired/quieted by the worker
        // before public projection. It never grants a partial gain write.
        for (const Pin *pin : {&record.capture, &record.playback}) {
            if (pin->node && pin->node->serial == serial
                && pin->node->objectIdentity == objectIdentity) {
                return true;
            }
        }
    }
    return false;
}

bool ConsoleSendAdmission::excludesBoundId(const quint32 boundId) const
{
    if (!m_producer.valid()) {
        return false;
    }
    for (const Record &record : m_records) {
        for (const Pin *pin : {&record.capture, &record.playback}) {
            if (pin->node && pin->node->boundId == boundId) {
                return true;
            }
        }
    }
    return false;
}

} // namespace QindaQt::Audio
