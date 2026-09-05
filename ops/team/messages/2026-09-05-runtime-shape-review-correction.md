# Correction to final review evidence

- The earlier review note incorrectly stated that `check-source-shape --warnings-as-errors --json` produced zero warnings. The JSON mode did not expose the decomposition warnings in the inspected fields.
- Plain `./tools/check-source-shape --root . --warnings-as-errors` exits 1 because of 12 existing decomposition-review warnings, including `src/shell/runtime/shellruntimeapplication.cpp` at 573 non-blank lines. This is a file-level review warning, not a new error caused by the candidate.
- The scoped candidate remains ACCEPTED: the helper relocation preserves ordering and behavior, compiles on the manager's integrated tree, and introduces no docs or contract changes. The source-shape burden is reduced at function level, while the pre-existing file-level warning remains for broader decomposition work.
