// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_renderer.h"
#include "fixtures.h"
#include <QTest>
using namespace QindaQt::Viewer;

class ViewerTextTest final : public QObject {
    Q_OBJECT
private slots:
    void actualPdfTextIsLiteralAndUnicode() {
        QTemporaryDir temp;
        auto latest = std::make_shared<std::atomic<quint64>>(1);
        DocumentRenderer renderer(latest);
        const auto result = renderer.render({1, ViewerFixtures::textPdf(temp.path())});
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.pdf && result.textAllowed);
        QVERIFY(result.pageText.contains(QStringLiteral("Alpha zero")));
        QVERIFY(result.pageText.contains(QStringLiteral("<b>literal & café</b>")));
        QVERIFY(result.textError.isEmpty());
    }
    void textlessAndImage() {
        QTemporaryDir temp;
        auto latest = std::make_shared<std::atomic<quint64>>(1);
        DocumentRenderer renderer(latest);
        auto result = renderer.render({1, ViewerFixtures::pdf(temp.path())});
        QVERIFY(result.pdf && result.textAllowed);
        QVERIFY(result.pageText.isEmpty() && result.textError.isEmpty());
        result = renderer.render({1, ViewerFixtures::image(temp.path())});
        QVERIFY(!result.pdf && !result.textAllowed);
        QVERIFY(result.pageText.isEmpty());
    }
    void permissionAndPassword() {
        auto latest = std::make_shared<std::atomic<quint64>>(1);
        auto search = std::make_shared<std::atomic<quint64>>(7);
        DocumentRenderer renderer(latest);
        const QString restricted = QStringLiteral(VIEWER_FIXTURES_DIR "/copy-restricted.pdf");
        const auto result = renderer.render({1, restricted});
        QVERIFY2(!result.image.isNull(), qPrintable(result.error));
        QVERIFY(result.pdf && !result.locked && !result.textAllowed);
        QVERIFY(result.pageText.isEmpty() && !result.textError.isEmpty());
        TextSearchRequest request;
        request.revision = 7; request.path = restricted; request.query = QStringLiteral("fixture");
        QCOMPARE(renderer.find(request, search).status, TextSearchStatus::Unavailable);
        request.path = QStringLiteral(VIEWER_FIXTURES_DIR "/password.pdf");
        QCOMPARE(renderer.find(request, search).status, TextSearchStatus::Unavailable);
        request.password = "viewer-test";
        QCOMPARE(renderer.find(request, search).status, TextSearchStatus::NotFound);
    }
    void forwardBackwardCaseAndWrap() {
        QTemporaryDir temp;
        auto latest = std::make_shared<std::atomic<quint64>>(1);
        auto search = std::make_shared<std::atomic<quint64>>(7);
        DocumentRenderer renderer(latest);
        TextSearchRequest request;
        request.revision = 7; request.path = ViewerFixtures::textPdf(temp.path());
        request.query = QStringLiteral("Alpha");
        auto found = renderer.find(request, search);
        QCOMPARE(found.status, TextSearchStatus::Found); QCOMPARE(found.page, 0);
        QCOMPARE(found.text.mid(found.start, found.length), QStringLiteral("Alpha"));
        const int first = found.start;
        request.offset = found.start + found.length;
        found = renderer.find(request, search);
        QCOMPARE(found.page, 0);
        QCOMPARE(found.text.mid(found.start, found.length), QStringLiteral("alpha"));
        request.offset = found.start + found.length;
        found = renderer.find(request, search); QCOMPARE(found.page, 1);
        request.page = 1; request.offset = found.start + found.length;
        found = renderer.find(request, search); QCOMPARE(found.page, 0); QCOMPARE(found.start, first);
        request.page = 0; request.offset = first; request.backward = true;
        found = renderer.find(request, search); QCOMPARE(found.page, 1);
        request.page = 0; request.offset = 0; request.backward = false; request.caseSensitive = true;
        found = renderer.find(request, search); QCOMPARE(found.page, 0);
        request.offset = found.start + found.length;
        found = renderer.find(request, search); QCOMPARE(found.page, 1);
        request.query = QStringLiteral("missing query");
        QCOMPARE(renderer.find(request, search).status, TextSearchStatus::NotFound);
    }
    void actualPdfPageTextBoundRefusesWithoutTruncation() {
        QTemporaryDir temp;
        auto latest = std::make_shared<std::atomic<quint64>>(1);
        auto search = std::make_shared<std::atomic<quint64>>(7);
        DocumentRenderer renderer(latest);
        TextSearchRequest request;
        request.revision = 7; request.query = QStringLiteral("a");
        request.path = ViewerFixtures::boundedTextPdf(temp.path(), 1, 262144);
        auto result = renderer.find(request, search);
        QCOMPARE(result.status, TextSearchStatus::Found);
        QCOMPARE(result.text.size(), 262144);
        request.path = ViewerFixtures::boundedTextPdf(temp.path(), 1, 262145);
        // A fresh renderer avoids the intentional same-path loaded-document cache.
        DocumentRenderer oversized(latest);
        result = oversized.find(request, search);
        QCOMPARE(result.status, TextSearchStatus::Limit);
        QVERIFY(result.text.isEmpty());
        QVERIFY(result.error.contains(QStringLiteral("too much text")));
    }
    void actualPdfPageSearchBoundIsNotNotFound() {
        QTemporaryDir temp;
        auto latest = std::make_shared<std::atomic<quint64>>(1);
        auto search = std::make_shared<std::atomic<quint64>>(7);
        DocumentRenderer renderer(latest);
        TextSearchRequest request;
        request.revision = 7; request.query = QStringLiteral("missing");
        request.path = ViewerFixtures::boundedTextPdf(temp.path(), 4097, 0);
        const auto result = renderer.find(request, search);
        QCOMPARE(result.status, TextSearchStatus::Limit);
        QVERIFY(result.text.isEmpty());
        QVERIFY(result.error.contains(QStringLiteral("4096")));
    }
    void rejectedBoundsAndRetiredSearch() {
        auto latest = std::make_shared<std::atomic<quint64>>(1);
        auto search = std::make_shared<std::atomic<quint64>>(7);
        DocumentRenderer renderer(latest);
        TextSearchRequest request; request.revision = 7;
        QCOMPARE(renderer.find(request, search).status, TextSearchStatus::Limit);
        request.query = QString(513, QLatin1Char('a'));
        QCOMPARE(renderer.find(request, search).status, TextSearchStatus::Limit);
        request.query = QStringLiteral("valid");
        search->store(8);
        const auto retired = renderer.find(request, search);
        QCOMPARE(retired.status, TextSearchStatus::Cancelled);
        QVERIFY(retired.text.isEmpty() && retired.error.isEmpty());
    }
};
QTEST_MAIN(ViewerTextTest)
#include "tst_text.moc"
