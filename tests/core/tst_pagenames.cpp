// SPDX-License-Identifier: GPL-3.0-or-later
#include "windowcontainer.h"
#include <QJsonArray>
#include <QtTest>
using namespace QindaQt::Core;
class PageNamesTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void namesSurviveReorderRemovalAndTransfer() {
        WindowContainer group("group");
        QVERIFY(group.addPage("a", "na", "wa"));
        QVERIFY(group.addPage("b", "nb", "wb"));
        QCOMPARE(group.page("a")->displayName(), QString("Tab 1"));
        QCOMPARE(group.page("b")->displayName(), QString("Tab 2"));
        QVERIFY(group.movePage("b", 0));
        QCOMPARE(group.pages()[0].displayName(), QString("Tab 2"));
        QVERIFY(group.renamePage("b", "  Planning  "));
        QCOMPARE(group.page("b")->displayName(), QString("Planning"));
        QVERIFY(group.removeWindow("wa"));
        QCOMPARE(group.page("b")->displayName(), QString("Planning"));
        QVERIFY(group.renamePage("b", " "));
        QCOMPARE(group.page("b")->displayName(), QString("Tab 2"));
        QVERIFY(group.addPage("c", "nc", "wc"));
        QCOMPARE(group.page("c")->displayName(), QString("Tab 3"));
        QVERIFY(group.renamePage("b", "Research"));
        auto page = group.detachPage("b");
        QVERIFY(page);
        WindowContainer other("other");
        QVERIFY(other.addPage("x", "nx", "wx"));
        QVERIFY(other.addPage("y", "ny", "wy"));
        QVERIFY(other.addPage(*page));
        QCOMPARE(other.page("b")->displayName(), QString("Research"));
        QVERIFY(other.renamePage("b", ""));
        QCOMPARE(other.page("b")->displayName(), QString("Tab 3"));
    }
    void rejectsInvalidNamesWithoutMutation() {
        WindowContainer group("group");
        QVERIFY(group.addPage("a", "na", "wa"));
        QVERIFY(group.renamePage("a", "Notes"));
        const auto before = group.toJson();
        for (const auto &name : {QString("bad\nname"), QString(256, 'x'),
                                 QString(QChar(0x202e)), QString(QChar(0xd800))}) {
            QVERIFY(!group.renamePage("a", name));
            QCOMPARE(group.toJson(), before);
        }
        QVERIFY(!group.renamePage("missing", "Name"));
        QCOMPARE(group.toJson(), before);
    }
    void oldAndNewDocumentsRoundTrip() {
        WindowContainer group("group");
        QVERIFY(group.addPage("a", "na", "wa"));
        QVERIFY(group.addPage("b", "nb", "wb"));
        QVERIFY(group.renamePage("b", "Notes"));
        const auto saved = group.toJson();
        auto loaded = WindowContainer::fromJson(saved);
        QVERIFY(loaded);
        QCOMPARE(loaded->toJson(), saved);
        auto old = saved;
        auto pages = old["pages"].toArray();
        for (qsizetype i = 0; i < pages.size(); ++i) {
            auto page = pages[i].toObject();
            page.remove("name"); page.remove("number"); pages[i] = page;
        }
        old["pages"] = pages;
        loaded = WindowContainer::fromJson(old);
        QVERIFY(loaded);
        QCOMPARE(loaded->pages()[0].displayName(), QString("Tab 1"));
        QCOMPARE(loaded->pages()[1].displayName(), QString("Tab 2"));
        for (const auto &invalid : {QJsonValue(-1), QJsonValue(1.5), QJsonValue("2")}) {
            auto bad = saved; auto values = bad["pages"].toArray();
            auto page = values[1].toObject(); page["number"] = invalid;
            values[1] = page; bad["pages"] = values;
            QVERIFY(!WindowContainer::fromJson(bad));
        }
    }
};
QTEST_GUILESS_MAIN(PageNamesTest)
#include "tst_pagenames.moc"
