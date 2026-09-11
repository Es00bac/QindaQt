// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>

namespace QindaQt::Apps::SettingsAccessibility::TestSupport {

class StubAccessibilitySettingsModel final : public QObject {
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
    Q_PROPERTY(QString textScaleError MEMBER textScaleError NOTIFY viewChanged)
    Q_PROPERTY(bool highContrast MEMBER highContrast NOTIFY viewChanged)
    Q_PROPERTY(bool reducedMotion MEMBER reducedMotion NOTIFY viewChanged)
    Q_PROPERTY(bool reducedTransparency MEMBER reducedTransparency NOTIFY viewChanged)
    Q_PROPERTY(double textScale MEMBER textScale NOTIFY viewChanged)
    Q_PROPERTY(bool draftHighContrast MEMBER draftHighContrast NOTIFY viewChanged)
    Q_PROPERTY(bool draftReducedMotion MEMBER draftReducedMotion NOTIFY viewChanged)
    Q_PROPERTY(bool draftReducedTransparency MEMBER draftReducedTransparency NOTIFY viewChanged)
    Q_PROPERTY(double draftTextScale MEMBER draftTextScale NOTIFY viewChanged)
    Q_PROPERTY(double minimumTextScale MEMBER minimumTextScale CONSTANT)
    Q_PROPERTY(double maximumTextScale MEMBER maximumTextScale CONSTANT)
    Q_PROPERTY(double defaultTextScale MEMBER defaultTextScale CONSTANT)

public:
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
    QString textScaleError;
    bool highContrast = false;
    bool reducedMotion = false;
    bool reducedTransparency = false;
    double textScale = 1.0;
    bool draftHighContrast = false;
    bool draftReducedMotion = false;
    bool draftReducedTransparency = false;
    double draftTextScale = 1.0;
    double minimumTextScale = 0.5;
    double maximumTextScale = 3.0;
    double defaultTextScale = 1.0;
    int highContrastRequests = 0;
    int reducedMotionRequests = 0;
    int reducedTransparencyRequests = 0;
    int textScaleRequests = 0;
    double lastTextScale = 1.0;
    int applyRequests = 0;
    int revertRequests = 0;
    int retryRequests = 0;

    Q_INVOKABLE bool setDraftHighContrast(bool enabled)
    {
        ++highContrastRequests;
        draftHighContrast = enabled;
        markDirty();
        return true;
    }
    Q_INVOKABLE bool setDraftReducedMotion(bool enabled)
    {
        ++reducedMotionRequests;
        draftReducedMotion = enabled;
        markDirty();
        return true;
    }
    Q_INVOKABLE bool setDraftReducedTransparency(bool enabled)
    {
        ++reducedTransparencyRequests;
        draftReducedTransparency = enabled;
        markDirty();
        return true;
    }
    Q_INVOKABLE bool setDraftTextScale(double scale)
    {
        ++textScaleRequests;
        lastTextScale = scale;
        draftTextScale = scale;
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

} // namespace QindaQt::Apps::SettingsAccessibility::TestSupport
