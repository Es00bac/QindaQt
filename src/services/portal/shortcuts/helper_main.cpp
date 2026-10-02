// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from QindaQt's existing admitted chooser process boundary.
#include "shortcut_wire.h"
#include <qindaqt/platform/foreign_parent/foreign_parent.h>
#include <qindaqt/services/shortcuts_client/transport.h>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QJsonDocument>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <qindaqt/services/portal/request_registry.h>
#include <QSocketNotifier>
#include <QTimer>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
namespace {
std::optional<QJsonObject> initialFrame() {
    QByteArray input; QElapsedTimer timer; timer.start();
    while (input.size() <= 131072) {
        const auto remaining = 2000 - timer.elapsed(); pollfd fd{STDIN_FILENO, POLLIN, 0};
        if (remaining <= 0 || poll(&fd, 1, static_cast<int>(remaining)) <= 0) return {};
        char byte = 0; if (read(STDIN_FILENO, &byte, 1) != 1) return {};
        if (byte == '\n') { QJsonParseError error; const auto doc = QJsonDocument::fromJson(input, &error); return error.error == QJsonParseError::NoError && doc.isObject() ? std::optional(doc.object()) : std::nullopt; }
        input.append(byte);
    }
    return {};
}
}
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores) != 0 || prctl(PR_SET_DUMPABLE, 0) != 0) return 2;
    signal(SIGPIPE, SIG_IGN); if (qEnvironmentVariable("WAYLAND_SOCKET").isEmpty()) return 2;
    const auto frame = initialFrame(); if (!frame) return 2;
    auto drafts = QindaQt::Services::Portal::shortcutDraftsFromFrame(*frame); if (!drafts) return 2;
    QApplication app(argc, argv); using namespace QindaQt::Services::Portal;
    QDialog dialog; dialog.setWindowTitle(QStringLiteral("QindaQt global shortcuts")); dialog.resize(640, 320);
    auto *layout = new QFormLayout(&dialog);
    auto *intro = new QLabel(QStringLiteral("%1 requests global shortcuts. Choose the combinations you allow; clear a field to leave an action unassigned.").arg(frame->value("app").toString()));
    intro->setTextFormat(Qt::PlainText); intro->setWordWrap(true); layout->addRow(intro);
    QList<QKeySequenceEdit *> editors;
    for (const auto &draft : *drafts) {
        auto *label = new QLabel(draft.description); label->setTextFormat(Qt::PlainText); label->setWordWrap(true);
        auto *editor = new QKeySequenceEdit(draft.key); editor->setClearButtonEnabled(true); editor->setMaximumSequenceLength(4);
        layout->addRow(label, editor); editors.append(editor);
    }
    auto *conflict = new QLabel; conflict->setTextFormat(Qt::PlainText); conflict->setWordWrap(true); layout->addRow(conflict);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok); auto *allow = buttons->button(QDialogButtonBox::Ok);
    allow->setText(QStringLiteral("Allow shortcuts")); allow->setEnabled(false); allow->setDefault(false);
    buttons->button(QDialogButtonBox::Cancel)->setDefault(true); layout->addRow(buttons);
    QindaQt::Services::Shortcuts::QtShortcutTransport native(QDBusConnection::sessionBus());
    QindaQt::Platform::ForeignParent::ForeignParent parent;
    bool ready = false; RequestResponse response = RequestResponse::Cancelled;
    const auto refresh = [&] {
        QString problem; QSet<QString> assigned;
        for (int i = 0; i < editors.size(); ++i) {
            const auto key = editors[i]->keySequence(); if (key.isEmpty()) continue;
            const auto text = key.toString(QKeySequence::PortableText);
            if (assigned.contains(text)) { problem = QStringLiteral("Two requested actions use the same combination."); break; } assigned.insert(text);
            if (!native.conflicts({key}, frame->value("component").toString(), drafts->at(i).id).isEmpty()) { problem = QStringLiteral("%1 is already assigned. Choose another combination.").arg(key.toString(QKeySequence::NativeText)); break; }
        }
        conflict->setText(problem); allow->setEnabled(ready && native.available() && problem.isEmpty());
    };
    for (auto *editor : editors) QObject::connect(editor, &QKeySequenceEdit::keySequenceChanged, &dialog, refresh);
    QObject::connect(&native, &QindaQt::Services::Shortcuts::QtShortcutTransport::bindingsChanged, &dialog, refresh);
    QObject::connect(&native, &QindaQt::Services::Shortcuts::QtShortcutTransport::authorityLost, &dialog, [&] { response = RequestResponse::Failed; dialog.reject(); });
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::ready, &dialog, [&] { ready = true; refresh(); });
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::lost, &dialog, [&] { response = RequestResponse::Failed; dialog.reject(); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { if (allow->isEnabled()) { response = RequestResponse::Success; dialog.accept(); } });
    QObject::connect(&dialog, &QDialog::finished, &app, [&] {
        QJsonObject results;
        if (response == RequestResponse::Success) {
            for (int i = 0; i < editors.size(); ++i) (*drafts)[i].key = editors[i]->keySequence();
            results.insert("shortcuts", shortcutFrame({}, {}, {}, *drafts).value("shortcuts"));
        }
        const auto bytes = QJsonDocument(QJsonObject{{"response", static_cast<int>(response)}, {"results", results}}).toJson(QJsonDocument::Compact);
        const auto written = write(STDOUT_FILENO, bytes.constData(), static_cast<size_t>(bytes.size())); app.exit(written == bytes.size() ? 0 : 2);
    });
    QSocketNotifier pipe(STDIN_FILENO, QSocketNotifier::Read, &app);
    QObject::connect(&pipe, &QSocketNotifier::activated, &dialog, [&] { response = RequestResponse::Failed; dialog.reject(); });
    QTimer::singleShot(60000, &dialog, [&] { response = RequestResponse::Failed; dialog.reject(); });
    dialog.show(); if (!dialog.windowHandle()) return 2; parent.attach(*dialog.windowHandle(), frame->value("parent").toString());
    return app.exec();
}
