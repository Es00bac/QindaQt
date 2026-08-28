// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization/layout_editing_repository.h"

#include <QVector>

#include <memory>

namespace QindaQt::ShellCustomizationEditor {

struct SequenceEvaluation final {
    bool accepted = false;
    QindaQt::ShellCustomization::EditingError error;
    quint64 revision = 0;
};

// Seam between the editor session and the in-process transaction engine. The
// engine itself stays the only mutation authority; this interface exists so
// the session and gesture logic are testable against a scripted engine
// without a repository, and so the presentation never touches the engine
// directly. Implementations must be confined to the editor thread: the
// repository contract is single-thread by design.
class EditingEngine {
public:
    virtual ~EditingEngine() = default;

    [[nodiscard]] virtual QindaQt::ShellCustomization::EditingResult
    execute(const QindaQt::ShellCustomization::EditingCommand &command) = 0;
    [[nodiscard]] virtual QindaQt::ShellCustomization::EditingEvaluation
    evaluate(const QindaQt::ShellCustomization::EditingCommand &command) const = 0;
    // Evaluates an ordered mutation sequence against one disposable copy of
    // the current snapshot. Implementations must retag each command from the
    // preceding simulated result and must not publish a real revision.
    [[nodiscard]] virtual SequenceEvaluation evaluateSequence(
        const QVector<QindaQt::ShellCustomization::EditingCommand> &commands) const = 0;
    // Null while the backing repository never initialized.
    [[nodiscard]] virtual std::shared_ptr<const QindaQt::ShellCustomization::LayoutEditingSnapshot>
    snapshot() const = 0;
    [[nodiscard]] virtual QindaQt::ShellCustomization::LayoutEditingStatus status() const = 0;
    [[nodiscard]] virtual bool hasPreview() const = 0;
};

} // namespace QindaQt::ShellCustomizationEditor
