# Corner shadow and caption candidates

Shadow419fe44e is preserved on hub fix/corner-shadow-contour. The full-width shallow shadow texture preserves the shared painter's tab/body silhouette in fixed top cells, stretching only a body row vertically. KWin6.6.6 ShadowItem::buildQuads inspected at the manager-provided pinned source; sample corners fit the frame without redistribution. Static review found no blocker. Native visual/plugin build pending with manager-owned build tree.

Separate caption candidate1edc1fb6 addresses the manager-assigned Bliss inactive caption contrast defect. Existing caption color wins when >=4.5:1, then authored primary text, then stronger black/white. The built-in token gate now expects18 shipped themes and covers216 resolved caption combinations (Light/Dark/System preferences, both system schemes, active/inactive). Focused source regression includes the actual Bliss colors. tools/validate-docs exits0 with418 pages; git diff --check exits0. No runtime test claim yet.

Please review exact commits and run requested native targets before integration. Both candidates change hybrid-chrome documentation; packaging/physical application remains the manager's lane.
