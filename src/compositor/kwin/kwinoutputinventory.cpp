// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinoutputinventory.h"

#include <core/backendoutput.h>
#include <core/output.h>
#include <core/outputbackend.h>
#include <workspace.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringView>
#include <QTimer>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

bool safeText(const QString &value,
              qsizetype maximumUtf8Bytes,
              qsizetype maximumCharacters,
              bool required)
{
    if ((required && value.isEmpty()) || value.size() > maximumCharacters
        || value.toUtf8().size() > maximumUtf8Bytes
        || value.contains(QChar::Null)) {
        return false;
    }
    for (const QChar character : value) {
        if (character.category() == QChar::Other_Control
            || character.category() == QChar::Other_Format) {
            return false;
        }
    }
    const QStringView view(value);
    for (qsizetype index = 0; index < view.size(); ++index) {
        if (view[index].isHighSurrogate()) {
            if (index + 1 >= view.size() || !view[index + 1].isLowSurrogate()) {
                return false;
            }
            ++index;
        } else if (view[index].isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

bool validGeometry(const QRectF &geometry)
{
    const auto bounded = [](qreal value) {
        return std::isfinite(value)
            && std::abs(value) <= OutputInventoryStore::MaxLogicalCoordinateMagnitude;
    };
    return geometry.isValid() && bounded(geometry.x()) && bounded(geometry.y())
        && bounded(geometry.width()) && bounded(geometry.height())
        && bounded(geometry.right()) && bounded(geometry.bottom());
}

bool validVisibilityGeometry(const QRect &geometry)
{
    return geometry.isValid() && validGeometry(QRectF(geometry));
}

QJsonObject geometryJson(const QRectF &geometry)
{
    return {{QStringLiteral("x"), geometry.x()},
            {QStringLiteral("y"), geometry.y()},
            {QStringLiteral("width"), geometry.width()},
            {QStringLiteral("height"), geometry.height()}};
}

QJsonObject sizeJson(const QSize &size)
{
    return {{QStringLiteral("width"), size.width()},
            {QStringLiteral("height"), size.height()}};
}

bool validTransform(const QString &transform)
{
    static const QSet<QString> transforms{
        QStringLiteral("normal"), QStringLiteral("rotate-90"),
        QStringLiteral("rotate-180"), QStringLiteral("rotate-270"),
        QStringLiteral("flip-x"), QStringLiteral("flip-x-90"),
        QStringLiteral("flip-x-180"), QStringLiteral("flip-x-270"),
    };
    return transforms.contains(transform);
}

QString transformName(KWin::OutputTransform::Kind transform)
{
    switch (transform) {
    case KWin::OutputTransform::Normal:
        return QStringLiteral("normal");
    case KWin::OutputTransform::Rotate90:
        return QStringLiteral("rotate-90");
    case KWin::OutputTransform::Rotate180:
        return QStringLiteral("rotate-180");
    case KWin::OutputTransform::Rotate270:
        return QStringLiteral("rotate-270");
    case KWin::OutputTransform::FlipX:
        return QStringLiteral("flip-x");
    case KWin::OutputTransform::FlipX90:
        return QStringLiteral("flip-x-90");
    case KWin::OutputTransform::FlipX180:
        return QStringLiteral("flip-x-180");
    case KWin::OutputTransform::FlipX270:
        return QStringLiteral("flip-x-270");
    }
    Q_UNREACHABLE_RETURN(QStringLiteral("normal"));
}

QByteArray unavailableResponse(quint64 generation)
{
    return QJsonDocument(QJsonObject{
                             {QStringLiteral("status"), QStringLiteral("unavailable")},
                             {QStringLiteral("schemaVersion"), 1},
                             {QStringLiteral("outputGeneration"),
                              QString::number(generation)},
                             {QStringLiteral("outputs"), QJsonArray{}},
                             {QStringLiteral("failure"),
                              QJsonObject{{QStringLiteral("code"),
                                           QStringLiteral("output-inventory-unavailable")},
                                          {QStringLiteral("message"),
                                           QStringLiteral("no valid output inventory has been published")}}},
                         })
        .toJson(QJsonDocument::Compact);
}

} // namespace

OutputInventoryStore::OutputInventoryStore(OutputGenerationSeed seed)
    : m_responseJson(unavailableResponse(seed.value))
    , m_generation(seed.value)
{
}

OutputInventoryPublishResult OutputInventoryStore::publish(
    const QVector<OutputInventoryEntry> &candidate,
    QString *error)
{
    if (candidate.isEmpty() || candidate.size() > MaxOutputs) {
        fail(error, QStringLiteral("output count is outside the inventory limit"));
        return OutputInventoryPublishResult::Rejected;
    }

    QSet<QString> names;
    QSet<QString> uuids;
    QJsonArray outputs;
    for (const auto &output : candidate) {
        const bool physicalSizeValid = output.physicalSizeMillimeters.width() >= 0
            && output.physicalSizeMillimeters.height() >= 0
            && output.physicalSizeMillimeters.width()
                <= MaxPhysicalDimensionMillimeters
            && output.physicalSizeMillimeters.height()
                <= MaxPhysicalDimensionMillimeters;
        const bool scalarValuesValid = validGeometry(output.geometry)
            && validVisibilityGeometry(output.visibilityGeometry)
            && std::isfinite(output.scale) && output.scale > 0.0
            && output.scale <= MaximumScale && output.refreshRateMilliHz > 0
            && physicalSizeValid;
        const bool textValuesValid = safeText(
                                         output.name, MaxNameUtf8Bytes,
                                         MaxNameCharacters, true)
            && safeText(output.uuid, MaxUuidUtf8Bytes, MaxUuidUtf8Bytes, false)
            && safeText(output.manufacturer, MaxManufacturerUtf8Bytes,
                        MaxManufacturerUtf8Bytes, false)
            && safeText(output.model, MaxModelUtf8Bytes,
                        MaxModelUtf8Bytes, false)
            && validTransform(output.transform);
        if (!scalarValuesValid || !textValuesValid || names.contains(output.name)
            || (!output.uuid.isEmpty() && uuids.contains(output.uuid))) {
            fail(error, QStringLiteral("an output is malformed or has ambiguous identity"));
            return OutputInventoryPublishResult::Rejected;
        }
        names.insert(output.name);
        if (!output.uuid.isEmpty()) {
            uuids.insert(output.uuid);
        }
        outputs.append(QJsonObject{
            {QStringLiteral("name"), output.name},
            {QStringLiteral("geometry"), geometryJson(output.geometry)},
            {QStringLiteral("scale"), output.scale},
            {QStringLiteral("refreshRateMilliHz"),
             static_cast<qint64>(output.refreshRateMilliHz)},
            {QStringLiteral("transform"), output.transform},
            {QStringLiteral("enabled"), output.enabled},
            {QStringLiteral("internal"), output.internal},
            {QStringLiteral("uuid"), output.uuid},
            {QStringLiteral("priority"), static_cast<qint64>(output.priority)},
            {QStringLiteral("physicalSizeMm"),
             sizeJson(output.physicalSizeMillimeters)},
            {QStringLiteral("manufacturer"), output.manufacturer},
            {QStringLiteral("model"), output.model},
        });
    }

    if (m_available && candidate == m_entries) {
        if (error) {
            error->clear();
        }
        return OutputInventoryPublishResult::Unchanged;
    }
    if (m_generation == std::numeric_limits<quint64>::max()) {
        fail(error, QStringLiteral("output generation is exhausted"));
        return OutputInventoryPublishResult::GenerationExhausted;
    }

    const quint64 nextGeneration = m_generation + 1;
    const auto response = QJsonDocument(QJsonObject{
                                            {QStringLiteral("status"),
                                             QStringLiteral("ok")},
                                            {QStringLiteral("schemaVersion"), 1},
                                            {QStringLiteral("outputGeneration"),
                                             QString::number(nextGeneration)},
                                            {QStringLiteral("outputs"), outputs},
                                        })
                              .toJson(QJsonDocument::Compact);
    m_generation = nextGeneration;
    m_entries = candidate;
    m_responseJson = response;
    m_available = true;
    if (error) {
        error->clear();
    }
    return OutputInventoryPublishResult::Published;
}

const QByteArray &OutputInventoryStore::responseJson() const noexcept
{
    return m_responseJson;
}

const QVector<OutputInventoryEntry> &OutputInventoryStore::entries() const noexcept
{
    return m_entries;
}

quint64 OutputInventoryStore::generation() const noexcept
{
    return m_generation;
}

bool OutputInventoryStore::available() const noexcept
{
    return m_available;
}

KWinOutputInventory::KWinOutputInventory(QObject *parent)
    : QObject(parent)
{
    auto *const compositorWorkspace = KWin::workspace();
    Q_ASSERT(compositorWorkspace);
    connect(compositorWorkspace, &KWin::Workspace::outputsChanged,
            this, [this] {
                rebuildOutputConnections();
                scheduleRefresh();
            });
    connect(compositorWorkspace, &KWin::Workspace::outputOrderChanged,
            this, &KWinOutputInventory::scheduleRefresh);
    auto *const backend = KWin::kwinApp()->outputBackend();
    connect(backend, &KWin::OutputBackend::outputAdded, this, [this] {
        rebuildOutputConnections();
        scheduleRefresh();
    });
    connect(backend, &KWin::OutputBackend::outputRemoved, this, [this] {
        rebuildOutputConnections();
        scheduleRefresh();
    });
    rebuildOutputConnections();
    // The D-Bus object is registered only after plugin construction, so the
    // first generation can be sampled synchronously without racing clients.
    refresh();
}

const QByteArray &KWinOutputInventory::responseJson() const noexcept
{
    return m_store.responseJson();
}

const QVector<OutputInventoryEntry> &KWinOutputInventory::entries() const noexcept
{
    return m_store.entries();
}

quint64 KWinOutputInventory::generation() const noexcept
{
    return m_store.generation();
}

bool KWinOutputInventory::available() const noexcept
{
    return m_store.available();
}

void KWinOutputInventory::rebuildOutputConnections()
{
    for (const auto &connection : std::as_const(m_outputConnections)) {
        disconnect(connection);
    }
    m_outputConnections.clear();
    for (auto *backendOutput : KWin::kwinApp()->outputBackend()->outputs()) {
        if (!backendOutput || backendOutput->isNonDesktop()) {
            continue;
        }
        const auto changed = [this] { scheduleRefresh(); };
        if (auto *output = KWin::workspace()->findOutput(backendOutput)) {
            m_outputConnections.append(connect(output, &KWin::LogicalOutput::changed,
                                               this, changed));
        }
        m_outputConnections.append(connect(backendOutput,
                                           &KWin::BackendOutput::enabledChanged,
                                           this, changed));
        m_outputConnections.append(connect(backendOutput,
                                           &KWin::BackendOutput::priorityChanged,
                                           this, changed));
        m_outputConnections.append(connect(backendOutput,
                                           &KWin::BackendOutput::uuidChanged,
                                           this, changed));
    }
}

void KWinOutputInventory::scheduleRefresh()
{
    if (m_refreshScheduled) {
        return;
    }
    m_refreshScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshScheduled = false;
        refresh();
    });
}

