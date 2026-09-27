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
pipewire_pid=
wireplumber_pid=
audio_pid=
wm=
samples=$(mktemp -d /tmp/qindaqt-showcase-XXXXXX)
cleanup() {
    if [[ -n $audio_pid && -f "$run_dir/config/qindaqt/audio-console.json" ]]; then
        cp "$run_dir/config/qindaqt/audio-console.json" \
            "$stage/private-audio-console.json"
    fi
    if [[ -n $wm ]]; then
        kill "$wm" 2>/dev/null || true
        wait "$wm" 2>/dev/null || true
    fi
    if [[ -n $audio_pid ]]; then
        kill "$audio_pid" 2>/dev/null || true
        wait "$audio_pid" 2>/dev/null || true
    fi
    if [[ -n $wireplumber_pid ]]; then
        kill "$wireplumber_pid" 2>/dev/null || true
        wait "$wireplumber_pid" 2>/dev/null || true
    fi
    if [[ -n $pipewire_pid ]]; then
        kill "$pipewire_pid" 2>/dev/null || true
        wait "$pipewire_pid" 2>/dev/null || true
    fi
    rm -rf -- "$samples"
    # A portal activated by the captured application may leave its document
    # fuse path briefly inaccessible after the private bus exits.
    rm -rf -- "$run_dir" 2>/dev/null || true
}
trap cleanup EXIT
cp -a "$repo/website/assets/sample/." "$samples/"
export QINDA_SHOWCASE_SAMPLES="$samples"
export XDG_CONFIG_HOME="$run_dir/config"
export XDG_DATA_HOME="$run_dir/data"
export XDG_CACHE_HOME="$run_dir/cache"
export XDG_STATE_HOME="$run_dir/state"
export XDG_CURRENT_DESKTOP=QindaQt
printf 'XDG_DESKTOP_DIR="%s"\n' "$run_dir/Desktop" > "$run_dir/config/user-dirs.dirs"
if [[ ${QINDA_SCENE:-} == audio-* || ${QINDA_SCENE:-} == audio-set || -z ${QINDA_SCENE:-} ]]; then
    # AGENT-GUARD: Audio1 controls the PipeWire graph it connects to. The
    # showcase must never change this workstation's live volume or routes.
    mkdir -m 700 "$run_dir/runtime"
    export XDG_RUNTIME_DIR="$run_dir/runtime"
    export PIPEWIRE_RUNTIME_DIR="$run_dir/runtime"
    unset PIPEWIRE_REMOTE
    # D-Bus activation must inherit this socket too if the service restarts.
    dbus-update-activation-environment XDG_RUNTIME_DIR PIPEWIRE_RUNTIME_DIR \
        XDG_CONFIG_HOME XDG_DATA_HOME XDG_CACHE_HOME XDG_STATE_HOME
    pipewire -c /usr/share/pipewire/pipewire.conf \
        > "$stage/private-pipewire.log" 2>&1 &
    pipewire_pid=$!
    for _ in {1..50}; do
        [[ -S "$run_dir/runtime/pipewire-0" ]] && break
        sleep .1
    done
    [[ -S "$run_dir/runtime/pipewire-0" ]]
    wireplumber -p policy > "$stage/private-wireplumber.log" 2>&1 &
    wireplumber_pid=$!
    sleep 1
    pw-cli create-node adapter \
        '{ factory.name = support.null-audio-sink node.name = qindaqt.showcase.output node.description = "Showcase output" media.class = Audio/Sink object.linger = true audio.position = [ FL FR ] }' \
        > "$stage/private-output.log" 2>&1
    # Claim Audio1 on this private D-Bus before Settings can auto-activate a
    # systemd user service that carries the workstation's audio environment.
    qindaqt-audio-service > "$stage/private-audio-service.log" 2>&1 &
    audio_pid=$!
    sleep 1
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
plugin_args=()
if [[ -n ${QINDA_SITE_PLUGIN_ROOT:-} ]]; then
    plugin_args=(--plugin-root "$QINDA_SITE_PLUGIN_ROOT")
fi
qindaqt-wm "${plugin_args[@]}" --virtual --socket "$socket" --width 1440 --height 900 \
    --no-lockscreen --no-global-shortcuts \
    --test-scenario "$repo/tests/scenarios/single-1080p.json" \
    --session "$run_dir/session.sh" > "$stage/wm.log" 2>&1 &
wm=$!
for _ in {1..60}; do
    [[ -S "$XDG_RUNTIME_DIR/$socket" ]] && break
    sleep .2
done
[[ -S "$XDG_RUNTIME_DIR/$socket" ]]
export WAYLAND_DISPLAY="$socket"
sleep 4
python3 website/scripts/record-live.py
