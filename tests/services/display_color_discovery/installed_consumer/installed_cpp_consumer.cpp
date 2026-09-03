// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtEndian>
#include <qindaqt/services/display_color_discovery/profile_discovery.h>
#include <qindaqt/services/display_color_model/color_types.h>

#include <cstring>

using namespace QindaQt::DisplayColor;

namespace
{

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content) == content.size();
}

QByteArray minimalIcc(quint32 profileSize)
{
    QByteArray file;
    file.resize(static_cast<qsizetype>(profileSize));
    file.fill('\0');
    uchar *base = reinterpret_cast<uchar *>(file.data());
    const quint32 sizeBe = qToBigEndian(profileSize);
    std::memcpy(base, &sizeBe, 4);
    const quint32 versionBe = qToBigEndian(0x02400000);
    std::memcpy(base + 8, &versionBe, 4);
    std::memcpy(base + 12, "mntr", 4);
    std::memcpy(base + 16, "RGB ", 4);
    std::memcpy(base + 20, "XYZ ", 4);
    const quint32 magicBe = qToBigEndian(IccMagicAcsp);
    std::memcpy(base + 36, &magicBe, 4);
    return file;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tree(QStringLiteral("discovery-installed-consumer-XXXXXX"));
    if (!tree.isValid()) {
        qCritical() << "Failed to create a consumer scratch tree";
        return 1;
    }
    const QString root = tree.filePath(QStringLiteral("system"));
    if (!QDir().mkpath(root)) {
        qCritical() << "Failed to create a consumer root";
        return 1;
    }
    if (!writeFile(root + QStringLiteral("/consumer.icc"), minimalIcc(512))) {
        qCritical() << "Failed to write a consumer profile";
        return 1;
    }

    ProfileDiscovery discovery({DiscoveryRoot{root, DiscoveryOrigin::System}});
    const DiscoveryResult result = discovery.discoverCatalog();
    if (result.profiles.size() != 1 ||
        result.profiles.first().descriptor.profileId != QStringLiteral("consumer")) {
        qCritical() << "Installed discovery consumer scanned the wrong catalog";
        return 1;
    }

    qInfo() << "Installed DisplayColorDiscovery C++ consumer verified successfully";
    return 0;
}
