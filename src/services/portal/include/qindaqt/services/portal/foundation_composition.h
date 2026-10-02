// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QObject>
#include <QStringList>
#include <memory>
namespace QindaQt::Services::Portal {
// Small composition of independent family adapters, not portal policy.
// Construct before the borrowed backend host is registered with ExportAdaptors;
// host and connection registration must outlive this same-thread object.
// Executable paths and data roots come only from native composition, never
// frontend options. start publishes the additive native Portal1 control; the
// supervisor explicitly attaches its accepted session/display through it.
class PortalFoundationComposition final {
public:
    PortalFoundationComposition(QObject &backendHost, QDBusConnection,
        QString privateRuntimeDirectory, QString consentExecutable,
        QString uriRelayExecutable, QStringList applicationDataRoots);
    // Additive chooser composition; original callers remain source/link
    // compatible and do not gain an ambient/default helper executable.
    PortalFoundationComposition(QObject &backendHost, QDBusConnection,
        QString privateRuntimeDirectory, QString consentExecutable,
        QString uriRelayExecutable, QStringList applicationDataRoots,
        QString chooserExecutable);
    // Additive capture helper; older constructors explicitly leave capture
    // unavailable and never resolve an ambient executable/display.
    PortalFoundationComposition(QObject &backendHost, QDBusConnection,
        QString privateRuntimeDirectory, QString consentExecutable,
        QString uriRelayExecutable, QStringList applicationDataRoots,
        QString chooserExecutable, QString captureExecutable);
    ~PortalFoundationComposition();
    bool start();
    void stop();
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::Portal
