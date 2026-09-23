// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QCoreApplication;
class QQmlApplicationEngine;
class QString;

namespace QindaQt::Apps::SettingsCenter {
// Private package-test boundary. The caller owns application, engine, and
// the registry-derived page identity/availability for the entire event loop.
// Returns 3 if a host cannot be observed or produces a mismatched witness;
// normal application launches do not call this helper.
int runRouteConstructionProbe(QCoreApplication &application,
                              QQmlApplicationEngine &engine,
                              const QString &page, bool available);
} // namespace QindaQt::Apps::SettingsCenter
