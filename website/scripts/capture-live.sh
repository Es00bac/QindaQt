#!/usr/bin/env bash
# Capture public sample content in an isolated virtual QindaQt session.
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
cd "$repo"
if [[ ${QINDA_SITE_INNER:-} != 1 ]]; then
    QINDA_SITE_INNER=1 dbus-run-session -- bash "$0"
    exit
fi

stage="$repo/build/site-capture/showcase"
mkdir -p "$stage/images"
# AGENT-GUARD: Fresh XDG state prevents a previous QindaCalc autosave from
# covering a public screenshot with its recovery dialog.
run_dir=$(mktemp -d "$stage/run.XXXXXX")
mkdir -p "$run_dir/config" "$run_dir/data" "$run_dir/cache" "$run_dir/state" "$run_dir/Desktop"
samples=$(mktemp -d /tmp/qindaqt-showcase-XXXXXX)
cp -a "$repo/website/assets/sample/." "$samples/"
export XDG_CONFIG_HOME="$run_dir/config"
export XDG_DATA_HOME="$run_dir/data"
export XDG_CACHE_HOME="$run_dir/cache"
export XDG_STATE_HOME="$run_dir/state"
export XDG_CURRENT_DESKTOP=QindaQt
printf 'XDG_DESKTOP_DIR="%s"\n' "$run_dir/Desktop" > "$run_dir/config/user-dirs.dirs"
cat > "$run_dir/session.sh" <<'EOF'
#!/usr/bin/env bash
exec qindaqt-session --no-autostart --no-polkit-agent \
    --no-powerdevil --no-input-method-daemon \
    --profile macos-inspired --theme qinda-dark
EOF
chmod +x "$run_dir/session.sh"

socket="qinda-showcase-$$"
qindaqt-wm --virtual --socket "$socket" --width 1440 --height 900 \
    --no-lockscreen --no-global-shortcuts --session "$run_dir/session.sh" \
    > "$stage/wm.log" 2>&1 &
wm=$!
app=''
cleanup() {
    [[ -z $app ]] || kill "$app" 2>/dev/null || true
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

capture() {
    local name=$1
    spectacle -b -n -f -o "$stage/images/$name.png" > "$stage/$name.log" 2>&1
    ffmpeg -hide_banner -loglevel error -y -i "$stage/images/$name.png" \
        -quality 82 "$repo/website/assets/images/$name.webp"
}
show_app() {
    local name=$1
    shift
    "$@" > "$stage/$name-app.log" 2>&1 &
    app=$!
    sleep 5
    capture "$name"
    kill "$app" 2>/dev/null || true
    wait "$app" 2>/dev/null || true
    app=''
    sleep .5
}

capture desktop-welcome
show_app desktop-settings qindaqt-settings
show_app desktop-files qindaqt-file-manager /usr/share/qindaqt/wallpapers
show_app office-write qindawrite --new-instance "$samples/signal-garden-brief.docx"
show_app office-calc qindacalc --new-instance "$samples/signal-garden-budget.xlsx"
show_app office-chart qindacalc --new-instance "$samples/signal-garden-scenarios.xlsx"
show_app office-show qindashow --new-instance "$samples/signal-garden-pitch.fodp"
show_app office-note qindanote --new-instance "$samples/signal-garden-notes"
