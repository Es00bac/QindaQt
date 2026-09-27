#!/usr/bin/env bash
# Record only public sample content in a private virtual QindaQt session.
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
cd "$repo"
mkdir -p "$repo/build/site-capture"
if [[ ${QINDA_SITE_INNER:-} != 1 ]]; then
    QINDA_SITE_INNER=1 dbus-run-session -- bash "$0" > "$repo/build/site-capture/record-live.log" 2>&1
    exit
fi

stage="$repo/build/site-capture/live"
mkdir -p "$stage"
# AGENT-GUARD: QindaOffice recovery journals use XDG_STATE_HOME. Reusing
# it can cover the sample workflow with a previous run's recovery dialog.
run_dir=$(mktemp -d "$stage/run.XXXXXX")
mkdir -p "$run_dir/config" "$run_dir/data" "$run_dir/cache" "$run_dir/state" "$run_dir/Desktop"
samples=$(mktemp -d /tmp/qindaqt-showcase-XXXXXX)
cp -a "$repo/website/assets/sample/." "$samples/"
export QINDA_SHOWCASE_SAMPLES="$samples"
export XDG_CONFIG_HOME="$run_dir/config"
export XDG_DATA_HOME="$run_dir/data"
export XDG_CACHE_HOME="$run_dir/cache"
export XDG_STATE_HOME="$run_dir/state"
export XDG_CURRENT_DESKTOP=QindaQt
printf 'XDG_DESKTOP_DIR="%s"\n' "$run_dir/Desktop" > "$run_dir/config/user-dirs.dirs"
if [[ ${QINDA_SCENE:-} == audio-* ]]; then
    mkdir -p "$run_dir/config/qindaqt"
    printf '{"schemaVersion":2,"layer":"user-overrides","values":{"shell.shortcutNoteDismissed":true}}\n' \
        > "$run_dir/config/qindaqt/settings-v2.json"
    cat > "$run_dir/session.sh" <<'EOF'
#!/usr/bin/env bash
exec qindaqt-session --no-autostart --no-polkit-agent \
    --no-powerdevil --no-input-method-daemon --welcome /bin/true \
    --profile qindaqt --theme qinda-dark
EOF
else
    cat > "$run_dir/session.sh" <<'EOF'
#!/usr/bin/env bash
exec qindaqt-session --no-autostart --no-polkit-agent \
    --no-powerdevil --no-input-method-daemon \
    --profile macos-inspired --theme qinda-dark
EOF
fi
chmod +x "$run_dir/session.sh"

socket="qinda-record-$$"
qindaqt-wm --virtual --socket "$socket" --width 1440 --height 900 \
    --no-lockscreen --no-global-shortcuts \
    --test-scenario "$repo/tests/scenarios/single-1080p.json" \
    --session "$run_dir/session.sh" > "$stage/wm.log" 2>&1 &
wm=$!
cleanup() {
    kill "$wm" 2>/dev/null || true
    wait "$wm" 2>/dev/null || true
    rm -rf -- "$samples"
    rm -rf -- "$run_dir"
}
trap cleanup EXIT
for _ in {1..60}; do
    [[ -S "$XDG_RUNTIME_DIR/$socket" ]] && break
    sleep .2
done
[[ -S "$XDG_RUNTIME_DIR/$socket" ]]
export WAYLAND_DISPLAY="$socket"
sleep 4
python3 website/scripts/record-live.py
