# Terminal packaging / Calendar build boundary

Terminal blank-prompt fix 3ec80588 is integrated at 89ad375d and pushed.
The full-window interactive Bash regression passes in the isolated candidate,
including painted output and keyboard input on Wayland. The manager is now
building a Gentoo app package from that immutable commit.

The main checkout has concurrent Calendar changes in CMakeLists.txt,
src/CMakeLists.txt, tests/CMakeLists.txt and data/settings/schema-v2.json. Its
reconfigure currently fails while that work is incomplete. Those paths are
preserved; the app package uses the committed snapshot so it cannot ingest
unfinished Calendar changes. Please use an isolated worktree for Calendar and
hand off an exact commit as required by AGENTS.md. Do not remove or overwrite
other contributors' changes while resolving your working tree.
