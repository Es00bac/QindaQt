# S3 Network staging passes units; mixed-prefix link stops before runtime

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T09:31:48-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: product repair unit-green; build-environment recovery in progress

Current manager `01145dcd5886861657a348b3e5b18a75fa7c6307` merged without
conflict as `03f27f396ec2c64a3c98426a4c99a8dfe3c3eb55`. The owned package
repair now queries and stages the Network QML library, plugin, `qmldir`,
`qmltypes`, and all five named Network QML files. Its exact closure, each-file
omission mutations, symlink rejection, and traversal rejection pass in the
complete desktop Python unit set: 102/102, exit 0.

`cmake --preset dev -DCMAKE_PREFIX_PATH=/tmp/qindaqt-arch-665/root/usr`
configured and generated, with its known warning that host Qt and the extracted
Arch prefix conflict. `cmake --build build/dev --parallel 1 --target
qindaqt-desktop-session-probe` compiled the Network module and linked the merged
Settings route, then stopped at action 866/870 linking `qindaqt-shell`, exit 1.
The link combined `/tmp/qindaqt-arch-665/root/usr/lib` KF6/QtWaylandClient with
host `/usr/lib64/libQt6Core.so.6.11.1`; undefined ICU `ucnv_*`/`ucal_*`/`ucol_*`
and `QSpiAccessibleBridge::*@Qt_6_PRIVATE_API` symbols prove this is the
mixed-prefix toolchain failure, not a Network source failure.

No private bus, compositor, or runtime row was started. The same `build/dev`
root and exact failure are preserved. I am recovering the previously proven
single package environment before an explicit build retry. The serialized
compiler/CTest/private-bus/private-runtime lane remains exclusively held.
