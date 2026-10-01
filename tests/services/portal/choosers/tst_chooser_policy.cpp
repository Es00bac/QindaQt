// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/portal/chooser_types.h>
#include <QFile>
#include <QtTest>
using namespace QindaQt::Services::Portal;
class ChooserPolicyTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void fileModesFiltersChoicesRoundTrip() {
        registerChooserTypes();
        const FileFilters filters{{QStringLiteral("Text"), {{0, QStringLiteral("*.txt")}, {1, QStringLiteral("text/plain")}}}};
        const AccessChoices choices{{QStringLiteral("encoding"), QStringLiteral("Encoding"), {{QStringLiteral("utf8"), QStringLiteral("UTF-8")}}, {}},
            {QStringLiteral("check"), QStringLiteral("Check"), {}, {}}};
        QVariantMap options{{QStringLiteral("filters"), QVariant::fromValue(filters)},
            {QStringLiteral("choices"), QVariant::fromValue(choices)}, {QStringLiteral("current_folder"), QByteArray("/private/fixture\0", 17)}};
        const auto open = fileChooserRequest(FileChooserMode::Open, {}, {}, QStringLiteral("Open"), options);
        QVERIFY(open); QCOMPARE(open->question.choices.last().initial, QStringLiteral("false"));
        const auto round = fileChooserFromFrame(fileChooserFrame(*open)); QVERIFY(round); QCOMPARE(round->filters, filters);
        const QJsonObject results{{"uris", QJsonArray{"file:///private/fixture/literal%20%25%24.txt"}},
            {"choices", QJsonArray{QJsonObject{{"id", "encoding"}, {"value", "utf8"}}, QJsonObject{{"id", "check"}, {"value", "true"}}}}, {"filter", 0}};
        const auto validated = fileChooserResults(*open, results); QVERIFY(validated);
        QCOMPARE(validated->value(QStringLiteral("uris")).toStringList().size(), 1);
        options.insert(QStringLiteral("files"), QVariant::fromValue(FileNames{QByteArray("one\0", 4), QByteArray("two\0", 4)}));
        const auto many = fileChooserRequest(FileChooserMode::SaveMany, {}, {}, QStringLiteral("Save"), options); QVERIFY(many);
        QVERIFY(fileChooserFromFrame(fileChooserFrame(*many))); QCOMPARE(many->files.size(), 2);
        QVERIFY(!fileChooserResults(*many, results)); // One result cannot satisfy two ordered save names.
        options.insert(QStringLiteral("current_name"), QStringLiteral("literal % $`.txt"));
        const auto save = fileChooserRequest(FileChooserMode::Save, {}, {}, QStringLiteral("Save"), options); QVERIFY(save);
        QCOMPARE(save->currentName, QStringLiteral("literal % $`.txt"));
    }
    void hostileKnownTypesPathsAndOutputFail() {
        const auto parse = [](const QVariantMap &options) { return fileChooserRequest(FileChooserMode::Open, {}, {}, {}, options); };
        QVERIFY(!parse({{QStringLiteral("multiple"), 1}}));
        QVERIFY(!parse({{QStringLiteral("current_folder"), QByteArray("/unterminated")}}));
        QVERIFY(!parse({{QStringLiteral("current_folder"), QByteArray("/bad\0tail\0", 10)}}));
        QVERIFY(!fileChooserRequest(FileChooserMode::SaveMany, {}, {}, {}, {{QStringLiteral("files"), QVariant::fromValue(FileNames{QByteArray("../escape\0", 10)})}}));
        QVERIFY(!parse({{QStringLiteral("filters"), QVariant::fromValue(FileFilters{{QStringLiteral("Bad"), {{2, QStringLiteral("*")}}}})}}));
        const auto request = parse({}); QVERIFY(request);
        for (const auto *uri : {"https://example.invalid/file", "file://remote/private/file", "file:///private/../file", "file:///private/file?query", "file:///private/a b"}) {
            QVERIFY(!fileChooserResults(*request, {{"uris", QJsonArray{QString::fromLatin1(uri)}}, {"choices", QJsonArray{}}, {"filter", -1}}));
        }
        QVERIFY(!fileChooserRequest(FileChooserMode::Open, {}, QStringLiteral("x11:123"), {}, {}));
    }
    void catalogAppIdsAndUpdatesRemainOfferedOnly() {
        QindaQt::ApplicationCatalog::DirectoryScan catalog;
        QindaQt::ApplicationCatalog::ScannedApplication app; app.entry.id = QStringLiteral("org.test.One"); app.entry.name = QStringLiteral("One"); catalog.applications.append(app);
        const auto request = appChooserRequest({}, {}, {QStringLiteral("org.test.One"), QStringLiteral("org.test.Missing")}, {}, catalog);
        QVERIFY(request); QCOMPARE(request->candidates.size(), 1); QVERIFY(appChooserFromFrame(appChooserFrame(*request)));
        QVERIFY(appChooserResults(*request, {{"choice", "org.test.One"}}));
        QVERIFY(!appChooserResults(*request, {{"choice", "org.test.Missing"}}));
        QVERIFY(!applicationCandidates({QStringLiteral("org.test.One.desktop")}, catalog));
        QVERIFY(!applicationCandidates({QStringLiteral("org.test.One"), QStringLiteral("org.test.One")}, catalog));
        QVERIFY(!appChooserRequest({}, {}, {}, {{QStringLiteral("uri"), QStringLiteral("file:///literal space")}}, catalog));
        QVERIFY(applicationCandidates({}, catalog)->isEmpty());
    }
};
QTEST_GUILESS_MAIN(ChooserPolicyTest)
#include "tst_chooser_policy.moc"
