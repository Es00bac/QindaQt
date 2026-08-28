// SPDX-License-Identifier: GPL-3.0-or-later

// Installed-boundary consumer probe. Links only the staged public library and
// headers of the ClipboardApplet component — no build-tree paths — and
// exercises the exact lock/privacy purge contract at that boundary.

#include <QCoreApplication>
#include <QtGlobal>
#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_controller.h>
#include <qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h>

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::Services::ClipboardModel;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue value;
    value.formats = { { QStringLiteral("text/plain"), "installed-consumer-payload" } };
    const auto admitted = model.admit(
        value, model.generation(), QStringLiteral("InstalledConsumer"), 1);
    if (!admitted.accepted()) {
        qCritical("installed consumer: admission refused");
        return 1;
    }

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter);

    if (controller.phaseText() != QStringLiteral("ready") || controller.entryCount() != 1) {
        qCritical("installed consumer: expected ready phase with one entry");
        return 2;
    }

    const quint32 generationBeforeLock = model.generation();

    // AGENT-GUARD: the authenticated lock must deny model privacy, purge the
    // entry, and advance the generation before the lock becomes observable.
    adapter.setLocked(true);
    if (controller.phaseText() != QStringLiteral("locked") || controller.entryCount() != 0) {
        qCritical("installed consumer: lock did not withhold presentation");
        return 3;
    }
    if (model.generation() != generationBeforeLock + 1) {
        qCritical("installed consumer: lock did not fence the generation");
        return 4;
    }
    if (!model.snapshot().entries.isEmpty()) {
        qCritical("installed consumer: lock did not purge model content");
        return 5;
    }

    // Unlock must not redisclose the pre-lock entry.
    adapter.setLocked(false);
    if (controller.phaseText() != QStringLiteral("ready") || controller.entryCount() != 0) {
        qCritical("installed consumer: unlock redisclosed pre-lock content");
        return 6;
    }

    return 0;
}
