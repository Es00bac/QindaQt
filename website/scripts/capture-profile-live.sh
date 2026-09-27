#!/usr/bin/env bash
# Capture one shipped profile with a first-party window in a private virtual session.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
cd "$repo"
profile=${1:?Usage: capture-profile-live.sh profile-id}
[[ $profile =~ ^[a-z0-9-]+$ && -f data/profiles/$profile.json ]]
if [[ ${QINDA_SITE_INNER:-} != 1 ]]; then
    QINDA_SITE_INNER=1 dbus-run-session -- bash "$0" "$profile"
    exit
fi
theme=$(python3 - "$profile" <<'PY'
import json, sys
from pathlib import Path
print(json.loads((Path('data/profiles') / f'{sys.argv[1]}.json').read_text())['defaultTheme'])
PY
)
stage="$repo/build/site-capture/profile-live-$profile"
mkdir -p "$stage"
run_dir=$(mktemp -d "$repo/build/site-capture/profile-run.XXXXXX")
app=''
wm=''
settings_pid=''
cleanup() {
    [[ -z $app ]] || kill "$app" 2>/dev/null || true
    [[ -z $wm ]] || { kill "$wm" 2>/dev/null || true; wait "$wm" 2>/dev/null || true; }
    [[ -z $settings_pid ]] || { kill "$settings_pid" 2>/dev/null || true; wait "$settings_pid" 2>/dev/null || true; }
    rm -rf -- "$run_dir"
}
trap cleanup EXIT
mkdir -p "$run_dir/config" "$run_dir/data" "$run_dir/cache" "$run_dir/state" "$run_dir/Desktop"
export XDG_CONFIG_HOME="$run_dir/config" XDG_DATA_HOME="$run_dir/data"
export XDG_CACHE_HOME="$run_dir/cache" XDG_STATE_HOME="$run_dir/state"
export XDG_CURRENT_DESKTOP=QindaQt
printf 'XDG_DESKTOP_DIR="%s"\n' "$run_dir/Desktop" > "$run_dir/config/user-dirs.dirs"
# AGENT-CONTRACT: A layout preset does not change the saved appearance.
# Seed the documented paired theme and a fitting bundled wallpaper in this
# private Settings1 store, so each capture demonstrates the complete look.
python3 - "$profile" "$theme" "$run_dir/config/qindaqt/settings-v2.json" <<'PY'
import json, sys
from pathlib import Path
profile, theme, target = sys.argv[1:]
wallpapers = {
    'qinda-bliss': 'qinda-bliss',
    'windows-modern': 'porcelain-dawn',
    'beos-inspired': 'porcelain-dawn',
    'nextstep-inspired': 'ink-tide',
    'win31-inspired': 'qinda-bliss',
}
variant = json.loads((Path('data/themes') / f'{theme}.json').read_text())['variant']
values = {'appearance.theme': theme,
          'appearance.colorScheme': 'light' if variant == 'light' else 'dark',
          'appearance.wallpaper': f"qindaqt:{wallpapers.get(profile, 'compile-club')}",
          'shell.shortcutNoteDismissed': True}
path = Path(target)
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps({'schemaVersion': 2, 'layer': 'user-overrides', 'values': values}))
PY
qindaqt-settings-service > "$stage/settings.log" 2>&1 &
settings_pid=$!
sleep .4
cat > "$run_dir/session.sh" <<EOF
#!/usr/bin/env bash
exec qindaqt-session --no-autostart --no-polkit-agent --no-powerdevil \
    --no-global-shortcut-daemon --no-input-method-daemon --welcome /bin/true \
    --profile "$profile" --theme "$theme"
EOF
chmod +x "$run_dir/session.sh"
socket="qinda-site-profile-$$"
qindaqt-wm --virtual --socket "$socket" --width 1440 --height 900 \
    --no-lockscreen --no-global-shortcuts --session "$run_dir/session.sh" \
    > "$stage/wm.log" 2>&1 &
wm=$!
for _ in {1..50}; do
    [[ -S "$XDG_RUNTIME_DIR/$socket" ]] && break
    sleep .2
done
[[ -S "$XDG_RUNTIME_DIR/$socket" ]]
export WAYLAND_DISPLAY="$socket"
sleep 4
qindaqt-file-manager /usr/share/qindaqt/wallpapers > "$stage/app.log" 2>&1 &
app=$!
sleep 3
spectacle -b -n -f -o "$stage/capture.png" > "$stage/spectacle.log" 2>&1
ffmpeg -hide_banner -loglevel error -y -i "$stage/capture.png" \
    -quality 82 "$repo/website/assets/images/layout-live-$profile.webp"
