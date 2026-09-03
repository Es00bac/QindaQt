// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/shellwindowidentity.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <limits>
#include <utility>

namespace QindaQt::Compositor {
namespace {

constexpr qsizetype MaximumPayloadBytes = 64 * 1024;

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

bool canonicalWindowId(const QString &value)
{
    const QUuid uuid(value);
    return !uuid.isNull() && value.size() <= ShellWindowActionMaximumWindowIdCharacters
        && uuid.toString(QUuid::WithoutBraces) == value;
}

bool canonicalRevisionString(const QString &value, quint64 *revision)
{
    if (value.isEmpty() || (value.size() > 1 && value.startsWith(u'0'))
        || value.size() > ShellWindowActionMaximumRevisionCharacters) {
        return false;
    }
    for (const QChar character : value) {
        if (!character.isDigit() || character.unicode() > u'9') {
            return false;
        }
    }
    bool ok = false;
    const quint64 parsed = value.toULongLong(&ok, 10);
    if (!ok) {
        return false;
    }
    *revision = parsed;
    return true;
}

bool canonicalBusName(const QString &name)
{
    const QByteArray bytes = name.toUtf8();
    if (bytes.isEmpty() || bytes.size() > ShellWindowIdentityMaximumServiceBytes
        || bytes.contains('\0')) {
        return false;
    }
    const bool unique = bytes.startsWith(':');
    const QList<QByteArray> parts = bytes.mid(unique ? 1 : 0).split('.');
    if (parts.size() < 2) {
        return false;
    }
    for (qsizetype index = 0; index < parts.size(); ++index) {
        const auto &part = parts[index];
        if (part.isEmpty()) {
            return false;
        }
        for (qsizetype offset = 0; offset < part.size(); ++offset) {
            const unsigned char character = static_cast<unsigned char>(part[offset]);
            const bool alphaNumeric = (character >= 'A' && character <= 'Z')
                || (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9');
            if (!alphaNumeric && character != '_' && character != '-') {
                return false;
            }
            if (!unique && index == 0 && offset == 0
                && character >= '0' && character <= '9') {
                return false;
            }
        }
    }
    return true;
}

bool canonicalObjectPath(const QString &path)
{
    const QByteArray bytes = path.toUtf8();
    if (bytes.isEmpty() || bytes.size() > ShellWindowIdentityMaximumObjectPathBytes
        || bytes.contains('\0') || !bytes.startsWith('/')) {
        return false;
    }
    if (bytes == "/") {
        return true;
    }
    if (bytes.endsWith('/') || bytes.contains("//")) {
        return false;
    }
    for (const char rawCharacter : bytes.mid(1)) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        const bool alphaNumeric = (character >= 'A' && character <= 'Z')
            || (character >= 'a' && character <= 'z')
            || (character >= '0' && character <= '9');
        if (!alphaNumeric && character != '_' && character != '/') {
            return false;
        }
    }
    return true;
}

bool validFacts(const ShellWindowIdentityFacts &facts)
{
    const bool addressPaired = facts.appMenuServiceName.has_value()
        == facts.appMenuObjectPath.has_value();
    return canonicalWindowId(facts.windowId)
        && (!facts.processId || *facts.processId > 1)
        && (!facts.appMenuWindowId || *facts.appMenuWindowId != 0)
        && addressPaired
        && (!facts.appMenuServiceName
            || canonicalBusName(*facts.appMenuServiceName))
        && (!facts.appMenuObjectPath
            || canonicalObjectPath(*facts.appMenuObjectPath));
}

QJsonValue optionalInteger(const std::optional<qint64> &value)
{
    return value ? QJsonValue(QString::number(*value)) : QJsonValue::Null;
}

QJsonValue optionalWindowId(const std::optional<quint32> &value)
{
    return value ? QJsonValue(static_cast<qint64>(*value)) : QJsonValue::Null;
}

QJsonValue optionalString(const std::optional<QString> &value)
{
    return value ? QJsonValue(*value) : QJsonValue::Null;
}

QJsonObject factsJson(const ShellWindowIdentityFacts &facts)
{
    return {{QStringLiteral("windowId"), facts.windowId},
            {QStringLiteral("processId"), optionalInteger(facts.processId)},
            {QStringLiteral("appMenuWindowId"),
             optionalWindowId(facts.appMenuWindowId)},
            {QStringLiteral("appMenuServiceName"),
             optionalString(facts.appMenuServiceName)},
            {QStringLiteral("appMenuObjectPath"),
             optionalString(facts.appMenuObjectPath)}};
}

QByteArray unavailableJson(const QString &epoch, quint64 revision,
                           const QString &code, const QString &message)
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("status"), QStringLiteral("unavailable")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("epoch"), epoch},
        {QStringLiteral("revision"), QString::number(revision)},
        {QStringLiteral("failure"),
         QJsonObject{{QStringLiteral("code"), code},
                     {QStringLiteral("message"), message}}},
    }).toJson(QJsonDocument::Compact);
}

