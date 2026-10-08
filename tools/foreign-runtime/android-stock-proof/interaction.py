"""Private-seat gestures on observed UUIDs; no shell or package authority."""
import time
from frame_capture import capture, process_version


def unchanged_others(before, after, target):
    if set(before) - {target} != set(after) - {target}:
        raise RuntimeError("interaction-unrelated-window-lost-or-added")
    for key in set(before) - {target}:
        if before[key]["geometry"] != after[key]["geometry"]:
            raise RuntimeError("interaction-unrelated-window-moved")


def point(row, fx=.5, fy=.5):
    g = row["geometry"]
    return g["x"] + g["width"] * fx, g["y"] + g["height"] * fy


def run(snapshot, inject, guard, launch, pid, ids, remaining, report=None):
    version = process_version(pid)
    report = {} if report is None else report
    captures, steps = [], []
    report.update(schema=1, identityAuthority=False, steps=steps, captures=captures)
    def frame(label):
        remaining()
        if len(captures) >= 8:
            raise RuntimeError("capture-count-bound")
        record, pixels = capture(pid, version, label, guard)
        captures.append(record)
        return pixels
    def events(batch):
        remaining(); guard(); inject(batch); guard()
    def key(name, pressed):
        return {"type": "key", "key": name, "pressed": pressed}
    def button(name, pressed):
        return {"type": "button", "button": name, "pressed": pressed}
    def move(x, y):
        return {"type": "pointer-absolute", "x": x, "y": y}
    def wait(predicate):
        deadline = time.monotonic() + min(5, remaining())
        while time.monotonic() < deadline:
            value = snapshot()
            if predicate(value):
                return value
            time.sleep(.1)
        raise RuntimeError("interaction-observation-timeout")
    # Clock is the last admitted launch and active topmost window. Shrink via
    # the real Meta+right gesture to expose Calculator without privileged focus.
    initial = snapshot()
    clock, calculator = ids[1], ids[0]
    if set(initial) != set(ids) or not initial[clock].get("active"):
        raise RuntimeError("interaction-initial-window-set")
    first = frame("clock-before")
    x, y = point(initial[clock], .85, .85)
    events([move(x, y), key("left-meta", True), button("right", True)])
    events([move(x - 300, y - 220)])
    events([button("right", False), key("left-meta", False)])
    current = wait(lambda rows: clock in rows
                   and rows[clock]["geometry"] != initial[clock]["geometry"])
    unchanged_others(initial, current, clock)
    if current[clock]["geometry"]["width"] <= 0 or current[clock]["geometry"]["height"] <= 0:
        raise RuntimeError("interaction-invalid-resize")
    resized = frame("clock-resized")
    if resized == first:
        raise RuntimeError("resize-without-render-change")
    steps.append({"action": "pointer-resize", "window": clock,
                  "before": initial[clock]["geometry"], "after": current[clock]["geometry"]})
    events([key("tab", True), key("tab", False)])
    time.sleep(.15)
    keyboard = frame("clock-tab")
    if keyboard == resized:
        raise RuntimeError("keyboard-without-visible-change")
    steps.append({"action": "keyboard-tab", "window": clock,
                  "qualification": "visible-response-not-semantic-app-state"})
    # Close via ordinary KWin menu, not CompositorShell1's dock-owner methods.
    # Guest-only MouseBindings maps Alt+right to the actual operations menu;
    # pinned KWin appends Close last. A refused/misdirected action fails the
    # observed UUID set; no force-stop or PID signal is substituted.
    def close(target):
        before = snapshot()
        # Select only an exposed point whose current topmost UUID is target.
        choices = [point(before[target], fx, fy)
                   for fx in (.5, .9, .1) for fy in (.5, .9, .1)]
        def visible(candidate):
            x, y = candidate
            for other, row in before.items():
                g = row["geometry"]
                if other != target and row["stackIndex"] > before[target]["stackIndex"] and (
                        g["x"] <= x < g["x"] + g["width"] and
                        g["y"] <= y < g["y"] + g["height"]):
                    return False
            return True
        exposed = [candidate for candidate in choices if visible(candidate)]
        if not exposed:
            raise RuntimeError("close-target-not-exposed")
        x, y = exposed[0]
        events([move(x, y), key("left-alt", True), button("right", True),
                button("right", False), key("left-alt", False)])
        time.sleep(.15)
        events([key("up", True), key("up", False), key("enter", True), key("enter", False)])
        after = wait(lambda rows: target not in rows)
        unchanged_others(before, after, target)
        steps.append({"action": "ordinary-menu-close", "window": target,
                      "survivors": sorted(after)})
        return after
    close(clock)
    calculator_before = frame("calculator-survived")
    if not snapshot()[calculator].get("active"):
        raise RuntimeError("calculator-not-active-after-close")
    events([key("tab", True), key("tab", False)])
    time.sleep(.15)
    if frame("calculator-tab") == calculator_before:
        raise RuntimeError("calculator-keyboard-without-visible-change")
    steps.append({"action": "keyboard-tab", "window": calculator,
                  "qualification": "visible-response-not-semantic-app-state"})
    before = snapshot()
    launch(1)
    current = wait(lambda rows: len(set(rows) - set(before)) == 1
                   and set(before).issubset(rows))
    new_clock = next(iter(set(current) - set(before)))
    unchanged_others(before, current, new_clock)
    if new_clock == clock:
        raise RuntimeError("relaunch-reused-window")
    steps.append({"action": "relaunch", "oldWindow": clock, "newWindow": new_clock})
    # Shrink the new foreground Clock to expose Calculator, then close only
    # Calculator while this exact new Clock remains alive and unmoved.
    before = snapshot()
    x, y = point(before[new_clock], .85, .85)
    events([move(x, y), key("left-meta", True), button("right", True)])
    events([move(x - 300, y - 220)])
    events([button("right", False), key("left-meta", False)])
    current = wait(lambda rows: new_clock in rows
                   and rows[new_clock]["geometry"] != before[new_clock]["geometry"])
    unchanged_others(before, current, new_clock)
    # Calculator remains exposed outside the smaller Clock rectangle.
    before = snapshot()
    x, y = point(before[calculator], .9, .9)
    g = before[new_clock]["geometry"]
    if g["x"] <= x < g["x"] + g["width"] and g["y"] <= y < g["y"] + g["height"]:
        raise RuntimeError("calculator-resize-point-covered")
    events([move(x, y), key("left-meta", True), button("right", True)])
    events([move(x - 200, y - 120)])
    events([button("right", False), key("left-meta", False)])
    current = wait(lambda rows: calculator in rows
                   and rows[calculator]["geometry"] != before[calculator]["geometry"])
    unchanged_others(before, current, calculator)
    frame("calculator-resized")
    steps.append({"action": "pointer-resize", "window": calculator,
                  "before": before[calculator]["geometry"],
                  "after": current[calculator]["geometry"]})
    close(calculator)
    frame("clock-survived")
    before = snapshot(); launch(0)
    current = wait(lambda rows: len(set(rows) - set(before)) == 1
                   and set(before).issubset(rows))
    new_calculator = next(iter(set(current) - set(before)))
    unchanged_others(before, current, new_calculator)
    if new_calculator in ids or new_calculator == new_clock:
        raise RuntimeError("interaction-window-reacquired")
    new_ids = [new_calculator, new_clock]
    steps.append({"action": "relaunch", "oldWindow": calculator,
                  "newWindow": new_calculator})
    frame("both-relaunched")
    final = snapshot()
    if set(final) != set(new_ids) or any(row["geometry"]["width"] <= 0 or
            row["geometry"]["height"] <= 0 for row in final.values()):
        raise RuntimeError("interaction-final-window-set")
    report.update(renderedPixelsObserved=True, pointerResizeObserved=True,
                  keyboardVisibleResponseObserved=True, closeRelaunchObserved=True,
                  finalWindowIds=new_ids)
    return report
