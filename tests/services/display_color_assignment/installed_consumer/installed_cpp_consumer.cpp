// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <qindaqt/services/display_color_assignment/assignment_document.h>
#include <qindaqt/services/display_color_assignment/assignment_store.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>

using namespace QindaQt::DisplayColor;

namespace
{

class NullTransport final : public QindaQt::Services::SettingsClient::SettingsTransport
{
    Q_OBJECT
public:
    bool start(QString *error) override
    {
        if (error != nullptr) {
            *error = QStringLiteral("no bus in consumer proof");
        }
        return false;
    }
    void stop() override {}
    void requestSnapshot(quint64, const QString &, const QStringList &) override {}
    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override
    {
    }
    void requestActivation() override {}
};

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // The pure document boundary round-trips without any transport at all.
    AssignmentDocument document;
    ColorAssignmentRecord record;
    record.outputStableId = QStringLiteral("DP-1");
    record.profileId = QStringLiteral("consumer-profile");
    document.records.append(record);
    const std::optional<QVariant> encoded = encodeAssignmentDocument(document);
    if (!encoded.has_value()) {
        qCritical() << "Failed to encode the assignment document";
        return 1;
    }
    const AssignmentDocumentDecodeResult decoded = decodeAssignmentDocument(*encoded);
    if (!decoded.ok || decoded.document != document) {
        qCritical() << "Failed to decode the assignment document";
        return 1;
    }

    // The store composes over the public settings client seam without any
    // D-Bus dependency of its own: before authority exists it refuses
    // writes fail-closed.
    NullTransport transport;
    QindaQt::Services::SettingsClient::SettingsClient client(
        transport, {QLatin1String(ColorAssignmentsSettingsKey)});
    if (!client.start()) {
        qCritical() << "Unexpected transport start failure in consumer proof";
        return 1;
    }
    SettingsAssignmentStore store(client);
    ColorAssignmentDraft draft;
    draft.entries.append(
        {QStringLiteral("DP-1"), QStringLiteral("consumer-profile"), QByteArray(), false});
    QString error;
    if (store.applyDraft(draft, &error) || error != QStringLiteral("unavailable")) {
        qCritical() << "Store must refuse writes without confirmed authority";
        return 1;
    }

    qInfo() << "Installed DisplayColorAssignment C++ consumer verified successfully";
    return 0;
}

#include "installed_cpp_consumer.moc"