void KWinOutputInventory::refresh()
{
    QString error;
    const auto candidate = sample(&error);
    if (!error.isEmpty()) {
        qWarning("QindaQt output inventory sampling failed: %s", qPrintable(error));
        return;
    }
    const auto result = m_store.publish(candidate, &error);
    if (result == OutputInventoryPublishResult::Published) {
        Q_EMIT inventoryChanged();
    } else if (result == OutputInventoryPublishResult::Rejected
               || result == OutputInventoryPublishResult::GenerationExhausted) {
        // AGENT-GUARD: Never replace the last complete projection with a
        // partial KWin transition. A later source signal retries the sample.
        qWarning("QindaQt output inventory publication failed: %s", qPrintable(error));
    }
}

QVector<OutputInventoryEntry> KWinOutputInventory::sample(QString *error) const
{
    QVector<OutputInventoryEntry> result;
    const auto backendOutputs = KWin::kwinApp()->outputBackend()->outputs();
    // Workspace order is the configured order of enabled logical desktops and
    // therefore the primary-selection order consumed by Display1. Backend
    // discovery additionally exposes connected disabled outputs, which must be
    // appended without perturbing that configured enabled order.
    QVector<const KWin::BackendOutput *> outputs;
    outputs.reserve(backendOutputs.size());
    QSet<const KWin::BackendOutput *> included;
    for (const auto *logicalOutput : KWin::workspace()->outputOrder()) {
        const auto *backendOutput = logicalOutput ? logicalOutput->backendOutput() : nullptr;
        if (backendOutput && !backendOutput->isNonDesktop()
            && backendOutputs.contains(const_cast<KWin::BackendOutput *>(backendOutput))) {
            outputs.append(backendOutput);
            included.insert(backendOutput);
        }
    }
    for (const auto *backendOutput : backendOutputs) {
        if (backendOutput && !backendOutput->isNonDesktop()
            && !included.contains(backendOutput)) {
            outputs.append(backendOutput);
        }
    }
    result.reserve(outputs.size());
    for (const auto *backendOutput : outputs) {
        if (!backendOutput) {
            fail(error, QStringLiteral("KWin output inventory contains a null output"));
            return {};
        }
        // OutputBackend includes connected VR and other non-desktop sinks. They
        // have no Workspace logical output and are intentionally outside the
        // Display1 desktop configuration contract.
        if (backendOutput->isNonDesktop()) {
            continue;
        }
        const auto *output = KWin::workspace()->findOutput(backendOutput);
        QSize physicalSize = backendOutput->physicalSize();
        if (!physicalSize.isValid() || physicalSize.isEmpty()) {
            // KWin uses an invalid QSize for unavailable EDID size. The wire's
            // non-negative 0x0 value explicitly means unknown, not measured.
            physicalSize = QSize(0, 0);
        }
        const QSize logicalSize = output ? output->geometry().size()
            : QSize(qRound(backendOutput->orientateSize(backendOutput->modeSize()).width()
                           / backendOutput->scale()),
                    qRound(backendOutput->orientateSize(backendOutput->modeSize()).height()
                           / backendOutput->scale()));
        QRect visibilityGeometry;
        if (output) {
            const KWin::Rect logicalGeometry = output->geometry();
            visibilityGeometry = QRect(logicalGeometry.x(), logicalGeometry.y(),
                                       logicalGeometry.width(), logicalGeometry.height());
        } else {
            visibilityGeometry = QRect(backendOutput->position(), logicalSize);
        }
        result.append({
            .name = backendOutput->name(),
            .geometry = QRectF(visibilityGeometry),
            .visibilityGeometry = visibilityGeometry,
            .scale = backendOutput->scale(),
            .refreshRateMilliHz = backendOutput->refreshRate(),
            .transform = transformName(backendOutput->transform().kind()),
            .enabled = backendOutput->isEnabled(),
            .internal = backendOutput->isInternal(),
            .uuid = backendOutput->uuid(),
            .priority = backendOutput->priority(),
            .physicalSizeMillimeters = physicalSize,
            .manufacturer = backendOutput->manufacturer(),
            .model = backendOutput->model(),
        });
    }
    if (error) {
        error->clear();
    }
    return result;
}

} // namespace QindaQt::Compositor::KWinIntegration
