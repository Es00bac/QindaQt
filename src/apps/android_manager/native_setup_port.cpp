// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_setup_port.h"
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

namespace QindaQt::AndroidManager {
namespace {
constexpr auto softwareManager = "/usr/bin/qindaportage";

bool protectedSoftwareManager()
{
    // Root-owned regular executable beneath protected fixed directories.
    // This is native planning UI admission, never a recipe/guest credential.
    for (const auto &path : {QStringLiteral("/"), QStringLiteral("/usr"), QStringLiteral("/usr/bin"),
                            QString::fromLatin1(softwareManager)}) {
        const QFileInfo info(path);
        if (info.isSymLink() || info.ownerId() != 0
            || (info.permissions() & (QFile::WriteGroup | QFile::WriteOther))) return false;
        if (path != QLatin1String(softwareManager) && !info.isDir()) return false;
        if (path == QLatin1String(softwareManager) && (!info.isFile() || !info.isExecutable())) return false;
    }
    return true;
}

QProcessEnvironment nativeUiEnvironment()
{
    QProcessEnvironment child;
    const auto source = QProcessEnvironment::systemEnvironment();
    const QStringList exact{QStringLiteral("HOME"), QStringLiteral("PATH"),
        QStringLiteral("LANG"), QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("DISPLAY"),
        QStringLiteral("XAUTHORITY"), QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
        QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("QT_SCALE_FACTOR")};
    for (const auto &key : source.keys()) {
        if (exact.contains(key) || key.startsWith(QLatin1String("XDG_"))
            || key.startsWith(QLatin1String("LC_"))) child.insert(key, source.value(key));
    }
    return child;
}

class NativeSetupPort final : public SetupPort {
public:
    bool available() const override { return protectedSoftwareManager(); }
    SetupOutcome openSelection(SetupTarget target) override {
        const auto arguments = softwareSetupArguments(target);
        if (!arguments) return {SetupOutcomeCode::InvalidTarget, tr("That Android setup selection is unavailable.")};
        if (!available()) return {SetupOutcomeCode::Unavailable,
            tr("QindaPortage is unavailable. Install app-portage/qindaportage through Portage to prepare Android packages.")};
        QProcess process;
        process.setProgram(QString::fromLatin1(softwareManager));
        process.setArguments(*arguments);
        process.setProcessEnvironment(nativeUiEnvironment());
        process.setStandardOutputFile(QProcess::nullDevice());
        process.setStandardErrorFile(QProcess::nullDevice());
        if (!process.startDetached()) return {SetupOutcomeCode::Failed, tr("Software Manager could not accept the setup request.")};
        // AGENT-GUARD: Spawn acknowledgement is only a native GUI handoff.
        // Never call the guest configured/installed/running from this result.
        return {SetupOutcomeCode::Accepted,
            tr("Setup request sent to Software Manager. Review the package plan there; Android launches remain unavailable until connected.")};
    }
};
}
std::unique_ptr<SetupPort> createNativeSetupPort() { return std::make_unique<NativeSetupPort>(); }
} // namespace QindaQt::AndroidManager
