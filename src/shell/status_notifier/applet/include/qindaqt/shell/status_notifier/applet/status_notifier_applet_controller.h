// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <qindaqt/shell/status_notifier/status_notifier_types.h>
#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_types.h"

namespace QindaQt::StatusNotifierApplet {

class StatusNotifierSourceInterface;

// AGENT-CONTRACT: Composed shell facade exposed to QML as `access`.
// It translates user gestures into bounded source intents, enforces
// capability and generation fencing, renders row icons through the seam, and
// reprojects the presentation model. It never performs D-Bus operations or
// touches StatusNotifier registry/monitor internals directly.
//
// Least authority: the composing shell passes the audited manifest/policy
// capability grants at construction. `readGranted == false` withholds all
// observation (phase reports unavailable with the registered reason code, no
// rows are retained, and no icon is ever rendered); `activateGranted ==
// false` keeps browsing live but refuses every intent before dispatch. Both
// gates fail closed and cannot change after construction.
class StatusNotifierAppletController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateReprojected)
    Q_PROPERTY(QString phaseReasonText READ phaseReasonText NOTIFY stateReprojected)
    Q_PROPERTY(bool watcherLive READ watcherLive NOTIFY stateReprojected)
    Q_PROPERTY(QVariantList itemRows READ itemRows NOTIFY stateReprojected)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY stateReprojected)
    Q_PROPERTY(int presentedCount READ presentedCount NOTIFY stateReprojected)
    Q_PROPERTY(int overflowCount READ overflowCount NOTIFY stateReprojected)
    Q_PROPERTY(QString overflowText READ overflowText NOTIFY stateReprojected)
    Q_PROPERTY(bool readGranted READ readGranted CONSTANT)
    Q_PROPERTY(bool activateGranted READ activateGranted CONSTANT)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY stateReprojected)
    Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)
    Q_PROPERTY(QString feedbackStatus READ feedbackStatus NOTIFY feedbackChanged)

