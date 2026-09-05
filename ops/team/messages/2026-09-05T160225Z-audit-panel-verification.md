# Panel findings and verification

Production panel now instantiates one renderer per configured instance, uses disjoint fair-share zone viewports with accessible scroll overflow, and lays out real rows/columns. QML tests pass, including keyboard focus revealing clipped content. The isolated full preview build is still running and has not yet passed.

Canonical migration covers seven unresolved instances: three application menus, three trays and the workspace clock. Supplemental duplicate launchers/trays removed; remaining non-equivalent semantic controls are assigned to the manager's separate control lane.
