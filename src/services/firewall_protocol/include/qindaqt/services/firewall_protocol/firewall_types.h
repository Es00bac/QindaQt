// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Firewall {

// AGENT-CONTRACT: org.qindaqt.Firewall1, owned by QindaFirewall
// (QindaFirewall docs/monitor.md, "AGENT-CONTRACT: D-Bus org.qindaqt.Firewall1").
// Session bus, object /org/qindaqt/Firewall1, interface org.qindaqt.Firewall1.
// Structured values travel as JSON text; these are the plain value types a
// client decodes that JSON into. QindaQt is a consumer only: it never widens
// or reinterprets this contract, and a schema change belongs to
// QindaFirewall first.

// LogWatcher state (the `State` property). `Unknown` is this client's own
// fallback for a value the service has not reported yet or a string outside
// the documented five; it is never sent on the wire.
enum class LogWatcherState {
    Off,
    Starting,
    Live,
    Denied,
    Retrying,
    Unknown,
};

[[nodiscard]] LogWatcherState logWatcherStateFromString(const QString &value);
[[nodiscard]] QString toString(LogWatcherState value);

// SecurityMonitor headline level. `Unknown` covers both the documented
// "unknown" value and a client that has not yet decoded a headline at all;
// presentation must treat the two the same way (ADR-0431).
enum class HeadlineLevel {
    Ok,
    Notice,
    Warning,
    Danger,
    Unknown,
};

[[nodiscard]] HeadlineLevel headlineLevelFromString(const QString &value);
[[nodiscard]] QString toString(HeadlineLevel value);

// The `Headline` property: `{level, title, text}`.
struct Headline final {
    HeadlineLevel level = HeadlineLevel::Unknown;
    QString title;
    QString text;

    friend bool operator==(const Headline &, const Headline &) = default;
};

// SecurityMonitor alert severity (the `severity` key of an alert map).
enum class AlertSeverity {
    Info,
    Notice,
    Warning,
    Danger,
};

[[nodiscard]] AlertSeverity alertSeverityFromString(const QString &value);
[[nodiscard]] QString toString(AlertSeverity value);

// One SecurityMonitor alert map, exactly as `TopAlert`, `Alerts()`, and
// `Alert(id)` carry it. Field names match the documented keys one for one;
// a consumer never reorders, renames, or infers one from another.
struct Alert final {
    QString id;
    QString kind;
    AlertSeverity severity = AlertSeverity::Info;
    QString severityLabel;
    QString title;
    QString summary;
    QString meaning;
    QString advice;
    bool canBlock = false;
    bool canConfirm = false;
    QString address;
    QString place;
    QString user;
    quint64 count = 0;
    QString firstText;
    QString lastText;
    bool ongoing = false;
    bool remote = false;
    QStringList evidence;
    QString mac;

    friend bool operator==(const Alert &, const Alert &) = default;
};

} // namespace QindaQt::Firewall
