// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_types.h>
#include <qindaqt/services/display_transaction/transaction_types.h>

#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QString>

namespace QindaQt::DisplayWriter
{

enum class ConfigurationScope {
    CompleteTopology,
    SurvivingProperties,
};

struct ModeReference {
    QSize pixelSize;
    quint32 refreshMilliHertz = 0;

    friend bool operator==(const ModeReference &, const ModeReference &) = default;
};

struct OutputChange {
    QString connectorName;
    bool enabled = false;
    bool primary = false;
    ModeReference mode;
    QPoint position;
    double scale = 1.0;
    Display::Transform transform = Display::Transform::Normal;
    quint32 priority = 0;
    QString replicationSourceConnector;

    friend bool operator==(const OutputChange &, const OutputChange &) = default;
};

struct Configuration {
    quint64 requestId = 0;
    ConfigurationScope scope = ConfigurationScope::CompleteTopology;
    QList<OutputChange> outputs;

    friend bool operator==(const Configuration &, const Configuration &) = default;
};

// Structural validation for the least-authority compositor value. This is a
// second trust boundary after Display1 request mapping so an alternate trusted
// producer cannot make the production adapter emit malformed protocol calls.
[[nodiscard]] bool validateConfiguration(const Configuration &configuration);

enum class MapError {
    None,
    InvalidRequest,
    UnsupportedIdentity,
    UnsupportedMode,
    InvalidTopology,
};

struct MapResult {
    Configuration configuration;
    MapError error = MapError::None;
    QString reasonCode;

    [[nodiscard]] bool accepted() const noexcept { return error == MapError::None; }
};

// Pure, total translation from the Display1 machine request into the least-
// authority value accepted by the compositor transport. It intentionally
// supports only the connector/current-mode identity carried by integrated D2.
[[nodiscard]] MapResult mapApplyRequest(
    const DisplayTransaction::ApplyRequest &request, quint64 requestId);

enum class PortStartStatus {
    Started,
    AlreadyStarted,
    UnsupportedPlatform,
    ConnectionUnavailable,
};

enum class SubmitStatus {
    Accepted,
    Unavailable,
    Busy,
    Unsupported,
    Malformed,
};

enum class CompletionOutcome {
    Applied,
    Rejected,
    TransportUncertain,
    Malformed,
};

class OutputManagementObserver
{
public:
    virtual ~OutputManagementObserver() = default;
    virtual void outputManagementOwnerChanged(quint64 ownerGeneration,
                                              bool available) = 0;
    virtual void outputManagementCompleted(quint64 ownerGeneration,
                                           quint64 requestId,
                                           CompletionOutcome outcome) = 0;
};

class OutputManagementPort
{
public:
    virtual ~OutputManagementPort() = default;

    // AGENT-CONTRACT: The observer is borrowed and receives callbacks only on
    // the start/submit thread. start() never synchronously reports availability;
    // stop() is idempotent and suppresses all later callbacks.
    virtual void setObserver(OutputManagementObserver *observer) = 0;
    [[nodiscard]] virtual PortStartStatus start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual SubmitStatus submit(const Configuration &configuration) = 0;
};

} // namespace QindaQt::DisplayWriter
