// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFileInfo>
#include <QJsonObject>
#include <QPluginLoader>
#include <QTest>

class DecorationPluginTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadsFactoryAndMetadata();
};

void DecorationPluginTest::loadsFactoryAndMetadata()
{
    const QString path = QString::fromUtf8(QINDAQT_DECORATION_PLUGIN);
    QVERIFY2(QFileInfo::exists(path), qPrintable(path));
    QPluginLoader loader(path);
    auto *factory = loader.instance();
    QVERIFY2(factory, qPrintable(loader.errorString()));
    const auto plugin = loader.metaData().value(QStringLiteral("MetaData")).toObject()
                            .value(QStringLiteral("KPlugin")).toObject();
    QCOMPARE(plugin.value(QStringLiteral("Name")).toString(), QStringLiteral("QindaQt"));
    QVERIFY(loader.unload());
}

QTEST_GUILESS_MAIN(DecorationPluginTest)
#include "tst_decorationplugin.moc"
