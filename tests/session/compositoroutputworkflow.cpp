// SPDX-License-Identifier: GPL-3.0-or-later
#include "compositoroutputworkflow.h"

#include "compositorprobeclient.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QThread>

#include <limits>

namespace QindaQt::Test {
namespace {

std::optional<quint64> generation(const QJsonObject &snapshot)
{
    const auto value = snapshot.value(QStringLiteral("outputGeneration"));
    if (!value.isString() || value.toString().isEmpty()
        || (value.toString().size() > 1
            && value.toString().startsWith(QLatin1Char('0')))) {
        return std::nullopt;
    }
    bool ok = false;
    const auto parsed = value.toString().toULongLong(&ok);
    return ok && parsed > 0 ? std::optional<quint64>(parsed) : std::nullopt;
}

QSet<QString> outputNames(const QJsonArray &outputs, QLatin1StringView field)
{
    QSet<QString> names;
    for (const auto &value : outputs) {
        if (value.isObject()) {
            names.insert(value.toObject().value(field).toString());
        }
    }
    return names;
}

bool awaitCoherentGeneration(CompositorProbeClient &client,
                             quint64 expectedGeneration,
                             qsizetype expectedCount,
                             const QString &changedName,
                             bool namePresent,
                             int expectedSignalCount,
                             QJsonObject *acceptedOutputs,
                             QString *error)
{
    QElapsedTimer timer;
    timer.start();
    QString lastState;
    while (timer.elapsed() < 3000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QString callError;
        const auto outputs = client.call(QStringLiteral("Outputs"), &callError);
        const auto visibility = client.call(
            QStringLiteral("ShellVisibilitySnapshot"), &callError);
        if (outputs && visibility) {
            const auto outputGeneration = generation(*outputs);
            const auto visibilityGeneration = generation(*visibility);
            const auto outputArray = outputs->value(QStringLiteral("outputs")).toArray();
            const auto visibilityArray = visibility->value(QStringLiteral("outputs")).toArray();
            const auto names = outputNames(outputArray, QLatin1StringView("name"));
            const auto visibilityNames = outputNames(
                visibilityArray, QLatin1StringView("id"));
            if (outputs->value(QStringLiteral("status")) == QStringLiteral("ok")
                && visibility->value(QStringLiteral("status")) == QStringLiteral("ok")
                && outputGeneration == expectedGeneration
                && visibilityGeneration == expectedGeneration
                && outputArray.size() == expectedCount
                && names.size() == expectedCount && names == visibilityNames
                && names.contains(changedName) == namePresent
                && client.outputsChangedCount() == expectedSignalCount) {
                *acceptedOutputs = *outputs;
                error->clear();
                return true;
            }
            lastState = QStringLiteral("outputs=%1 visibility=%2 signals=%3")
                            .arg(QString::fromUtf8(QJsonDocument(*outputs).toJson(
                                     QJsonDocument::Compact)),
                                 QString::fromUtf8(QJsonDocument(*visibility).toJson(
                                     QJsonDocument::Compact)))
                            .arg(client.outputsChangedCount());
        } else if (!callError.isEmpty()) {
            lastState = callError;
        }
        QThread::msleep(10);
    }
    *error = QStringLiteral("output inventories did not converge: %1").arg(lastState);
    return false;
}

bool rejectedWithDisabled(const std::optional<QJsonObject> &reply)
{
    return reply && reply->value(QStringLiteral("status")) == QStringLiteral("rejected")
        && reply->value(QStringLiteral("failure")).toObject()
               .value(QStringLiteral("code")) == QStringLiteral("control-disabled");
}

} // namespace

std::optional<QJsonObject>
exerciseDevelopmentOutputHotplug(CompositorProbeClient &client, QString *error)
{
    const auto initial = client.call(QStringLiteral("Outputs"), error);
    const auto initialGeneration = initial ? generation(*initial) : std::nullopt;
    const auto initialOutputs = initial
        ? initial->value(QStringLiteral("outputs")).toArray() : QJsonArray{};
    if (!initialGeneration || initialOutputs.isEmpty()
        || *initialGeneration >= std::numeric_limits<quint64>::max() - 1) {
        *error = QStringLiteral("initial output generation is invalid");
        return std::nullopt;
    }

    constexpr auto RequestName = "qindaqt-hotplug";
    const QString connectorName = QStringLiteral("Virtual-%1")
                                      .arg(QString::fromLatin1(RequestName));
    const int signalBaseline = client.outputsChangedCount();
    const auto added = client.callWithArguments(
        QStringLiteral("AddVirtualOutputForTest"),
        {QString::fromLatin1(RequestName), 1280, 720, 1.25}, error);
    QJsonObject addedOutputs;
    if (!added || added->value(QStringLiteral("status")) != QStringLiteral("added")
        || !awaitCoherentGeneration(client, *initialGeneration + 1,
                                    initialOutputs.size() + 1, connectorName, true,
                                    signalBaseline + 1,
                                    &addedOutputs, error)
        || client.outputsChangedCount() != signalBaseline + 1) {
        if (error->isEmpty()) {
            *error = QStringLiteral("virtual output add did not publish one invalidation");
        }
        return std::nullopt;
    }

    const auto removed = client.call(
        QStringLiteral("RemoveVirtualOutputForTest"),
        QString::fromLatin1(RequestName), error);
    QJsonObject restoredOutputs;
    if (!removed || removed->value(QStringLiteral("status")) != QStringLiteral("removed")
        || !awaitCoherentGeneration(client, *initialGeneration + 2,
                                    initialOutputs.size(), connectorName, false,
                                    signalBaseline + 2,
                                    &restoredOutputs, error)
        || client.outputsChangedCount() != signalBaseline + 2) {
        if (error->isEmpty()) {
            *error = QStringLiteral("virtual output removal did not publish one invalidation");
        }
        return std::nullopt;
    }
    return QJsonObject{
        {QStringLiteral("virtualOutputHotplug"), true},
        {QStringLiteral("initialOutputGeneration"),
         QString::number(*initialGeneration)},
        {QStringLiteral("addedOutputGeneration"),
         QString::number(*initialGeneration + 1)},
        {QStringLiteral("removedOutputGeneration"),
         QString::number(*initialGeneration + 2)},
        {QStringLiteral("outputsChangedCount"), 2},
        {QStringLiteral("sharedVisibilityGeneration"), true},
    };
}

std::optional<QJsonObject>
exerciseProductionOutputGate(CompositorProbeClient &client, QString *error)
{
    const auto before = client.call(QStringLiteral("Outputs"), error);
    const int signalBaseline = client.outputsChangedCount();
    const auto hostile = client.callWithArguments(
        QStringLiteral("AddVirtualOutputForTest"),
        {QStringLiteral("bad name"), -1, -1,
         std::numeric_limits<double>::quiet_NaN()}, error);
    const auto valid = client.callWithArguments(
        QStringLiteral("AddVirtualOutputForTest"),
        {QStringLiteral("qindaqt-gate-probe"), 1280, 720, 1.25}, error);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    const auto after = client.call(QStringLiteral("Outputs"), error);
    if (!before || !after || !rejectedWithDisabled(hostile)
        || hostile != valid || *before != *after
        || client.outputsChangedCount() != signalBaseline) {
        if (error->isEmpty()) {
            *error = QStringLiteral("production output gate parsed or mutated a request");
        }
        return std::nullopt;
    }
    return QJsonObject{{QStringLiteral("outputRequestsRejectedBeforeParsing"), true},
                       {QStringLiteral("outputInventoryUnchanged"), true}};
}

} // namespace QindaQt::Test
