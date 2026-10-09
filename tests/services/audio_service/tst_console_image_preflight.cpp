// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/gain_qualified_image.h"
#include <QtTest>
using namespace QindaQt::Audio::Test;

class ConsoleImagePreflightTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void actualPinnedImageIsAdmitted()
    {
        QualifiedGainImage image;
        QString reason;
        QVERIFY2(image.preflight(&reason), qPrintable(reason));
        QVERIFY(image.bootstrapStillPinned());
    }
    // Invoked separately by the recipe after withholding/changing only its
    // private copied receipt. No private daemon or Core is constructed here.
    void unavailableOrChangedReceiptRefuses()
    {
        QualifiedGainImage image;
        QString reason;
        QVERIFY(!image.preflight(&reason));
        QVERIFY(reason == QLatin1String("qualified-receipt-unavailable")
             || reason == QLatin1String("qualified-receipt-changed"));
        QVERIFY(!image.bootstrapStillPinned());
    }
};
QTEST_GUILESS_MAIN(ConsoleImagePreflightTests)
#include "tst_console_image_preflight.moc"
