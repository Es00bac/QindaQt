// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/private_audio_churn.h"

#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Audio::Test;

class WirePlumberMeterVisibilityTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void internalMetersDoNotCrowdOutApplications()
    {
        PrivateAudioChurn churn;
        QString failure;
        QVERIFY2(churn.start(&failure), qPrintable(failure));
        for (const Stream &stream : churn.snapshot().streams) {
            QVERIFY2(!stream.mediaName.startsWith(QLatin1String("qindaqt.meter.")),
                     qPrintable(stream.mediaName));
        }
        QVERIFY(churn.cycle());
        // Ownership is the actual bound stream, not a reserved-looking name.
        QVERIFY(churn.cycle(QStringLiteral("qindaqt.meter.external"), true));
    }
};

QTEST_GUILESS_MAIN(WirePlumberMeterVisibilityTests)
#include "tst_wireplumber_meter_visibility.moc"
