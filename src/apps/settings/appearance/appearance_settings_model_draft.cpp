// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"

namespace QindaQt::Apps::SettingsAppearance {

bool AppearanceSettingsModel::setDraftValue(const QString &key,
                                            const QVariant &value)
{
    if (!canEdit()) {
        return false;
    }

    AppearanceValues next = m_draft;
    // AGENT-GUARD: QVariant silently converts string "12" to 12.0. Strict
    // field typing makes a presentation bug a rejected draft edit instead of
    // a differently typed persistent write.
    const auto requireDouble = [&value](double *target) {
        switch (value.metaType().id()) {
        case QMetaType::Double:
        case QMetaType::Float:
        case QMetaType::LongLong:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::ULongLong:
            *target = value.toDouble();
            return true;
        default:
            return false;
        }
    };
    if (key == QLatin1String(AppearanceKeys::Theme)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        next.themeId = value.toString();
    } else if (key == QLatin1String(AppearanceKeys::ColorScheme)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        const auto scheme = colorSchemeFromToken(value.toString());
        if (!scheme.has_value()) return false;
        next.colorScheme = *scheme;
    } else if (key == QLatin1String(AppearanceKeys::FontFamily)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        next.fontFamily = value.toString();
    } else if (key == QLatin1String(AppearanceKeys::FontPointSize)) {
        if (!requireDouble(&next.fontPointSize)) return false;
    } else if (key == QLatin1String(AppearanceKeys::FontAntialiasing)) {
        if (value.metaType().id() != QMetaType::Bool) return false;
        next.fontAntialiasing = value.toBool();
    } else if (key == QLatin1String(AppearanceKeys::FontHinting)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        const auto hinting = fontHintingFromToken(value.toString());
        if (!hinting.has_value()) return false;
        next.fontHinting = *hinting;
    } else if (key == QLatin1String(AppearanceKeys::FontSubpixelOrder)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        const auto subpixel = subpixelOrderFromToken(value.toString());
        if (!subpixel.has_value()) return false;
        next.fontSubpixelOrder = *subpixel;
    } else if (key == QLatin1String(AppearanceKeys::Wallpaper)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        next.wallpaper = value.toString();
    } else if (key == QLatin1String(AppearanceKeys::WallpaperMode)) {
        if (value.metaType().id() != QMetaType::QString) return false;
        const auto mode = wallpaperModeFromToken(value.toString());
        if (!mode.has_value()) return false;
        next.wallpaperMode = *mode;
    } else if (key == QLatin1String(AppearanceKeys::UiScale)) {
        if (!requireDouble(&next.uiScale)) return false;
    } else {
        return false;
    }

    if (next == m_draft) {
        return true;
    }
    m_draft = next;
    const QVariantMap draftMap = m_draft.toVariantMap();
    const QVariantMap confirmedMap = m_confirmed.toVariantMap();
    if (draftMap.value(key) == confirmedMap.value(key)) {
        m_dirtyKeys.remove(key);
    } else {
        m_dirtyKeys.insert(key);
    }
    refreshValidationAndPreview();
    return true;
}

bool AppearanceSettingsModel::cancelDraft()
{
    if (!canEdit() || !draftDirty()) {
        return false;
    }
    m_draft = m_confirmed;
    m_dirtyKeys.clear();
    m_conflictIntent = false;
    m_confirmedError.clear();
    clearSaveResults();
    setState(State::Ready);
    refreshValidationAndPreview();
    return true;
}

bool AppearanceSettingsModel::applyDraft()
{
    if (!applyAvailable()) {
        return false;
    }
    return startApplySequence();
}

void AppearanceSettingsModel::setConfirmed(AppearanceValues values)
{
    const bool baselineBefore = m_hasBaseline;
    const bool confirmedChanged = !(m_confirmed == values);
    const AppearanceValues draftBefore = m_draft;
    m_confirmed = values;
    if (!baselineBefore) {
        m_hasBaseline = true;
        // AGENT-GUARD: Editing is impossible before the first baseline, so
        // the composed default draft is guesswork rather than user intent.
        m_draft = values;
        m_dirtyKeys.clear();
    } else {
        // AGENT-GUARD: Only explicitly edited fields survive authority refresh.
        // Retaining every old field invents intent and can overwrite a peer.
        QVariantMap rebased = m_draft.toVariantMap();
        const QVariantMap confirmedMap = m_confirmed.toVariantMap();
        for (const QString &key : AppearanceKeys::scopedKeys()) {
            if (!m_dirtyKeys.contains(key)) {
                rebased.insert(key, confirmedMap.value(key));
            }
        }
        const auto decoded = AppearanceValues::fromVariantMap(rebased);
        Q_ASSERT(decoded.has_value());
        m_draft = *decoded;
        const QVariantMap draftMap = m_draft.toVariantMap();
        for (const QString &key : AppearanceKeys::scopedKeys()) {
            if (draftMap.value(key) == confirmedMap.value(key)) {
                m_dirtyKeys.remove(key);
            }
        }
    }
    if (confirmedChanged || !(draftBefore == m_draft) || !baselineBefore) {
        refreshValidationAndPreview();
    }
}

bool AppearanceSettingsModel::startApplySequence()
{
    const QVariantMap draftMap = m_draft.toVariantMap();
    const QVariantMap confirmedMap = m_confirmed.toVariantMap();
    for (const QString &key : AppearanceKeys::scopedKeys()) {
        const QVariant next = draftMap.value(key);
        if (m_dirtyKeys.contains(key) && next != confirmedMap.value(key)) {
            m_queue.append(CommitIntent{key, next});
        }
    }
    if (m_queue.isEmpty()) {
        return false;
    }
    m_sequenceActive = true;
    m_conflictIntent = false;
    m_confirmedError.clear();
    beginSaveResults();
    setState(State::Saving);
    writeNextQueuedKey();
    // A synchronous refusal either aborts honestly or waits for fresh
    // authority, so return the sequence state after the attempted dispatch.
    return m_sequenceActive;
}

} // namespace QindaQt::Apps::SettingsAppearance
