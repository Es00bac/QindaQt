// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"

#include <QString>
#include <QtTypes>

namespace QindaQt::Profiles {

enum class ProfileErrorCode {
    None,
    FileReadFailed,
    InvalidJson,
    DuplicateJsonKey,
    ExcessiveNesting,
    InvalidRoot,
    MissingRequiredField,
    InvalidFieldType,
    UnsupportedSchemaVersion,
    InvalidIdentifier,
    InvalidValue,
    InvalidEnumValue,
    OutOfRange,
    EmptyPanelSet,
    DuplicatePanelId,
    DuplicateAppletId,
    NonJsonSettingsValue,
};

struct ProfileError final {
    ProfileErrorCode code = ProfileErrorCode::None;
    QString origin;
    QString path;
    QString panelId;
    QString appletId;
    QString message;
    qsizetype byteOffset = -1;

    [[nodiscard]] bool hasError() const noexcept
    {
        return code != ProfileErrorCode::None;
    }

    [[nodiscard]] QString diagnostic() const;
};

struct ProfileValidationResult final {
    ProfileError error;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return !error.hasError();
    }
};

class ProfileValidator final {
public:
    // AGENT-CONTRACT: validation owns no input or returned state. Concurrent
    // callers may therefore validate independent immutable values safely.
    [[nodiscard]] static ProfileValidationResult validate(const LayoutProfile &profile);

    // AGENT-CONTRACT: shell_layout consumes only panel placement values. This
    // narrower check deliberately ignores applets so geometry never becomes an
    // owner of plugin identity or settings validation.
    [[nodiscard]] static ProfileValidationResult validatePanelLayout(const PanelSpec &panel);
};

} // namespace QindaQt::Profiles
