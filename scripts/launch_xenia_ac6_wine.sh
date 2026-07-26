#!/usr/bin/env bash
# Launch the pinned Windows Xenia Canary AC6 oracle through Wine and Vulkan.
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
xenia_release="16e1eb8"
xenia_directory="$repository_root/.tools/xenia-canary-windows/$xenia_release/app"
xenia_executable="$xenia_directory/xenia_canary.exe"
xenia_config="$xenia_directory/xenia-canary.config.toml"
xenia_sha256="c52d27f9a115c036257efbedd91006e74964e0c12aebb09b0c1dd93a31280f9a"
wine_prefix="$repository_root/.tools/wineprefixes/ac6-xenia-canary-$xenia_release"
ac6_xex="$repository_root/workspaces/ace-combat-6/game-files/default.xex"
codex_profile_xuid="E030000042B27D70"
codex_profile_account="$xenia_directory/content/$codex_profile_xuid/FFFE07D1/00010000/$codex_profile_xuid/Account"
service_name="ac6-xenia-wine-gui.service"
action="${1:-launch}"

check_inputs() {
    command -v wine64 >/dev/null
    command -v winepath >/dev/null
    command -v systemd-run >/dev/null
    [[ -f "$xenia_executable" ]]
    [[ -f "$xenia_config" ]]
    [[ -f "$ac6_xex" ]]
    [[ -f "$codex_profile_account" ]]
    printf '%s  %s\n' "$xenia_sha256" "$xenia_executable" | sha256sum -c - >/dev/null
    grep -Eq '^gpu = "vulkan"' "$xenia_config"
    grep -Eq '^ac6_ground_fix = true' "$xenia_config"
    grep -Eq '^hid = "winkey"' "$xenia_config"
    grep -Eq '^keyboard_mode = 1' "$xenia_config"
    grep -Eq '^keybind_start = "0x0D"' "$xenia_config"
    grep -Eq '^keybind_left_thumb_up = "_Z"' "$xenia_config"
    grep -Eq '^keybind_left_thumb_left = "_Q"' "$xenia_config"
    grep -Eq "^logged_profile_slot_0_xuid = \"$codex_profile_xuid\"" "$xenia_config"
}

case "$action" in
    --check|check)
        check_inputs
        printf 'status=ready\nrelease=%s\nrenderer=vulkan\nservice=%s\n' \
            "$xenia_release" "$service_name"
        ;;
    status)
        # `systemd-run --collect` removes a stopped transient unit.  Treat a
        # missing unit as the normal inactive state rather than surfacing
        # systemctl's diagnostic as a launcher failure.
        if systemctl --user is-active --quiet "$service_name"; then
            printf 'status=running\nservice=%s\nrelease=%s\nrenderer=vulkan\n' \
                "$service_name" "$xenia_release"
        else
            printf 'status=inactive\nservice=%s\nrelease=%s\nrenderer=vulkan\n' \
                "$service_name" "$xenia_release"
        fi
        ;;
    stop)
        if systemctl --user is-active --quiet "$service_name"; then
            systemctl --user stop "$service_name"
            printf 'status=stopped\nservice=%s\n' "$service_name"
        else
            printf 'status=inactive\nservice=%s\n' "$service_name"
        fi
        ;;
    launch)
        check_inputs
        if systemctl --user is-active --quiet "$service_name"; then
            printf 'status=already-running\nservice=%s\n' "$service_name"
            exit 0
        fi
        mkdir -p "$wine_prefix"
        chmod 700 "$wine_prefix"
        if [[ ! -f "$wine_prefix/system.reg" ]]; then
            WINEPREFIX="$wine_prefix" WINEARCH=win64 WINEDEBUG=-all wineboot -u
        fi
        ac6_xex_windows="$(WINEPREFIX="$wine_prefix" winepath -w "$ac6_xex")"
        systemd-run --user --unit="$service_name" --collect \
            --working-directory="$xenia_directory" \
            --setenv=DISPLAY="${DISPLAY:?DISPLAY is required}" \
            --setenv=WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-}" \
            --setenv=XAUTHORITY="${XAUTHORITY:?XAUTHORITY is required}" \
            --setenv=DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:?DBUS session is required}" \
            --setenv=WINEPREFIX="$wine_prefix" \
            --setenv=WINEARCH=win64 \
            --setenv=WINEDEBUG=-all \
            --setenv=WINEDLLOVERRIDES='winemenubuilder.exe=d' \
            /usr/bin/wine64 "$xenia_executable" "$ac6_xex_windows"
        printf 'status=launched\nservice=%s\nrelease=%s\nrenderer=vulkan\n' \
            "$service_name" "$xenia_release"
        ;;
    *)
        printf 'usage: %s [launch|status|stop|check]\n' "$0" >&2
        exit 2
        ;;
esac
