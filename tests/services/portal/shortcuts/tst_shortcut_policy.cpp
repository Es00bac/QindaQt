// SPDX-License-Identifier: LGPL-3.0-or-later
#include "shortcut_wire.h"
#include <QDBusMetaType>
#include <QJsonArray>
#include <QtTest>
using namespace QindaQt::Services::Portal;
class ShortcutPolicyTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void preferredTriggerAndAdvisoryUnsupported() {
        const auto drafts = shortcutDrafts({{"capture", {{"description", "Capture a window"}, {"preferred_trigger", "CTRL+ALT+s"}}}, {"other", {{"description", "Other"}, {"preferred_trigger", "CAPS+Unsupported_Key"}}}});
        QVERIFY(drafts); QCOMPARE(drafts->size(), 2); QCOMPARE(drafts->first().key, QKeySequence("Ctrl+Alt+S")); QVERIFY(drafts->last().key.isEmpty());
    }
    void boundsTypesAndDuplicateIDs() {
        QVERIFY(!shortcutDrafts({{"a", {{"description", 42}}}})); QVERIFY(!shortcutDrafts({{"a", {{"description", "Action"}, {"preferred_trigger", true}}}}));
        QVERIFY(!shortcutDrafts({{QString(129, 'a'), {{"description", "Action"}}}}));
        const auto dedup = shortcutDrafts({{"a", {{"description", "First"}}}, {"a", {{"description", "Duplicate"}}}}); QVERIFY(dedup); QCOMPARE(dedup->size(), 1); QCOMPARE(dedup->first().description, QStringLiteral("First"));
        PortalShortcuts tooMany; for(int i=0;i<33;++i) tooMany.append({QString::number(i), {{"description", "Action"}}}); QVERIFY(!shortcutDrafts(tooMany));
    }
    void selectionsBindActualOfferedIdentity() {
        const ShortcutDrafts offered{{"capture", "Capture", QKeySequence("Meta+S")}, {"unassigned", "Unassigned", {}}};
        auto frame = shortcutFrame("example.app", "wayland:parent", "portal.component", offered);
        const auto decoded = shortcutDraftsFromFrame(frame); QVERIFY(decoded); QCOMPARE(decoded->first().key, offered.first().key);
        const QJsonObject results{{"shortcuts", frame.value("shortcuts")}}; QVERIFY(shortcutSelection(offered, results));
        auto rows = frame.value("shortcuts").toArray(); auto replaced = rows.first().toObject(); replaced["id"] = "different-action"; rows[0] = replaced;
        QVERIFY(!shortcutSelection(offered, {{"shortcuts", rows}}));
        frame["extra"] = "refused"; QVERIFY(!shortcutDraftsFromFrame(frame));
    }
    void malformedPortableKeyCannotAuthorize() {
        const ShortcutDrafts offered{{"capture", "Capture", QKeySequence("Meta+S")}};
        auto rows = shortcutFrame({}, {}, {}, offered).value("shortcuts").toArray(); auto row = rows.first().toObject(); row["key"] = "UnknownBogus+S"; rows[0] = row;
        QVERIFY(!shortcutSelection(offered, {{"shortcuts", rows}}));
        row["key"] = ""; rows[0] = row; const auto disabled = shortcutSelection(offered, {{"shortcuts", rows}}); QVERIFY(disabled); QVERIFY(disabled->first().key.isEmpty());
    }
    void standardWireAndPublication() {
        registerShortcutWire(); QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<PortalShortcuts>())), QByteArray("a(sa{sv})"));
        const auto rows = shortcutDescriptions({{"capture", "Capture", QKeySequence("Ctrl+K, Ctrl+C")}}); QCOMPARE(rows.first().id, QStringLiteral("capture"));
        QCOMPARE(rows.first().options.value("trigger_description").toString(), QKeySequence("Ctrl+K, Ctrl+C").toString(QKeySequence::NativeText));
    }
};
QTEST_GUILESS_MAIN(ShortcutPolicyTest)
#include "tst_shortcut_policy.moc"