std::optional<QString> optionalJsonString(const QJsonObject &object,
                                          const QString &key, bool *ok)
{
    const QJsonValue value = object.value(key);
    if (value.isNull()) {
        return std::nullopt;
    }
    if (!value.isString()) {
        *ok = false;
        return std::nullopt;
    }
    return value.toString();
}

} // namespace

bool ShellWindowIdentitySnapshot::available() const noexcept
{
    return status == ShellWindowIdentityStatus::Ok;
}

ShellWindowIdentityStore::ShellWindowIdentityStore(QString epoch,
                                                   quint64 revisionSeed)
    : m_snapshotJson(unavailableJson(
          epoch, revisionSeed, QStringLiteral("identity-unavailable"),
          QStringLiteral("no active-window identity has been published")))
    , m_epoch(std::move(epoch))
    , m_revision(revisionSeed)
{
}

ShellWindowIdentityPublishResult ShellWindowIdentityStore::publish(
    const ShellWindowIdentityCandidate &candidate, QString *error)
{
    if (!candidate.actionGeneration.isValid()
        || candidate.actionGeneration.epoch != m_epoch
        || (candidate.activeWindow && !validFacts(*candidate.activeWindow))) {
        setError(error, QStringLiteral("active-window identity candidate is invalid"));
        return ShellWindowIdentityPublishResult::Rejected;
    }
    QJsonObject state{{QStringLiteral("actionRevision"),
                       QString::number(candidate.actionGeneration.revision)},
                      {QStringLiteral("activeWindow"), QJsonValue::Null}};
    if (candidate.activeWindow) {
        state.insert(QStringLiteral("activeWindow"), factsJson(*candidate.activeWindow));
    }
    const QByteArray canonical = QJsonDocument(state).toJson(QJsonDocument::Compact);
    if (m_available && canonical == m_canonicalState) {
        if (error) error->clear();
        return ShellWindowIdentityPublishResult::Unchanged;
    }
    if (m_revision == std::numeric_limits<quint64>::max()) {
        setError(error, QStringLiteral("active-window identity revision is exhausted"));
        return ShellWindowIdentityPublishResult::RevisionExhausted;
    }
    ++m_revision;
    state.insert(QStringLiteral("status"), QStringLiteral("ok"));
    state.insert(QStringLiteral("schemaVersion"), 1);
    state.insert(QStringLiteral("epoch"), m_epoch);
    state.insert(QStringLiteral("revision"), QString::number(m_revision));
    const QByteArray payload = QJsonDocument(state).toJson(QJsonDocument::Compact);
    if (payload.size() > MaximumPayloadBytes) {
        --m_revision;
        setError(error, QStringLiteral("active-window identity payload is too large"));
        return ShellWindowIdentityPublishResult::Rejected;
    }
    m_canonicalState = canonical;
    m_snapshotJson = payload;
    m_available = true;
    if (error) error->clear();
    return ShellWindowIdentityPublishResult::Published;
}

bool ShellWindowIdentityStore::markUnavailable(const QString &code,
                                               const QString &message)
{
    if (!m_available) {
        return false;
    }
    m_available = false;
    if (m_revision < std::numeric_limits<quint64>::max()) {
        ++m_revision;
    }
    m_snapshotJson = unavailableJson(m_epoch, m_revision, code, message);
    return true;
}

