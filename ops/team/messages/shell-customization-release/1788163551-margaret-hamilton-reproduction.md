# Exact GCC 15.3 Release reproduction and bounded repair direction

- Posted: 2026-08-31T02:05:51-06:00 (unix 1788163551)
- Worker: Margaret Hamilton
- Exact base: `21c5a779c15b315c763c916c68b646cb4c19d8bb`
- Configuration: standalone customization-editor tree, Release,
  `QINDAQT_ENABLE_STRICT_WARNINGS=ON`, GCC 15.3.0
- Reproduction result: **FAIL**, Ninja action 59/61

The exact compile command contains `-O3 -DNDEBUG -std=c++20 -Wall -Wextra
-Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`. GCC reports two
`-Werror=maybe-uninitialized` diagnostics from `std::__exchange` while moving
the inactive `QString` payload storage of `DropTarget::beforeAppletId` through
the implicit `DropTarget`-to-`std::optional<DropTarget>` return conversion at
`keyboard_navigation.cpp:115`. It names the local default-constructed `target`
at line 111.

The semantic value is valid: the nested optional is deliberately disengaged so
a panel step appends at the neighboring panel end. The fragile shape is the
sequence of default construction, three assignments (including `reset()`), and
an implicit move into an outer optional. The bounded repair will aggregate-
initialize all `DropTarget` fields, keep that object const, and explicitly copy
it into the outer optional. This makes both optional engagement states and
lifetimes apparent without warning suppression or changing the append
contract. The focused test will compare complete forward/backward result
tuples and both list/missing-panel boundaries.
