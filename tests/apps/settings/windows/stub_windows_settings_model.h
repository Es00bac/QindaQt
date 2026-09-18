// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsWindows::TestSupport {

class StubWindowsSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading MEMBER loading NOTIFY viewChanged)
    Q_PROPERTY(bool ready MEMBER ready NOTIFY viewChanged)
    Q_PROPERTY(bool saving MEMBER saving NOTIFY viewChanged)
    Q_PROPERTY(bool conflict MEMBER conflict NOTIFY viewChanged)
    Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY viewChanged)
    Q_PROPERTY(bool canEdit MEMBER canEdit NOTIFY viewChanged)
    Q_PROPERTY(bool draftDirty MEMBER draftDirty NOTIFY viewChanged)
    Q_PROPERTY(bool applyAvailable MEMBER applyAvailable NOTIFY viewChanged)
    Q_PROPERTY(QString statusText MEMBER statusText NOTIFY viewChanged)
    Q_PROPERTY(QString errorText MEMBER errorText NOTIFY viewChanged)
    Q_PROPERTY(QString snapDistanceError MEMBER snapDistanceError NOTIFY viewChanged)
    Q_PROPERTY(QString focusPolicy MEMBER focusPolicy NOTIFY viewChanged)
    Q_PROPERTY(QString dockingModifier MEMBER dockingModifier NOTIFY viewChanged)
    Q_PROPERTY(int snapDistance MEMBER snapDistance NOTIFY viewChanged)
    Q_PROPERTY(QString closeContainerPolicy MEMBER closeContainerPolicy NOTIFY viewChanged)
    Q_PROPERTY(QString draftFocusPolicy MEMBER draftFocusPolicy NOTIFY viewChanged)
    Q_PROPERTY(QString draftDockingModifier MEMBER draftDockingModifier NOTIFY viewChanged)
    Q_PROPERTY(int draftSnapDistance MEMBER draftSnapDistance NOTIFY viewChanged)
    Q_PROPERTY(QString draftCloseContainerPolicy MEMBER draftCloseContainerPolicy NOTIFY viewChanged)
    Q_PROPERTY(QVariantList focusPolicyChoices MEMBER focusPolicyChoices CONSTANT)
    Q_PROPERTY(QVariantList dockingModifierChoices MEMBER dockingModifierChoices CONSTANT)
    Q_PROPERTY(QVariantList closeContainerPolicyChoices MEMBER closeContainerPolicyChoices CONSTANT)
    Q_PROPERTY(int minimumSnapDistance MEMBER minimumSnapDistance CONSTANT)
    Q_PROPERTY(int maximumSnapDistance MEMBER maximumSnapDistance CONSTANT)
    Q_PROPERTY(int defaultSnapDistance MEMBER defaultSnapDistance CONSTANT)

public:
    static QVariantList choices(std::initializer_list<std::pair<const char *, const char *>> items)
    {
        QVariantList list;
        for (const auto &[token, label] : items) {
            list.append(QVariantMap{{QStringLiteral("token"), QString::fromLatin1(token)},
                                    {QStringLiteral("label"), QString::fromLatin1(label)}});
        }
        return list;
    }

    bool loading = false;
    bool ready = true;
    bool saving = false;
    bool conflict = false;
    bool unavailable = false;
    bool canEdit = true;
    bool draftDirty = false;
    bool applyAvailable = false;
    QString statusText;
    QString errorText;
    QString snapDistanceError;
    QString focusPolicy = QStringLiteral("click");
    QString dockingModifier = QStringLiteral("super");
    int snapDistance = 12;
    QString closeContainerPolicy = QStringLiteral("ask");
    QString draftFocusPolicy = QStringLiteral("click");
    QString draftDockingModifier = QStringLiteral("super");
    int draftSnapDistance = 12;
    QString draftCloseContainerPolicy = QStringLiteral("ask");
    QVariantList focusPolicyChoices = choices({{"click", "Click to focus"},
                                               {"focus-follows-mouse", "Focus follows mouse"},
                                               {"focus-under-mouse", "Focus under mouse"}});
    QVariantList dockingModifierChoices = choices({{"super", "Meta + Shift"},
                                                   {"alt", "Alt + Shift"},
                                                   {"control", "Ctrl + Shift"},
                                                   {"disabled", "Off"}});
    QVariantList closeContainerPolicyChoices = choices({{"ask", "Ask every time"},
                                                        {"close-all", "Close every window"},
                                                        {"ungroup", "Ungroup"}});
    int minimumSnapDistance = 0;
    int maximumSnapDistance = 64;
    int defaultSnapDistance = 12;
    int focusPolicyRequests = 0;
    int dockingModifierRequests = 0;
    int snapDistanceRequests = 0;
    int closePolicyRequests = 0;
    QString lastFocusPolicy;
    QString lastDockingModifier;
    QString lastClosePolicy;
    int lastSnapDistance = 12;
    int applyRequests = 0;
    int revertRequests = 0;
    int retryRequests = 0;

    Q_INVOKABLE bool setDraftFocusPolicy(const QString &token)
    {
        ++focusPolicyRequests;
        lastFocusPolicy = token;
        draftFocusPolicy = token;
        markDirty();
        return true;
    }
    Q_INVOKABLE bool setDraftDockingModifier(const QString &token)
    {
        ++dockingModifierRequests;
        lastDockingModifier = token;
        draftDockingModifier = token;
        markDirty();
        return true;
    }
    Q_INVOKABLE bool setDraftSnapDistance(int distance)
    {
        ++snapDistanceRequests;
        lastSnapDistance = distance;
        draftSnapDistance = distance;
        markDirty();
        return true;
    }
    Q_INVOKABLE bool setDraftCloseContainerPolicy(const QString &token)
    {
        ++closePolicyRequests;
        lastClosePolicy = token;
        draftCloseContainerPolicy = token;
        markDirty();
        return true;
    }
    Q_INVOKABLE bool applyDraft() { ++applyRequests; return true; }
    Q_INVOKABLE bool revertDraft() { ++revertRequests; return true; }
    Q_INVOKABLE void retry() { ++retryRequests; }

Q_SIGNALS:
    void viewChanged();

private:
    void markDirty()
    {
        draftDirty = true;
        applyAvailable = canEdit;
        Q_EMIT viewChanged();
    }
};

} // namespace QindaQt::Apps::SettingsWindows::TestSupport