public:
    explicit StatusNotifierAppletController(StatusNotifierSourceInterface *source,
                                            bool readGranted,
                                            bool activateGranted,
                                            int iconSize = 22,
                                            QObject *parent = nullptr);
    ~StatusNotifierAppletController() override = default;

    [[nodiscard]] QString phaseText() const noexcept;
    [[nodiscard]] QString phaseReasonText() const;
    // AGENT-NOTE: the seam deliberately exposes watcher liveness only through
    // the S1 presentation state (Loading/Ready/Empty all require a live
    // transport; Degraded covers watcher loss and registry degradation). With
    // the read grant denied, liveness is withheld observation and reports
    // false.
    [[nodiscard]] bool watcherLive() const noexcept;
    [[nodiscard]] QVariantList itemRows() const;
    [[nodiscard]] int itemCount() const noexcept;
    [[nodiscard]] int presentedCount() const noexcept;
    [[nodiscard]] int overflowCount() const noexcept;
    [[nodiscard]] QString overflowText() const;
    [[nodiscard]] bool readGranted() const noexcept { return m_readGranted; }
    [[nodiscard]] bool activateGranted() const noexcept { return m_activateGranted; }
    [[nodiscard]] int iconSize() const noexcept { return m_iconSize; }
    void setIconSize(int size);
    [[nodiscard]] bool feedbackPresent() const noexcept { return m_feedbackPresent; }
    [[nodiscard]] QString feedback() const { return m_feedback; }
    [[nodiscard]] QString feedbackStatus() const { return m_feedbackStatus; }

    [[nodiscard]] const StatusNotifierAppletProjection &projection() const noexcept
    {
        return m_projection;
    }

    Q_INVOKABLE bool activateItem(const QString &uniqueName,
                                  const QString &objectPath,
                                  quint64 generation, int x = 0, int y = 0);
    Q_INVOKABLE bool secondaryActivateItem(const QString &uniqueName,
                                           const QString &objectPath,
                                           quint64 generation, int x = 0, int y = 0);
    Q_INVOKABLE bool openContextMenu(const QString &uniqueName,
                                     const QString &objectPath,
                                     quint64 generation, int x = 0, int y = 0);
    // The menu revision is an opaque decimal string in QML: JavaScript numbers
    // cannot exactly represent every transport publication serial.
    Q_INVOKABLE bool itemIsMenu(const QString &uniqueName, const QString &objectPath,
                               quint64 generation) const;
    Q_INVOKABLE bool hasExportedMenu(const QString &uniqueName, const QString &objectPath,
                                    quint64 generation) const;
    Q_INVOKABLE QVariantMap menuStateFor(const QString &uniqueName, const QString &objectPath,
                                        quint64 generation) const;
    Q_INVOKABLE bool invokeMenu(const QString &uniqueName, const QString &objectPath,
                               quint64 generation, const QString &revision, int itemId);
    Q_INVOKABLE bool aboutToShowMenu(const QString &uniqueName, const QString &objectPath,
                                    quint64 generation, const QString &revision, int itemId);
    Q_INVOKABLE bool scrollItem(const QString &uniqueName, const QString &objectPath,
                               quint64 generation, int delta, const QString &orientation);
    // Compatibility-only descriptor projection; the live popup uses menuStateFor.
    Q_INVOKABLE QVariantList menuRowsFor(const QString &uniqueName,
                                         const QString &objectPath,
                                         quint64 generation);
    // AGENT-CONTRACT: the status-tray contract's degradation acknowledgement
    // (docs/wiki/shell/status-tray.md), exposed as the admitted controller
    // action. Fail-closed: with the read grant denied or no source composed
    // it does nothing. Otherwise the seam clears a pending registry
    // degradation marker and its changed() reprojects the phase from live
    // state — a later valid update is never the event that first reveals or
    // clears a degradation.
    Q_INVOKABLE void acknowledgeDegraded();
    Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
    void stateReprojected();
    void feedbackChanged();
    void menuChanged();

private:
    enum class IntentKind {
        Activate,
        SecondaryActivate,
        ContextMenu,
    };

    [[nodiscard]] bool admitAction(const QString &uniqueName, const QString &objectPath,
                                   quint64 generation);
    [[nodiscard]] bool liveTarget(const QString &uniqueName, const QString &objectPath,
                                  quint64 generation) const;
    [[nodiscard]] bool dispatchMenu(const QString &uniqueName, const QString &objectPath,
                                    quint64 generation, const QString &revision, int itemId,
                                    bool opening);
    void reproject();
    void setFeedback(const QString &message, const QString &status = QStringLiteral("error"));
    [[nodiscard]] const StatusNotifierItemRow *findRow(const QString &uniqueName,
                                                       const QString &objectPath,
                                                       quint64 generation) const;
    [[nodiscard]] bool dispatchIntent(IntentKind kind,
                                      const QString &uniqueName,
                                      const QString &objectPath,
                                      quint64 generation, int x, int y);

    StatusNotifierSourceInterface *m_source = nullptr;
    bool m_readGranted = false;
    bool m_activateGranted = false;
    int m_iconSize = 22;
    StatusNotifierAppletTexts m_texts;
    StatusNotifierAppletProjection m_projection;
    // Enriched row variants: the pure projection rows plus the controller-
    // rendered iconDataUrl/iconIsPlaceholder fields.
    QVariantList m_itemRows;

    // AGENT-GUARD: exactly-once dispatch. A seam may emit changed()
    // synchronously INSIDE an intent call; without this flag that reentrant
    // reprojection (or a nested gesture) could issue a second dispatch for
    // one user gesture. While set, every intent is refused with feedback.
    bool m_dispatchInProgress = false;

    bool m_feedbackPresent = false;
    QString m_feedback;
    QString m_feedbackStatus = QStringLiteral("error");
};

} // namespace QindaQt::StatusNotifierApplet
