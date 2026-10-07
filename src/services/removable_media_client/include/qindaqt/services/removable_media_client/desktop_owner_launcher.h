// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_client/media_source.h>
#include <QStringList>
#include <memory>
namespace QindaQt::RemovableMedia {
// Borrowed same-thread literal argv port. Only the fixed validated owner's
// plan reaches it. Returning true means process submission, never readiness.
class MediaArgvStarter {
public:
    virtual ~MediaArgvStarter() = default;
    [[nodiscard]] virtual bool start(const QString &program, const QStringList &arguments) = 0;
};
// Optional launch integration, separate from the transport-only target. Roots
// are owning copies; an injected starter outlives this adapter. Construction
// is inert, and startOwner resolves only the installed graphical owner entry.
class DesktopMediaOwnerLauncher final : public MediaOwnerLauncher {
public:
    explicit DesktopMediaOwnerLauncher(QStringList applicationDataRoots);
    DesktopMediaOwnerLauncher(QStringList applicationDataRoots, MediaArgvStarter &);
    ~DesktopMediaOwnerLauncher() override;
    [[nodiscard]] bool startOwner() override;
private:
    QStringList m_dataRoots;
    std::unique_ptr<MediaArgvStarter> m_ownedStarter;
    MediaArgvStarter *m_starter;
};
}
