// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_identity/identity_resolver.h>

#include <qindaqt/services/display_identity/identity_limits.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QHash>
#include <QtCore/QRegularExpression>

namespace QindaQt::DisplayIdentity
{
namespace
{

struct Materials {
    QByteArray identifier;
    QByteArray rawEdid;
    QByteArray mstComposite;
};

bool boundedSafeText(const QString &value, const qsizetype maximumBytes,
                     const bool required)
{
    if (required && value.isEmpty()) {
        return false;
    }
    if (value.contains(QChar::Null) || value.toUtf8().size() > maximumBytes) {
        return false;
    }
    for (const QChar character : value) {
        if (character.category() == QChar::Other_Control
            || character.category() == QChar::Other_Format) {
            return false;
        }
    }
    return true;
}

ResolutionResult failure(const IdentityError error, const char *reason)
{
    return {.outputs = {}, .error = error, .reasonCode = QString::fromLatin1(reason)};
}

QByteArray mstComposite(const ObservedOutput &output)
{
    if (output.mstPath.isEmpty()) {
        return {};
    }
    QByteArray material("QindaQt-MST-v1\0", 15);
    if (output.edidState == EdidState::Valid && !output.edidIdentifier.isEmpty()) {
        material.append(output.edidIdentifier);
    } else if (output.edidState == EdidState::Valid && !output.rawEdid.isEmpty()) {
        material.append(QCryptographicHash::hash(output.rawEdid,
                                                QCryptographicHash::Sha256));
    }
    material.append('\0');
    material.append(output.mstPath.toUtf8());
    return material;
}

template<typename T>
QHash<T, qsizetype> occurrenceCounts(const QList<T> &values)
{
    QHash<T, qsizetype> counts;
    for (const T &value : values) {
        if (!value.isEmpty()) {
            counts[value] += 1;
        }
    }
    return counts;
}

QString hashedId(const char *prefix, const QByteArray &material,
                 const DigestFunction &digest, bool &valid)
{
    const QByteArray digestValue = digest(material);
    if (digestValue.size() != kDigestBytes) {
        valid = false;
        return {};
    }
    return QString::fromLatin1(prefix) + QString::fromLatin1(digestValue.toHex());
}

QString connectorFallback(const QString &connector, const DigestFunction &digest,
                          IdentitySource &source, bool &valid)
{
    static const QRegularExpression safeConnector(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.:-]*$"));
    if (connector.toUtf8().size() <= 110 && safeConnector.match(connector).hasMatch()) {
        source = IdentitySource::Connector;
        return QStringLiteral("conn:") + connector;
    }
    source = IdentitySource::ConnectorHash;
    return hashedId("connhash:", connector.toUtf8(), digest, valid);
}

} // namespace

QByteArray truncatedSha256(const QByteArrayView value)
{
    return QCryptographicHash::hash(value, QCryptographicHash::Sha256).first(kDigestBytes);
}

ResolutionResult resolve(const QList<ObservedOutput> &connectedOutputs)
{
    return resolve(connectedOutputs, truncatedSha256);
}

ResolutionResult resolve(const QList<ObservedOutput> &connectedOutputs,
                         const DigestFunction &digest)
{
    if (!digest) {
        return failure(IdentityError::InvalidDigest, "missing-digest-function");
    }
    if (connectedOutputs.size() > kMaxConnectedOutputs) {
        return failure(IdentityError::TooManyOutputs, "too-many-connected-outputs");
    }

    QList<Materials> materials;
    QList<QByteArray> identifiers;
    QList<QByteArray> rawEdids;
    QList<QByteArray> mstComposites;
    materials.reserve(connectedOutputs.size());
    for (const ObservedOutput &output : connectedOutputs) {
        if (!boundedSafeText(output.connectorName, kMaxConnectorUtf8Bytes, true)) {
            return failure(IdentityError::InvalidConnector, "invalid-connector");
        }
        if (!boundedSafeText(output.runtimeCompositorUuid, kMaxCompositorUuidUtf8Bytes,
                             false)) {
            return failure(IdentityError::InvalidCompositorUuid,
                           "invalid-runtime-compositor-uuid");
        }
        if (!boundedSafeText(output.mstPath, kMaxMstPathUtf8Bytes, false)) {
            return failure(IdentityError::InvalidMstPath, "invalid-mst-path");
        }
        if (!boundedSafeText(output.manufacturer, kMaxManufacturerUtf8Bytes, false)
            || !boundedSafeText(output.model, kMaxModelUtf8Bytes, false)) {
            return failure(IdentityError::InvalidMetadata, "invalid-display-metadata");
        }
        if (output.edidIdentifier.size() > kMaxEdidIdentifierBytes
            || output.rawEdid.size() > kMaxRawEdidBytes
            || (output.edidState == EdidState::Absent
                && (!output.edidIdentifier.isEmpty() || !output.rawEdid.isEmpty()))
            || (output.edidState == EdidState::Valid && output.rawEdid.isEmpty())) {
            return failure(IdentityError::InvalidEdidMaterial, "invalid-edid-material");
        }

        Materials value;
        if (output.edidState == EdidState::Valid) {
            value.identifier = output.edidIdentifier;
            value.rawEdid = output.rawEdid;
        }
        value.mstComposite = mstComposite(output);
        identifiers.push_back(value.identifier);
        rawEdids.push_back(value.rawEdid);
        mstComposites.push_back(value.mstComposite);
        materials.push_back(std::move(value));
    }

    const auto identifierCounts = occurrenceCounts(identifiers);
    const auto rawCounts = occurrenceCounts(rawEdids);
    const auto mstCounts = occurrenceCounts(mstComposites);
    QList<ResolvedOutput> resolved;
    resolved.reserve(connectedOutputs.size());
    for (qsizetype index = 0; index < connectedOutputs.size(); ++index) {
        const ObservedOutput &input = connectedOutputs.at(index);
        const Materials &value = materials.at(index);
        bool digestValid = true;
        ResolvedOutput output{.stableId = {},
                              .connectorName = input.connectorName,
                              .source = IdentitySource::Connector,
                              .ambiguous = false,
                              .manufacturer = input.manufacturer,
                              .model = input.model,
                              .hasSerial = input.edidState == EdidState::Valid
                                  && input.hasSerial,
                              .internal = input.internal};
        if (!value.identifier.isEmpty() && identifierCounts.value(value.identifier) == 1) {
            output.source = IdentitySource::EdidIdentifier;
            output.stableId = hashedId("edid:", value.identifier, digest, digestValid);
        } else if (!value.rawEdid.isEmpty() && rawCounts.value(value.rawEdid) == 1) {
            output.source = IdentitySource::RawEdid;
            output.stableId = hashedId("edidraw:", value.rawEdid, digest, digestValid);
        } else if (!value.mstComposite.isEmpty()
                   && mstCounts.value(value.mstComposite) == 1) {
            output.source = IdentitySource::MstPath;
            output.stableId = hashedId("mst:", value.mstComposite, digest, digestValid);
        } else {
            output.stableId = connectorFallback(input.connectorName, digest, output.source,
                                                digestValid);
        }
        output.ambiguous = (!value.identifier.isEmpty()
                            && identifierCounts.value(value.identifier) > 1)
            || (!value.rawEdid.isEmpty() && rawCounts.value(value.rawEdid) > 1)
            || (!value.mstComposite.isEmpty() && mstCounts.value(value.mstComposite) > 1);
        if (!digestValid || output.stableId.toUtf8().size() > kMaxStableIdUtf8Bytes) {
            return failure(IdentityError::InvalidDigest, "invalid-digest-result");
        }
        resolved.push_back(std::move(output));
    }

    QHash<QString, QList<qsizetype>> collisions;
    for (qsizetype index = 0; index < resolved.size(); ++index) {
        collisions[resolved.at(index).stableId].push_back(index);
    }
    for (auto iterator = collisions.cbegin(); iterator != collisions.cend(); ++iterator) {
        if (iterator.value().size() < 2) {
            continue;
        }
        for (qsizetype collisionIndex = 0; collisionIndex < iterator.value().size();
             ++collisionIndex) {
            ResolvedOutput &output = resolved[iterator.value().at(collisionIndex)];
            output.stableId += QStringLiteral("#") + QString::number(collisionIndex + 1);
            output.ambiguous = true;
        }
    }
    return {.outputs = std::move(resolved), .error = IdentityError::None, .reasonCode = {}};
}

} // namespace QindaQt::DisplayIdentity
