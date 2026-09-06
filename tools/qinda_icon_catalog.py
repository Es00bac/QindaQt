#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Merges the per-group canonical catalogs into one semantic name table.

AGENT-CONTRACT: this is the single source of truth the generator and
validator both read (tools/generate_qinda_icon_theme.py,
tools/validate_qinda_icon_theme.py). A name absent from CANON and ALIASES is
not a fallback candidate -- generation must fail rather than draw a
placeholder glyph for it. See docs/wiki/shell/icon-theme.md for the contract
this catalog implements.
"""
from typing import Dict, Tuple

import qinda_icon_catalog_actions as _actions
import qinda_icon_catalog_apps as _apps
import qinda_icon_catalog_categories_status as _categories_status
import qinda_icon_catalog_devices as _devices
import qinda_icon_catalog_mimetypes as _mimetypes
import qinda_icon_catalog_places as _places
from qinda_icon_shapes import Icon

_MODULES = (_apps, _actions, _devices, _places, _mimetypes, _categories_status)

GROUPS = ("apps", "actions", "devices", "places", "mimetypes", "categories", "status")
GROUP_CONTEXT = {
    "apps": "Applications",
    "actions": "Actions",
    "devices": "Devices",
    "places": "Places",
    "mimetypes": "MimeTypes",
    "categories": "Categories",
    "status": "Status",
}

CANON: Dict[str, Icon] = {}
for _module in _MODULES:
    _overlap = set(_module.CANON) & set(CANON)
    if _overlap:
        raise ValueError(f"duplicate canonical icon name(s): {sorted(_overlap)}")
    CANON.update(_module.CANON)

for _icon_name, _icon in CANON.items():
    if _icon.group not in GROUPS:
        raise ValueError(f"canonical icon {_icon_name!r} declares unknown group {_icon.group!r}")

# alias -> (canonical name it renders as, directory group it is filed under)
ALIASES: Dict[str, Tuple[str, str]] = {}
for _module in _MODULES:
    for _alias, (_target, _group_override) in _module.ALIASES.items():
        if _target not in CANON:
            raise ValueError(f"alias {_alias!r} points to unknown canonical icon {_target!r}")
        if _alias in ALIASES or _alias in CANON:
            raise ValueError(f"alias {_alias!r} collides with an existing name")
        ALIASES[_alias] = (_target, _group_override or CANON[_target].group)


def icon_for(name: str) -> Tuple[Icon, str]:
    """Resolve a canonical or alias name to its Icon and destination group.

    Raises KeyError for any name outside the catalog; callers must not
    substitute a placeholder glyph for an unresolved name.
    """
    if name in CANON:
        return CANON[name], CANON[name].group
    target, group = ALIASES[name]
    return CANON[target], group


def all_names() -> Dict[str, str]:
    """Every theme-visible name mapped to the directory group it is written into."""
    names = {name: icon.group for name, icon in CANON.items()}
    names.update({alias: group for alias, (_, group) in ALIASES.items()})
    return names
