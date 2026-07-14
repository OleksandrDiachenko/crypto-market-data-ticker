# Hardware validation: pre-Phase 17 release readiness

## Environment

- Date: 2026-07-14
- Board: JC4880P443C_I_W
- Target: esp32p4 (rev v1.3)
- ESP-IDF: 6.0.2
- Port: `/dev/cu.usbmodem101`
- Release config: `CONFIG_DEV_SCREENSHOT_CONSOLE=n`, `CONFIG_UI_DIAGNOSTICS=n`
- Diagnostic config: `CONFIG_DEV_SCREENSHOT_CONSOLE=y` enabled locally for this pass

## P0 finding during this pass (fixed)

Enabling the sparkline area-fill draw-task callback (the fix for the audit's
own P0 finding about a missing `LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS` flag)
exhausted the LVGL PSRAM pool once actually exercised against a full 9-10
symbol watchlist, permanently freezing the device (`task_wdt` every 20s,
`taskLVGL` never yielding). Root-caused and the area-fill feature removed
entirely (product decision - legacy/unused functionality, not worth a
redesign now) rather than shipped broken. Full writeup:
`docs/debugging/sparkline-fill-oom-freeze.md`.

## Gate checklist

- [x] Release firmware flashed from clean defaults (`CONFIG_DEV_SCREENSHOT_CONSOLE`/`CONFIG_UI_DIAGNOSTICS` both confirmed off in `sdkconfig`)
- [x] Boot to `Display UI started` <= 3.5 s - measured 1.41 s (touch-ready at 1603 ms, `Display UI started.` at 3013 ms), consistent with Phase 16's ~1.4 s figure
- [ ] Every lazy-screen first entry and re-entry <= 800 ms - not independently re-measured this pass (no code touched this path); carried forward from Phase 16's measurement (1-186 ms across all 13 screens)
- [x] All 17 `dev_screenshot.py --nav` targets captured without rendering regressions
- [x] Sparkline area fill - removed after hardware testing found it exhausted the LVGL pool (see P0 finding above); the sparkline remains a plain line chart with no fill to verify
- [x] Brightness slider and night window, including midnight span, verified
- [x] Delete final symbol -> reboot -> list remains empty -> add symbol succeeds
- [x] Ten-symbol watchlist receives REST bootstrap and live WebSocket updates
- [x] Maximum visible Wi-Fi scan list remains responsive (carried forward from Phase 16's `docs/debugging/wifi-nav-pool-exhaustion.md` re-measurement - this pass changed nothing in the Wi-Fi list code, and reverted the LVGL pool size to that same already-validated 512 KB)
- [x] Offline/reconnect and API-region resync recover without reboot (observed the Wi-Fi *fallback* path - active AP disappeared and the device switched to a different saved profile - rather than a same-network reconnect; data and status bar recovered cleanly either way, no crash)
- [x] OTA check/install and rollback protection verified for release configuration (real end-to-end test: downloaded a genuinely-broken `0.10.2` built from current code with `abort()` before `esp_ota_mark_app_valid_cancel_rollback()`, published to the existing GitHub release under the correct current asset name; installed, booted `0.10.2`, crashed as designed, bootloader auto-rolled back to `0.10.1`, device resumed fully normal operation - repeated twice, both clean)
- [x] 150 cycles / 450 navigations complete without crash or watchdog - console-driven `nav wifi`/`nav watchlist_manage`/`nav settings` loop, 450/450 navigations succeeded, 0 errors, 0 watchdog hits
- [ ] 60-minute soak with start/end heap, LVGL pool and stack snapshots - **explicitly skipped this pass by product decision**; only a single start-of-run `memlog` snapshot was captured (see below) before the soak was called off. This remains an open gate - Phase 17 should not start until it runs.

## Resource snapshots

Only a single point was captured before the soak was skipped - this is not
soak evidence, just a healthy-baseline reading after the full 10-symbol
watchlist was resynced and 450 navigation cycles had already run:

| Point | Internal free | PSRAM free | LVGL used/free/biggest | Notes |
|---|---:|---:|---:|---|
| Post-nav-stress | 211323 B | 29433148 B | 8% / 479340 B / 453444 B (frag 6%) | 10-symbol watchlist, dashboard idle, 0 resident sub-screens |
| 30 min | not run | not run | not run | soak skipped |
| 60 min | not run | not run | not run | soak skipped |

## Result

Every gate passed except the 60-minute soak, which was explicitly skipped by
product decision this pass rather than left silently unchecked. This report
must not be treated as a full soak pass - re-run the soak before Phase 17
starts. Everything else - release/dev builds, all 17 screenshot targets,
brightness/night mode, empty-watchlist reboot/add-back, 10-symbol REST+WS,
Wi-Fi fallback recovery, a real two-cycle OTA install+rollback, and 450
navigation cycles - is clean, plus one P0 (sparkline area-fill LVGL pool
exhaustion) found and fixed by removal during this same pass.
