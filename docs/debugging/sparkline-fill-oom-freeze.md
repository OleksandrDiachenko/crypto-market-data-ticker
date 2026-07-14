# Debug Report: Permanent freeze after watchlist sync (sparkline area-fill)

## Symptom

Shortly after boot - once the whole watchlist finished its first REST sync -
the device froze permanently: touch went dead, the dev screenshot console
stopped responding, and `task_wdt` fired every 20s naming `taskLVGL` on CPU 0,
forever (no reboot, since `CONFIG_ESP_TASK_WDT_PANIC` is off).

## Environment

- Date: 2026-07-14
- Board: JC4880P443C_I_W
- Target: esp32p4 (rev v1.3)
- ESP-IDF: v6.0.2
- Firmware: `chore/pre-phase-17-code-audit` (PR #82, merged to `main` at
  `73632cc`), dev config (`CONFIG_DEV_SCREENSHOT_CONSOLE=y`)
- Watchlist: 9 symbols persisted from a previous session

## Steps To Reproduce

Flash the merged `main` (post-PR #82) dev-config build, connect to Wi-Fi, let
the watchlist's first REST sync complete for all persisted symbols.

## Expected Result

Dashboard renders the 9-10 watchlist rows with sparkline + area-fill (the
draw-task callback this same PR enabled) and stays responsive indefinitely.

## Actual Result

The device froze within ~23 seconds of the last symbol's REST sync
completing. Serial log:

```
[Error] (23.445, +23445) allocate_item: Asserted at expression: item != NULL (Out of memory) lv_draw_sw_grad.c:112
E (86019) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
E (86019) task_wdt:  - IDLE0 (CPU 0)
E (86019) task_wdt: Tasks currently running:
E (86019) task_wdt: CPU 0: taskLVGL
E (86019) task_wdt: CPU 1: IDLE1
```

repeating every ~20s indefinitely. Symbolized backtrace (against the running
`.elf`):

```
allocate_item                lv_draw_sw_grad.c:112
lv_draw_sw_grad_get          lv_draw_sw_grad.c:151
lv_draw_sw_triangle          lv_draw_sw_triangle.c:142
execute_drawing               lv_draw_sw.c:418
dispatch                      lv_draw_sw.c:333
lv_draw_dispatch_layer        lv_draw.c:287
lv_draw_dispatch              lv_draw.c:222
draw_buf_flush                lv_refr.c:1381
refr_invalid_areas             lv_refr.c:802
lv_display_refr_timer          lv_refr.c:421
lv_timer_exec / lv_timer_handler
lvgl_port_task                 esp_lvgl_port.c:242
```

`lv_malloc()` inside `allocate_item()` returned `NULL`; LVGL's default
`LV_ASSERT_MALLOC` handler traps in an infinite `while(1)` on the LVGL task -
the same catastrophic failure mode already documented in
`wifi-nav-pool-exhaustion.md`, this time triggered by a different feature.

## Investigation

The sparkline area-fill (`chart_draw_event_cb`, added in the pre-Phase-17
audit to replicate `lv_demo_widgets_analytics`'s technique) hooks
`LV_EVENT_DRAW_TASK_ADDED` and, for every pair of adjacent points in the
chart's line, issues one gradient-filled triangle plus one gradient-filled
rectangle. With `MARKET_DATA_KLINES_V1_LIMIT` = 288 points per row and up to
`SETTINGS_MAX_WATCHLIST` = 10 rows, that is up to 10 x 287 x 2 ~= 5740 small
`lv_malloc()` calls (via `lv_draw_sw_grad_get()`) per full chart redraw.

That alone would only be a one-time peak, and doubling the LVGL PSRAM pool
(512 -> 1024 KB, tried first) only delayed the crash from ~23s to ~85s rather
than fixing it - proving this is not a single oversized peak but an
accumulating cost. The reason: `update_timer_cb()` calls `update_row()` for
**every row, every second, unconditionally** (`UPDATE_PERIOD_MS`), and
`update_row()` has no dirty-check before `lv_chart_set_point_count()` /
`lv_chart_set_series_ext_y_array()` / `lv_chart_refresh()` - so the full
~5740-allocation draw storm repeats every second forever, not just once at
boot. At that churn rate, even without a true leak, cumulative TLSF pool
fragmentation from tens of thousands of alloc/free cycles per minute
eventually fails to find a contiguous block, regardless of pool size.

This also breaks Phase 11's own accepted design ("no full-list redraw per
tick") - that criterion was met for label/price updates, but the sparkline's
per-tick full redraw (present since Phase 11, just visually cheap before this
audit enabled the fill's per-segment gradient draws) was already an
exception to it.

## Root Cause

`chart_draw_event_cb`'s per-segment gradient-fill technique, combined with an
unconditional full per-second chart redraw for every watchlist row, produces
thousands of transient small allocations per second against a fixed-size LVGL
pool. Enlarging the pool only delays the inevitable; the draw volume itself
is the problem.

## Fix

Removed the sparkline area-fill feature entirely (per product decision - it
was legacy/experimental functionality, not worth a redesign right now):

- Deleted `chart_draw_event_cb()` (main/display_ui.c).
- Removed the `LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS` flag and the
  `LV_EVENT_DRAW_TASK_ADDED` callback registration on each row's chart.
- Reverted the LVGL pool size probe (1024 KB) back to the prior, already
  hardware-validated `CONFIG_LV_MEM_SIZE_KILOBYTES=512`.

The sparkline itself (plain `LV_CHART_TYPE_LINE`, no fill) is unaffected and
was never part of this bug - only the fill's draw-task callback was.

## Validation

- `idf.py build` clean (dev config).
- Flashed to JC4880P443C_I_W: full 9-symbol REST bootstrap, then a 10th
  symbol (`DOTUSDT`) added live via touch, all synced - 90+ seconds of quiet
  operation with zero watchdog errors (previously crashed at 23s).
- `dev_screenshot.py --nav watchlist` succeeds and shows all 10 rows with
  plain line sparklines, confirming the console/UI stayed responsive.
- Host tests unaffected (`chart_draw_event_cb` had no host-test coverage -
  LVGL draw callbacks aren't host-testable).

## Follow-up

- If area-fill is revisited later, it needs either a dirty-check in
  `update_row()` (skip the chart update entirely when the underlying klines
  haven't changed since the last tick) or a redesign that draws the fill as
  one shape per chart instead of one gradient pair per segment - not both
  problems solved by a bigger pool.
- The `update_timer_cb()` per-second unconditional full chart redraw
  predates this bug and remains in place (cheap for the plain line chart);
  worth a dirty-check regardless, since it already contradicts Phase 11's
  "no full-list redraw per tick" criterion.
