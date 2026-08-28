// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::DisplayIdentity
{

enum class EdidState {
    Absent,
    Valid,
    Malformed,
};

enum class IdentitySource {
    EdidIdentifier,
    RawEdid,
    MstPath,
    Connector,
    ConnectorHash,
};

enum class IdentityError {
    None,
    TooManyOutputs,
    InvalidConnector,
    InvalidCompositorUuid,
    InvalidEdidMaterial,
    InvalidMstPath,
    InvalidMetadata,
    InvalidDigest,
};

struct ObservedOutput {
    QString connectorName;
    QString runtimeCompositorUuid;
    EdidState edidState = EdidState::Absent;
    QByteArray edidIdentifier;
    QByteArray rawEdid;
    QString mstPath;
    QString manufacturer;
    QString model;
    bool hasSerial = false;
    bool internal = false;

    friend bool operator==(const ObservedOutput &, const ObservedOutput &) = default;
};

struct ResolvedOutput {
    QString stableId;
    QString connectorName;
    IdentitySource source = IdentitySource::Connector;
    bool ambiguous = false;
    QString manufacturer;
    QString model;
    bool hasSerial = false;
    bool internal = false;

    friend bool operator==(const ResolvedOutput &, const ResolvedOutput &) = default;
};

struct ResolutionResult {
    QList<ResolvedOutput> outputs;
    IdentityError error = IdentityError::None;
    QString reasonCode;

    [[nodiscard]] bool succeeded() const noexcept { return error == IdentityError::None; }
};

} // namespace QindaQt::DisplayIdentity

Q_DECLARE_METATYPE(QindaQt::DisplayIdentity::EdidState)
Q_DECLARE_METATYPE(QindaQt::DisplayIdentity::IdentitySource)
Q_DECLARE_METATYPE(QindaQt::DisplayIdentity::IdentityError)
Q_DECLARE_METATYPE(QindaQt::DisplayIdentity::ObservedOutput)
Q_DECLARE_METATYPE(QindaQt::DisplayIdentity::ResolvedOutput)
