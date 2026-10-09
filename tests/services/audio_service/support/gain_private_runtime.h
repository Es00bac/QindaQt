// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gain_qualified_image.h"

#include <QtCore/QMap>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryDir>
#include <optional>

namespace QindaQt::Audio::Test
{
// Every native gain test creates this before WpCore. Its private daemon has no
// hardware monitor, both host buses are absent, and no host graph name/path is
// accepted by the fixture. Processes die before the temporary roots disappear.
class PrivateGainRuntime final
{
public:
    PrivateGainRuntime();
    ~PrivateGainRuntime();
    PrivateGainRuntime(const PrivateGainRuntime &) = delete;
    PrivateGainRuntime &operator=(const PrivateGainRuntime &) = delete;
    [[nodiscard]] bool start(bool savedLoudState = false);
    [[nodiscard]] bool ready() const;
    [[nodiscard]] const QString &failure() const { return m_failure; }
    [[nodiscard]] QString statePath() const;
    void stopPolicy();
    [[nodiscard]] const QualifiedGainImage &image() const { return m_image; }

private:
    bool startProcess(QProcess &process, const QString &program,
                      const QStringList &arguments);
    static void stopProcess(QProcess &process);
    QualifiedGainImage m_image;
    QTemporaryDir m_root;
    QMap<QByteArray, std::optional<QByteArray>> m_previous;
    QProcessEnvironment m_environment;
    QProcess m_pipewire;
    QProcess m_policy;
    QString m_failure;
    bool m_started = false;
};
} // namespace QindaQt::Audio::Test
