// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QObject;
class QString;

namespace QindaQt::AppShell {
class ApplicationCoordinator;
}

namespace QindaQt::Apps::Calendar {

class CalendarController;

// Drives the production QML surface and the real store for --check-ui-actions.
// Exercises: event.new dialog → create → on-disk persistence, openForEdit →
// uid-stable update (summary + recurrence + reminder, revision bump), a
// second edit clearing recurrence/reminder, occurrence model visibility,
// view.* action switching with coordinator checked-state sync, event.delete
// persistence, and fixture import. dataRoot is the controller's injected
// root; fixturesDir points at the committed .ics fixtures. Returns false with
// a diagnostic on the first failed step.
[[nodiscard]] bool verifyCalendarUiActions(
    QObject *qmlRoot, QindaQt::AppShell::ApplicationCoordinator *coordinator,
    CalendarController *controller, const QString &dataRoot,
    const QString &fixturesDir, QString *error);

// Returns the first missing required UI object name for --check-ui-contract,
// or an empty string when the complete contract surface is present.
[[nodiscard]] QString missingUiContractObject(QObject *qmlRoot);

} // namespace QindaQt::Apps::Calendar