const QByteArray &ShellWindowIdentityStore::snapshotJson() const noexcept
{
    return m_snapshotJson;
}

const QString &ShellWindowIdentityStore::epoch() const noexcept
{
    return m_epoch;
}

quint64 ShellWindowIdentityStore::revision() const noexcept
{
    return m_revision;
}

ShellWindowIdentityController::ShellWindowIdentityController(
    ShellWindowCredentialSource &credentials,
    ShellPanelOwnerSource &panelOwner,
    ShellWindowIdentitySource &identitySource)
    : m_credentials(credentials)
    , m_panelOwner(panelOwner)
    , m_identitySource(identitySource)
{
}

bool ShellWindowIdentityController::authorized(
    const QString &callerUniqueName) const
{
    const auto panelPid = m_panelOwner.shellPanelProcessId();
    if (!panelPid || *panelPid <= 1 || callerUniqueName.isEmpty()
        || !callerUniqueName.startsWith(u':')) {
        return false;
    }
    const auto callerPid = m_credentials.processIdForUniqueName(callerUniqueName);
    return callerPid && *callerPid == *panelPid;
}

QByteArray ShellWindowIdentityController::snapshot(const QString &callerUniqueName)
{
    // AGENT-GUARD: The source can touch KWin window state. Authenticate first
    // so an arbitrary session-bus peer cannot use this method as an identity
    // or focus oracle.
    if (!authorized(callerUniqueName)) {
        return QJsonDocument(QJsonObject{
            {QStringLiteral("status"), QStringLiteral("unauthorized")},
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("failure"),
             QJsonObject{{QStringLiteral("code"),
                          QStringLiteral("caller-pid-mismatch")},
                         {QStringLiteral("message"),
                          QStringLiteral("the D-Bus caller does not own the shell panels")}}},
        }).toJson(QJsonDocument::Compact);
    }
    return m_identitySource.snapshotJson();
}

