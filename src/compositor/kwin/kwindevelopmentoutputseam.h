// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QSize>
#include <QString>

namespace KWin {
class BackendOutput;
class OutputBackend;
}

namespace QindaQt::Compositor::KWinIntegration {

enum class DevelopmentOutputMutationResult {
    Succeeded,
    BackendUnavailable,
    BackendRejected,
};

// Non-owning, GUI-thread port used by the bounded request controller. The
// implementation owns only outputs it created and must never remove any other
// backend output, including one with a colliding external name.
class DevelopmentOutputMutator
{
public:
    virtual ~DevelopmentOutputMutator() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual qsizetype totalOutputCount() const = 0;
    [[nodiscard]] virtual qsizetype ownedOutputCount() const = 0;
    [[nodiscard]] virtual bool outputNameExists(const QString &name) const = 0;
    [[nodiscard]] virtual bool ownsOutput(const QString &name) const = 0;
    [[nodiscard]] virtual DevelopmentOutputMutationResult addVirtualOutput(
        const QString &name,
        const QSize &logicalSize,
        qreal scale) = 0;
    [[nodiscard]] virtual DevelopmentOutputMutationResult removeVirtualOutput(
        const QString &name) = 0;
    virtual void removeOwnedOutputs() = 0;
};

class DevelopmentOutputController final
{
public:
    static constexpr qsizetype MaxOwnedVirtualOutputs = 8;
    static constexpr qsizetype MaxTotalOutputs = 32;
    static constexpr qsizetype MaxNameUtf8Bytes = 128;
    static constexpr int MinimumWidth = 320;
    static constexpr int MinimumHeight = 200;
    static constexpr int MaximumLogicalDimension = 16'384;
    static constexpr qreal MinimumScale = 1.0;
    static constexpr qreal MaximumScale = 3.0;

    // AGENT-CONTRACT: The mutator is optional and non-owning. When present it
    // must outlive this controller. All calls occur on KWin's GUI thread.
    DevelopmentOutputController(bool mutationsEnabled,
                                DevelopmentOutputMutator *mutator);

    [[nodiscard]] QByteArray addVirtualOutputForTest(
        const QString &name,
        int width,
        int height,
        double scale) const;
    [[nodiscard]] QByteArray removeVirtualOutputForTest(const QString &name) const;
    [[nodiscard]] QJsonObject capabilities() const;
    // Idempotent plugin-teardown path. Call only after D-Bus unregistration so
    // backend output signals cannot be mistaken for a completed public call.
    void shutdown();

private:
    bool m_mutationsEnabled = false;
    DevelopmentOutputMutator *m_mutator = nullptr;
};

// Exact KWin 6.6.5 OutputBackend adapter. Backend and output pointers are
// borrowed QPointers; the adapter removes only entries recorded after its own
// successful createVirtualOutput call.
class KWinDevelopmentOutputSeam final : public QObject,
                                       public DevelopmentOutputMutator
{
    Q_OBJECT

public:
    explicit KWinDevelopmentOutputSeam(KWin::OutputBackend *backend,
                                       QObject *parent = nullptr);
    ~KWinDevelopmentOutputSeam() override;

    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] qsizetype totalOutputCount() const override;
    [[nodiscard]] qsizetype ownedOutputCount() const override;
    [[nodiscard]] bool outputNameExists(const QString &name) const override;
    [[nodiscard]] bool ownsOutput(const QString &name) const override;
    [[nodiscard]] DevelopmentOutputMutationResult addVirtualOutput(
        const QString &name,
        const QSize &logicalSize,
        qreal scale) override;
    [[nodiscard]] DevelopmentOutputMutationResult removeVirtualOutput(
        const QString &name) override;
    void removeOwnedOutputs() override;

private:
    QPointer<KWin::OutputBackend> m_backend;
    QHash<QString, QPointer<KWin::BackendOutput>> m_ownedOutputs;
};

} // namespace QindaQt::Compositor::KWinIntegration
