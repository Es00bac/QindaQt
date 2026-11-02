// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/pairing_presenter.h>

#include <QtTest/QtTest>

using namespace QindaQt::Link;

class LinkPresenterTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void escapesMarkupAndStripsControls()
    {
        QCOMPARE(sanitizeForNotification(QStringLiteral("<b>Eve</b> & \"co\"")),
                 QStringLiteral("&lt;b&gt;Eve&lt;/b&gt; &amp; &quot;co&quot;"));
        QCOMPARE(sanitizeForNotification(QStringLiteral("a\nb\x01" "c") + QChar(0x202E) + QLatin1Char('d')), QStringLiteral("a b c d"));
        QCOMPARE(sanitizeForNotification(QString(200, QLatin1Char('x'))).size(), 64);
        QCOMPARE(spacedCode(QStringLiteral("007123")), QStringLiteral("007 123"));
    }
};

QTEST_GUILESS_MAIN(LinkPresenterTest)
#include "tst_link_presenter.moc"