QByteArray encodeShellWindowIdentitySnapshot(
    const ShellWindowIdentitySnapshot &snapshot)
{
    if (snapshot.status == ShellWindowIdentityStatus::Unauthorized) {
        return QJsonDocument(QJsonObject{
            {QStringLiteral("status"), QStringLiteral("unauthorized")},
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("failure"),
             QJsonObject{{QStringLiteral("code"), snapshot.failureCode},
                         {QStringLiteral("message"), snapshot.message}}},
        }).toJson(QJsonDocument::Compact);
    }
    if (snapshot.status == ShellWindowIdentityStatus::Unavailable) {
        return unavailableJson(snapshot.epoch, snapshot.revision,
                               snapshot.failureCode, snapshot.message);
    }
    QJsonObject object{{QStringLiteral("status"), QStringLiteral("ok")},
                       {QStringLiteral("schemaVersion"), 1},
                       {QStringLiteral("epoch"), snapshot.epoch},
                       {QStringLiteral("revision"), QString::number(snapshot.revision)},
                       {QStringLiteral("actionRevision"),
                        QString::number(snapshot.actionGeneration.revision)},
                       {QStringLiteral("activeWindow"), QJsonValue::Null}};
    if (snapshot.activeWindow) {
        object.insert(QStringLiteral("activeWindow"), factsJson(*snapshot.activeWindow));
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<ShellWindowIdentitySnapshot> decodeShellWindowIdentitySnapshot(
    const QByteArray &payload, QString *error)
{
    if (payload.isEmpty() || payload.size() > MaximumPayloadBytes) {
        setError(error, QStringLiteral("active-window identity reply size is invalid"));
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("active-window identity reply is not an object"));
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("schemaVersion")).toInt(-1) != 1) {
        setError(error, QStringLiteral("active-window identity schema is unsupported"));
        return std::nullopt;
    }
    const QString status = object.value(QStringLiteral("status")).toString();
    const QJsonObject failure = object.value(QStringLiteral("failure")).toObject();
    if (status == QLatin1StringView("unauthorized")) {
        if (failure.value(QStringLiteral("code")).toString().isEmpty()
            || object.contains(QStringLiteral("epoch"))
            || object.contains(QStringLiteral("revision"))
            || object.contains(QStringLiteral("actionRevision"))
            || object.contains(QStringLiteral("activeWindow"))) {
            setError(error, QStringLiteral("unauthorized identity reply leaked facts"));
            return std::nullopt;
        }
        return ShellWindowIdentitySnapshot{
            ShellWindowIdentityStatus::Unauthorized, {}, 0, {}, std::nullopt,
            failure.value(QStringLiteral("code")).toString(),
            failure.value(QStringLiteral("message")).toString()};
    }
    quint64 revision = 0;
    const QString epoch = object.value(QStringLiteral("epoch")).toString();
    // AGENT-GUARD: Apply the public action-generation rule even to an
    // unavailable identity reply (whose identity revision may legitimately
    // be zero). A merely nonempty epoch is not a usable action fence.
    if (!ShellWindowGeneration{epoch, 1}.isValid()
        || !canonicalRevisionString(
            object.value(QStringLiteral("revision")).toString(), &revision)) {
        setError(error, QStringLiteral("active-window identity lineage is invalid"));
        return std::nullopt;
    }
    if (status == QLatin1StringView("unavailable")) {
        if (failure.value(QStringLiteral("code")).toString().isEmpty()
            || object.contains(QStringLiteral("actionRevision"))
            || object.contains(QStringLiteral("activeWindow"))) {
            setError(error, QStringLiteral("unavailable identity reply has no failure"));
            return std::nullopt;
        }
        return ShellWindowIdentitySnapshot{
            ShellWindowIdentityStatus::Unavailable, epoch, revision, {}, std::nullopt,
            failure.value(QStringLiteral("code")).toString(),
            failure.value(QStringLiteral("message")).toString()};
    }
    quint64 actionRevision = 0;
    if (status != QLatin1StringView("ok") || revision == 0
        || !canonicalRevisionString(
            object.value(QStringLiteral("actionRevision")).toString(),
            &actionRevision)
        || !ShellWindowGeneration{epoch, actionRevision}.isValid()) {
        setError(error, QStringLiteral("active-window identity status is invalid"));
        return std::nullopt;
    }
    std::optional<ShellWindowIdentityFacts> facts;
    const QJsonValue activeValue = object.value(QStringLiteral("activeWindow"));
    if (!activeValue.isNull()) {
        if (!activeValue.isObject()) {
            setError(error, QStringLiteral("activeWindow must be an object or null"));
            return std::nullopt;
        }
        const QJsonObject active = activeValue.toObject();
        bool optionalTypesOk = true;
        ShellWindowIdentityFacts decoded;
        decoded.windowId = active.value(QStringLiteral("windowId")).toString();
        const QJsonValue processValue = active.value(QStringLiteral("processId"));
        if (!processValue.isNull()) {
            quint64 processId = 0;
            if (!processValue.isString()
                || !canonicalRevisionString(processValue.toString(), &processId)
                || processId > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
                optionalTypesOk = false;
            } else {
                decoded.processId = static_cast<qint64>(processId);
            }
        }
        const QJsonValue menuIdValue = active.value(QStringLiteral("appMenuWindowId"));
        if (!menuIdValue.isNull()) {
            const qint64 menuId = menuIdValue.toInteger(-1);
            if (menuId <= 0 || menuId > std::numeric_limits<quint32>::max()) {
                optionalTypesOk = false;
            } else {
                decoded.appMenuWindowId = static_cast<quint32>(menuId);
            }
        }
        decoded.appMenuServiceName = optionalJsonString(
            active, QStringLiteral("appMenuServiceName"), &optionalTypesOk);
        decoded.appMenuObjectPath = optionalJsonString(
            active, QStringLiteral("appMenuObjectPath"), &optionalTypesOk);
        if (!optionalTypesOk || !validFacts(decoded)) {
            setError(error, QStringLiteral("active-window identity facts are invalid"));
            return std::nullopt;
        }
        facts = std::move(decoded);
    }
    if (error) error->clear();
    return ShellWindowIdentitySnapshot{ShellWindowIdentityStatus::Ok,
                                       epoch, revision, {epoch, actionRevision},
                                       std::move(facts), {}, {}};
}

} // namespace QindaQt::Compositor
