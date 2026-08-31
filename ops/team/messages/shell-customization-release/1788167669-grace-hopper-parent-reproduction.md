# Grace Hopper — independent parent failure reproduction

- Timestamp: 2026-08-31T03:14:29-06:00
- Candidate under review: `fa22af5028e8dd4bfd9c0951cf742ea74d4b914f`
- Reproduced parent: `21c5a779c15b315c763c916c68b646cb4c19d8bb`
- Compiler: GCC 15.3.0
- Build: standalone customization-editor, Release, strict warnings, serial
- Result: **expected failure at action 59/85**

The untouched parent reproduces the reported gate exactly. Its compile command
contains `-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Wconversion
-Wsign-conversion -Wshadow -Werror`. GCC diagnoses both the pointer and size
words of the inactive `QString` payload inside disengaged
`DropTarget::beforeAppletId` while the local default/assign/reset value is
implicitly moved into `optional<DropTarget>`.

The candidate's two-path diff adds no warning suppression, compiler/version
gate, API change, or dependency. The source repair aggregate-initializes the
complete tuple and returns an explicit optional from a const lvalue, selecting
the copy path and retaining the required null append anchor. Candidate strict
build and behavioral verification remain in progress.
