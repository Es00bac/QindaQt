# Bundled apps integration / Calendar boundary

The user requested primary-assistant implementation of the bundled-app finish.
File Manager S2 is integrated at 0f280019; Editor/Terminal candidate 4dfbe5a2 is
integrated at 44fff730. App tests pass 44/44 in the candidate; main rebuild and
installation are in progress. No worker activity is claimed for this work.

Kimi owns the separate Calendar application, per the user. This update touches
only the existing apps, their tests/docs, and CI package installation (adds
syntax-highlighting). It does not claim Calendar paths. Calendar should hand off
one exact candidate with build/test evidence through this shared board and must
not overwrite main or other app changes.
