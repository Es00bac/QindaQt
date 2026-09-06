// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitycontrolchannel.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Test::PanelVisibilityControl;

namespace {

void writeCommand(const QString &path, const QJsonObject &command)
{
    QSaveFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray bytes = QJsonDocument(command).toJson(QJsonDocument::Compact);
    QCOMPARE(file.write(bytes), static_cast<qint64>(bytes.size()));
    QVERIFY(file.commit());
}

QJsonObject acknowledgement(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

} // namespace

class PanelVisibilityControlChannelTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void rejectsRelativeControlPath();
    void readsEachAtomicFullscreenCommandOnce();
    void rejectsMalformedOrNonMonotonicCommands();
    void recordsReadyAppliedAndTimeoutAcknowledgements();
};

void PanelVisibilityControlChannelTest::rejectsRelativeControlPath()
{
    Channel channel(QStringLiteral("control.json"));
    QString error;
    QVERIFY(!channel.isValid(&error));
    QCOMPARE(error, QStringLiteral("control file path is not absolute"));
}

void PanelVisibilityControlChannelTest::readsEachAtomicFullscreenCommandOnce()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = temporary.filePath(QStringLiteral("control.json"));
    Channel channel(path);
    QString error;
    QVERIFY(channel.isValid(&error));
    QVERIFY(!channel.readNext(&error).has_value());

    writeCommand(path, {{QStringLiteral("schemaVersion"), 1},
                        {QStringLiteral("sequence"), 1},
                        {QStringLiteral("action"), QStringLiteral("fullscreen")},
                        {QStringLiteral("enabled"), true}});
    const auto command = channel.readNext(&error);
    QVERIFY(command.has_value());
    QCOMPARE(command->sequence, quint64{1});
    QVERIFY(command->action == Action::Fullscreen);
    QVERIFY(command->fullscreen);
    QVERIFY(channel.acknowledge(*command, true, &error));
    const QJsonObject ack = acknowledgement(path + QStringLiteral(".ack"));
    QVERIFY(!ack.isEmpty());
    QCOMPARE(ack.value(QStringLiteral("status")).toString(), QStringLiteral("applied"));
    QVERIFY(ack.value(QStringLiteral("fullscreen")).toBool());
    QVERIFY(!channel.readNext(&error).has_value());
}

void PanelVisibilityControlChannelTest::rejectsMalformedOrNonMonotonicCommands()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = temporary.filePath(QStringLiteral("control.json"));
    Channel channel(path);
    QString error;
    writeCommand(path, {{QStringLiteral("schemaVersion"), 1},
                        {QStringLiteral("sequence"), 1},
                        {QStringLiteral("action"), QStringLiteral("fullscreen")}});
    QVERIFY(!channel.readNext(&error).has_value());
    QCOMPARE(error, QStringLiteral("fullscreen command lacks boolean enabled"));

    writeCommand(path, {{QStringLiteral("schemaVersion"), 1},
                        {QStringLiteral("sequence"), 1},
                        {QStringLiteral("action"), QStringLiteral("close")}});
    QVERIFY(channel.readNext(&error).has_value());
    writeCommand(path, {{QStringLiteral("schemaVersion"), 1},
                        {QStringLiteral("sequence"), 1},
                        {QStringLiteral("action"), QStringLiteral("close")},
                        {QStringLiteral("padding"), 1}});
    QVERIFY(!channel.readNext(&error).has_value());
    QCOMPARE(error, QStringLiteral("control command has an invalid sequence"));
}

void PanelVisibilityControlChannelTest::recordsReadyAppliedAndTimeoutAcknowledgements()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = temporary.filePath(QStringLiteral("control.json"));
    Channel channel(path);
    QString error;
    QVERIFY(channel.publishReady(QStringLiteral("probe title"), &error));
    QJsonObject ack = acknowledgement(path + QStringLiteral(".ack"));
    QVERIFY(!ack.isEmpty());
    QCOMPARE(ack.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
    QCOMPARE(ack.value(QStringLiteral("title")).toString(), QStringLiteral("probe title"));

    Command close;
    close.sequence = 4;
    close.action = Action::Close;
    QVERIFY(channel.acknowledge(close, false, &error));
    ack = acknowledgement(path + QStringLiteral(".ack"));
    QCOMPARE(ack.value(QStringLiteral("action")).toString(), QStringLiteral("close"));
    QVERIFY(ack.value(QStringLiteral("closed")).toBool());
    QVERIFY(channel.acknowledgeTimeout(&error));
    ack = acknowledgement(path + QStringLiteral(".ack"));
    QCOMPARE(ack.value(QStringLiteral("status")).toString(), QStringLiteral("timed-out"));
}

QTEST_GUILESS_MAIN(PanelVisibilityControlChannelTest)
#include "tst_panelvisibilitycontrolchannel.moc"
