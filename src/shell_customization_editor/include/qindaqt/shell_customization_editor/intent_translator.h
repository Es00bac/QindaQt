// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"

#include <QVariantMap>
#include <QVector>
#include <QtTypes>

namespace QindaQt::ShellCustomizationEditor {

// Everything the translator needs that is not the intent or target. The
// session derives these values from state it already observed.
//
// AGENT-GUARD: expectedRevision must be the revision the caller observed (the
// session chains it from executed EditingResult::revision values). Re-reading
// the repository revision here would defeat optimistic concurrency by
// construction (architecture prohibited shortcut 13).
struct TranslationContext final {
    quint64 expectedRevision = 0;
    // InsertApplet only: the unique instance id the session chose for the new
    // instance. The translator never generates identity.
    QString newInstanceAppletId;
    // MoveApplet/DuplicateApplet only: settings of the source instance as they
    // exist in the state the commands target. Inside an active preview this is
    // the dragged instance's current settings, not the gesture-start values.
    QVariantMap sourceSettings;
};

// Emits only mutation commands; the gesture bracket (BeginPreview/CommitPreview)
// is added by the session or by gestureSequence(). Pure: identical inputs
// always produce an identical command sequence, which is what the pointer vs.
// keyboard parity invariant is tested against.
[[nodiscard]] QVector<QindaQt::ShellCustomization::EditingCommand>
translateIntent(const CustomizationIntent &intent,
                const DropTarget &target,
                const TranslationContext &context);

// Canonical preview-bracketed form: BeginPreview, the translated mutation
// commands, CommitPreview. This is the documentation/parity form. It is not a
// literal execution script: the engine advances the revision on every executed
// command, so the session executes the parts with revisions chained from
// returned results instead of replaying this sequence blindly.
[[nodiscard]] QVector<QindaQt::ShellCustomization::EditingCommand>
gestureSequence(const CustomizationIntent &intent,
                const DropTarget &target,
                const TranslationContext &context);

} // namespace QindaQt::ShellCustomizationEditor
