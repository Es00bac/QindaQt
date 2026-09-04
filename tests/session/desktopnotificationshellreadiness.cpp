// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopnotificationshellreadiness.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSet>

#include <utility>

namespace QindaQt::Test {
namespace {

DesktopNotificationShellCheck check(
    DesktopNotificationShellDisposition disposition, QString code,
    QString message, QJsonObject evidence = {})
{
    return {disposition, std::move(code), std::move(message),
            std::move(evidence)};
}

bool canonicalPositiveId(const QJsonValue &value, qint64 *result)
{
    if (!value.isString()) {
        return false;
    }
    bool converted = false;
    const qint64 parsed = value.toString().toLongLong(&converted);
    if (!converted || parsed <= 1 || value.toString() != QString::number(parsed)) {
        return false;
    }
    *result = parsed;
    return true;
}

bool canonicalCounter(const QJsonValue &value, quint64 *result)
{
    if (!value.isString()) {
        return false;
    }
    bool converted = false;
    const quint64 parsed = value.toString().toULongLong(&converted);
    if (!converted || value.toString() != QString::number(parsed)) {
        return false;
    }
    *result = parsed;
    return true;
}

bool exactIntegerRectShape(const QJsonObject &geometry)
{
    const QSet<QString> expected{QStringLiteral("x"), QStringLiteral("y"),
                                 QStringLiteral("width"), QStringLiteral("height")};
    QSet<QString> observed;
    for (auto iterator = geometry.constBegin(); iterator != geometry.constEnd(); ++iterator) {
        observed.insert(iterator.key());
    }
    if (observed != expected) {
        return false;
    }
    for (const auto *key : {"x", "y", "width", "height"}) {
        const QJsonValue coordinate = geometry.value(QString::fromLatin1(key));
        if (!coordinate.isDouble()
            || coordinate.toDouble() != coordinate.toInt()) {
            return false;
        }
    }
    return true;
}

bool positiveIntegerRect(const QJsonObject &geometry)
{ return exactIntegerRectShape(geometry)
        && geometry.value(QStringLiteral("width")).toInt() > 0
        && geometry.value(QStringLiteral("height")).toInt() > 0; }

bool exactColdEnvelope(const QJsonObject &envelope)
{
    const QJsonObject failure = envelope.value(QStringLiteral("failure")).toObject();
    return envelope.size() == 2 && failure.size() == 2
        && envelope.value(QStringLiteral("status")) == QStringLiteral("unavailable")
        && failure.value(QStringLiteral("code")) == QStringLiteral("service-not-ready")
        && failure.value(QStringLiteral("message")).isString()
        && !failure.value(QStringLiteral("message")).toString().isEmpty();
}

bool validReadyTokens(const QJsonObject &tokens)
{
    quint64 generation = 0;
    const QString sourceTheme =
        tokens.value(QStringLiteral("sourceThemeId")).toString();
    const QString backgroundBase =
        tokens.value(QStringLiteral("backgroundBase")).toString();
    const QColor backgroundColor(backgroundBase);
    return tokens.size() == 5
        && tokens.value(QStringLiteral("ready")) == QJsonValue(true)
        && tokens.value(QStringLiteral("qstRevision")).toInt(-1) == 1
        && canonicalCounter(tokens.value(QStringLiteral("generation")),
                            &generation)
        && generation > 0 && !sourceTheme.isEmpty()
        && backgroundColor.isValid()
        && backgroundBase == backgroundColor.name(QColor::HexRgb);
}

QJsonObject normalizedEvidence(
    const DesktopNotificationShellObservation &observation,
    qint64 shellProcessId, bool privatePresentationAllowed, bool centerOpen,
    quint64 centerOpenedCount, const QJsonObject &center,
    const QJsonObject &tokens)
{
    QJsonObject centerEvidence{{QStringLiteral("exists"),
                                center.value(QStringLiteral("exists"))}};
    if (center.value(QStringLiteral("exists")).toBool()) {
        centerEvidence.insert(QStringLiteral("visible"), center.value(QStringLiteral("visible")));
        centerEvidence.insert(QStringLiteral("outputName"), center.value(QStringLiteral("outputName")));
    }
    return {
        {QStringLiteral("owner"), observation.owner},
        {QStringLiteral("servicePid"), QString::number(observation.serviceProcessId)},
        {QStringLiteral("shellPid"), QString::number(shellProcessId)},
        {QStringLiteral("tokens"), tokens},
        {QStringLiteral("presentation"),
         QJsonObject{
             {QStringLiteral("privatePresentationAllowed"), privatePresentationAllowed},
             {QStringLiteral("centerOpen"), centerOpen},
         }},
        {QStringLiteral("centerOpenedCount"), QString::number(centerOpenedCount)},
        {QStringLiteral("centerWindow"), centerEvidence},
    };
}

} // namespace

bool DesktopNotificationShellCheck::ready() const noexcept
{ return disposition == DesktopNotificationShellDisposition::Ready; }

QJsonObject DesktopNotificationShellCheck::document() const
{
    QString status;
    switch (disposition) {
    case DesktopNotificationShellDisposition::Ready:
        status = QStringLiteral("ok");
        break;
    case DesktopNotificationShellDisposition::Pending:
        status = QStringLiteral("pending");
        break;
    case DesktopNotificationShellDisposition::Invalid:
        status = QStringLiteral("unavailable");
        break;
    }
    QJsonObject result{{QStringLiteral("status"), status}};
    if (!evidence.isEmpty()) {
        result.insert(QStringLiteral("evidence"), evidence);
    }
    if (!ready()) {
        result.insert(
            QStringLiteral("failure"),
            QJsonObject{{QStringLiteral("code"), code},
                        {QStringLiteral("message"), message}});
    }
    return result;
}

DesktopNotificationShellCheck validateDesktopNotificationShell(
    const DesktopNotificationShellObservation &observation,
    const DesktopNotificationShellExpectation &expectation)
{
    const auto invalid = [](QString code, QString message,
                            QJsonObject evidence = {}) {
        return check(DesktopNotificationShellDisposition::Invalid,
                     std::move(code), std::move(message), std::move(evidence));
    };
    const auto pending = [](QString code, QString message,
                            QJsonObject evidence = {}) {
        return check(DesktopNotificationShellDisposition::Pending,
                     std::move(code), std::move(message), std::move(evidence));
    };
    if (!observation.serviceOwnerReplyValid) {
        if (observation.serviceOwnerReplyErrorName
            == QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner")) {
            return pending(QStringLiteral("service-missing"),
                           QStringLiteral("ShellDevelopment is not owned yet"));
        }
        return invalid(QStringLiteral("service-owner-query-failed"),
                       observation.serviceOwnerReplyError.isEmpty()
                           ? QStringLiteral("ShellDevelopment owner query failed")
                           : observation.serviceOwnerReplyError);
    }
    if (expectation.dockProcessId <= 1 || expectation.outputName.isEmpty()) {
        return invalid(QStringLiteral("invalid-expectation"),
                       QStringLiteral("shell PID or selected output is invalid"));
    }
    if (observation.owner.isEmpty()) {
        return invalid(QStringLiteral("service-owner-empty"),
                       QStringLiteral("ShellDevelopment owner query returned no owner"));
    }
    if (!observation.owner.startsWith(QLatin1Char(':'))
        || observation.ownerAfterSnapshot != observation.owner
        || observation.serviceProcessId <= 1) {
        return invalid(QStringLiteral("unstable-owner"),
                       QStringLiteral("ShellDevelopment owner changed during Snapshot"));
    }
    if (observation.serviceProcessId != expectation.dockProcessId) {
        return invalid(QStringLiteral("owner-pid-mismatch"),
                       QStringLiteral("ShellDevelopment PID does not own the dock"));
    }
    if (!expectation.requiredUniqueOwner.isEmpty()
        && observation.owner != expectation.requiredUniqueOwner) {
        return invalid(QStringLiteral("owner-replaced"),
                       QStringLiteral("ShellDevelopment owner changed across input"));
    }
    if (!observation.snapshotReplyValid) {
        if (observation.replyErrorName
            == QStringLiteral("org.freedesktop.DBus.Error.UnknownObject")) {
            return pending(QStringLiteral("snapshot-object-pending"),
                           QStringLiteral("ShellDevelopment object is not registered yet"));
        }
        return invalid(QStringLiteral("snapshot-call-failed"),
                       observation.replyError.isEmpty()
                           ? QStringLiteral("ShellDevelopment Snapshot failed")
                           : observation.replyError);
    }
    const QJsonObject &snapshot = observation.snapshot;
    if (snapshot.value(QStringLiteral("schemaVersion")).toInt(-1) != 1) {
        return invalid(QStringLiteral("invalid-schema"),
                       QStringLiteral("ShellDevelopment returned invalid schema"));
    }
    qint64 shellProcessId = 0;
    if (!canonicalPositiveId(snapshot.value(QStringLiteral("shellPid")),
                             &shellProcessId)
        || shellProcessId != observation.serviceProcessId) {
        return invalid(QStringLiteral("snapshot-pid-mismatch"),
                       QStringLiteral("ShellDevelopment snapshot PID changed"));
    }
    const QJsonValue presentationValue =
        snapshot.value(QStringLiteral("presentation"));
    const QJsonValue tokensValue = snapshot.value(QStringLiteral("tokens"));
    const QJsonValue windowsValue = snapshot.value(QStringLiteral("windows"));
    const QJsonValue observationsValue =
        snapshot.value(QStringLiteral("observations"));
    if (!presentationValue.isObject() || !tokensValue.isObject()
        || !windowsValue.isObject()
        || !observationsValue.isObject()) {
        return invalid(QStringLiteral("invalid-shape"),
                       QStringLiteral("ShellDevelopment snapshot shape is invalid"));
    }
    const QJsonObject tokens = tokensValue.toObject();
    if (!validReadyTokens(tokens)) {
        return invalid(QStringLiteral("tokens-not-ready"),
                       QStringLiteral("ShellDevelopment tokens are not ready"));
    }
    const QJsonObject presentation = presentationValue.toObject();
    const QJsonValue privacyValue =
        presentation.value(QStringLiteral("privatePresentationAllowed"));
    const QJsonValue centerOpenValue =
        presentation.value(QStringLiteral("centerOpen"));
    quint64 centerOpenedCount = 0;
    if (!privacyValue.isBool() || !centerOpenValue.isBool()
        || !canonicalCounter(
            observationsValue.toObject().value(
                QStringLiteral("centerOpenedCount")),
            &centerOpenedCount)) {
        return invalid(QStringLiteral("invalid-presentation"),
                       QStringLiteral("ShellDevelopment presentation evidence is invalid"));
    }
    const bool privatePresentationAllowed = privacyValue.toBool();
    const bool centerOpen = centerOpenValue.toBool();
    const QJsonValue centerValue =
        windowsValue.toObject().value(QStringLiteral("center"));
    if (!centerValue.isObject()) {
        return invalid(QStringLiteral("invalid-center-window"),
                       QStringLiteral("center window evidence is invalid"));
    }
    const QJsonObject center = centerValue.toObject();
    const QJsonValue existsValue = center.value(QStringLiteral("exists"));
    if (!existsValue.isBool()) {
        return invalid(QStringLiteral("invalid-center-window"),
                       QStringLiteral("center window existence is invalid"));
    }
    const QJsonObject evidence = normalizedEvidence(
        observation, shellProcessId, privatePresentationAllowed, centerOpen,
        centerOpenedCount, center, tokens);
    if (expectation.phase == DesktopNotificationShellPhase::ClosedHidden
        && centerOpen) {
        return invalid(QStringLiteral("center-preopened"),
                       QStringLiteral("notification center was open before input"),
                       evidence);
    }
    if (!existsValue.toBool()) {
        return pending(QStringLiteral("center-window-missing"),
                       QStringLiteral("notification center window is not created yet"),
                       evidence);
    }
    const QJsonValue visibleValue = center.value(QStringLiteral("visible"));
    const QJsonValue outputValue = center.value(QStringLiteral("outputName"));
    const QJsonValue geometryValue = center.value(QStringLiteral("geometry"));
    if (!visibleValue.isBool() || !outputValue.isString()
        || !geometryValue.isObject()) {
        return invalid(QStringLiteral("invalid-center-window"),
                       QStringLiteral("center window presentation is invalid"));
    }
    const QJsonObject geometry = geometryValue.toObject();
    if (!positiveIntegerRect(geometry)) {
        return invalid(QStringLiteral("invalid-center-geometry"),
                       QStringLiteral("center window geometry is invalid"));
    }
    const bool visible = visibleValue.toBool();
    if (expectation.phase == DesktopNotificationShellPhase::ClosedHidden
        && visible) {
        return invalid(QStringLiteral("center-visible-before-input"),
                       QStringLiteral("notification center was visible before input"),
                       evidence);
    }
    if (outputValue.toString() != expectation.outputName) {
        return pending(QStringLiteral("center-output-pending"),
                       QStringLiteral("center window has not joined the selected output"),
                       evidence);
    }
    if (!privatePresentationAllowed) {
        return check(
            expectation.phase == DesktopNotificationShellPhase::ClosedHidden
                ? DesktopNotificationShellDisposition::Pending
                : DesktopNotificationShellDisposition::Invalid,
            QStringLiteral("privacy-denied"),
            QStringLiteral("private notification presentation is not allowed"),
            evidence);
    }
    if (expectation.phase == DesktopNotificationShellPhase::OpenVisible
        && (!centerOpen || !visible
            || centerOpenedCount <= expectation.centerOpenedCountBefore)) {
        return pending(QStringLiteral("center-open-pending"),
                       QStringLiteral("notification center presentation has not completed"),
                       evidence);
    }

    return check(DesktopNotificationShellDisposition::Ready,
                 QStringLiteral("ready"), QString{},
                 evidence);
}

bool DesktopNotificationShellExpectationCheck::ready() const noexcept
{ return disposition == DesktopNotificationShellDisposition::Ready
      && expectation.has_value(); }

QJsonObject DesktopNotificationShellExpectationCheck::document() const
{
    return {
        {QStringLiteral("status"),
         disposition == DesktopNotificationShellDisposition::Pending
             ? QStringLiteral("pending")
             : QStringLiteral("unavailable")},
        {QStringLiteral("failure"),
         QJsonObject{{QStringLiteral("code"), code},
                     {QStringLiteral("message"), message}}},
    };
}

DesktopNotificationShellExpectationCheck
desktopNotificationShellExpectation(
    const QJsonObject &developmentShellSurfaces, const QJsonObject &outputs,
    const QString &requestedOutput, DesktopNotificationShellPhase phase,
    quint64 centerOpenedCountBefore, QString *error)
{
    error->clear();
    const auto result = [&](DesktopNotificationShellDisposition disposition,
                            QString code, QString message) {
        *error = message;
        return DesktopNotificationShellExpectationCheck{
            disposition, std::nullopt, std::move(code), std::move(message)};
    };
    const QJsonValue surfacesValue = developmentShellSurfaces.value(QStringLiteral("surfaces"));
    const QJsonValue outputsValue = outputs.value(QStringLiteral("outputs"));
    const QJsonValue surfaceStatus = developmentShellSurfaces.value(QStringLiteral("status"));
    const QJsonValue outputStatus = outputs.value(QStringLiteral("status"));
    const bool surfaceCold = exactColdEnvelope(developmentShellSurfaces), outputCold = exactColdEnvelope(outputs);
    const bool surfaceReady = surfaceStatus == QStringLiteral("ok") && surfacesValue.isArray();
    const bool outputReady = outputStatus == QStringLiteral("ok") && outputsValue.isArray();
    if ((!surfaceCold && !surfaceReady) || (!outputCold && !outputReady)) {
        return result(DesktopNotificationShellDisposition::Invalid,
                      QStringLiteral("public-topology-invalid"), QStringLiteral("public shell/output evidence is malformed"));
    }
    if (surfaceCold || outputCold) {
        return result(DesktopNotificationShellDisposition::Pending,
                      QStringLiteral("public-topology-pending"), QStringLiteral("public shell/output evidence is not ready"));
    }

    QString outputName;
    int requestedMatches = 0;
    QSet<QString> outputNames;
    const QJsonArray outputArray = outputsValue.toArray();
    if (outputArray.isEmpty()) {
        return result(DesktopNotificationShellDisposition::Pending,
                      QStringLiteral("output-pending"), QStringLiteral("no selected output is available yet"));
    }
    for (qsizetype index = 0; index < outputArray.size(); ++index) {
        if (!outputArray.at(index).isObject()) {
            return result(DesktopNotificationShellDisposition::Invalid,
                          QStringLiteral("output-shape-invalid"), QStringLiteral("output evidence is malformed"));
        }
        const QJsonObject output = outputArray.at(index).toObject();
        const QString name = output.value(QStringLiteral("name")).toString();
        if (name.isEmpty() || outputNames.contains(name)) {
            return result(DesktopNotificationShellDisposition::Invalid,
                          QStringLiteral("output-name-invalid"), QStringLiteral("output name is invalid or duplicated"));
        }
        outputNames.insert(name);
        if (index == 0 && requestedOutput.isEmpty()) {
            outputName = name;
        }
        if (!requestedOutput.isEmpty() && name == requestedOutput) {
            outputName = name;
            ++requestedMatches;
        }
    }
    if (outputName.isEmpty()
        || (!requestedOutput.isEmpty() && requestedMatches != 1)) {
        return result(DesktopNotificationShellDisposition::Pending,
                      QStringLiteral("selected-output-pending"), QStringLiteral("selected notification output is unavailable"));
    }

    qint64 dockProcessId = 0; int readyDockCount = 0; bool unsettledDock = false;
    for (const QJsonValue &value : surfacesValue.toArray()) {
        if (!value.isObject()) {
            return result(DesktopNotificationShellDisposition::Invalid,
                          QStringLiteral("surface-shape-invalid"), QStringLiteral("shell surface evidence is malformed"));
        }
        const QJsonObject surface = value.toObject();
        if (surface.value(QStringLiteral("scope")) != QStringLiteral("dock")) {
            continue;
        }
        const QJsonValue mapped = surface.value(QStringLiteral("mapped"));
        const QJsonValue committed = surface.value(QStringLiteral("committed"));
        const QJsonValue geometryValue = surface.value(QStringLiteral("geometry"));
        const QString currentOutput =
            surface.value(QStringLiteral("outputName")).toString();
        const QString desiredOutput =
            surface.value(QStringLiteral("desiredOutputName")).toString();
        if (!mapped.isBool() || !committed.isBool() || !geometryValue.isObject()
            || currentOutput.isEmpty() || desiredOutput.isEmpty()) {
            return result(DesktopNotificationShellDisposition::Invalid,
                          QStringLiteral("dock-shape-invalid"), QStringLiteral("dock readiness evidence is malformed"));
        }
        if (!outputNames.contains(currentOutput)
            || !outputNames.contains(desiredOutput)) {
            return result(DesktopNotificationShellDisposition::Invalid,
                          QStringLiteral("dock-output-foreign"), QStringLiteral("dock names an output outside Outputs"));
        }
        const QJsonObject geometry = geometryValue.toObject();
        if (!exactIntegerRectShape(geometry)) {
            return result(DesktopNotificationShellDisposition::Invalid,
                          QStringLiteral("dock-geometry-invalid"), QStringLiteral("dock geometry is malformed"));
        }
        qint64 processId = 0;
        if (!canonicalPositiveId(surface.value(QStringLiteral("processId")),
                                 &processId)) {
            return result(DesktopNotificationShellDisposition::Invalid,
                          QStringLiteral("dock-owner-invalid"), QStringLiteral("dock process ID is invalid"));
        }
        if (dockProcessId != 0 && dockProcessId != processId) {
            return result(DesktopNotificationShellDisposition::Invalid,
                          QStringLiteral("dock-owner-ambiguous"), QStringLiteral("dock surfaces have multiple owners"));
        }
        dockProcessId = processId;
        if (!mapped.toBool() || !committed.toBool()
            || currentOutput != desiredOutput
            || geometry.value(QStringLiteral("width")).toInt() <= 0
            || geometry.value(QStringLiteral("height")).toInt() <= 0) {
            unsettledDock = true;
            continue;
        }
        ++readyDockCount;
    }
    if (readyDockCount == 0 || unsettledDock) {
        return result(DesktopNotificationShellDisposition::Pending,
                      QStringLiteral("dock-owner-pending"), QStringLiteral("no settled dock owner is available yet"));
    }
    return {
        DesktopNotificationShellDisposition::Ready,
        DesktopNotificationShellExpectation{
            dockProcessId, outputName, {}, phase, centerOpenedCountBefore},
        QStringLiteral("ready"),
        {},
    };
}

} // namespace QindaQt::Test
