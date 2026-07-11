# 0011: Display brightness and night mode

## Status

Accepted (2026-07-11)

## Context

`display_settings_t` has carried a `brightness_percent` field (default 80,
range 1-100) with full codec/store/host-test support since it was first
added, and its `reserved2` padding was explicitly commented as "room for
theme/timeout once the board supports more than on/off backlight." Neither
promise had been kept: `board_jc4880p443c`'s LEDC backlight channel only
ever drove full duty (`board_jc4880p443c_backlight_on()`) or zero
(`_backlight_off()`), `brightness_percent` was never loaded or applied
anywhere, and there was no Settings UI to change it. This phase closes that
gap and adds a night-mode window that dims the display to a fixed minimum
between a user-configured start and end time.

## Decision

### 1. A new `Settings > Display` screen, not a Display sub-menu of Time

`Display` is added as its own top-level row in `build_settings_list()`
(`main/display_ui.c`), between "Time" and "Watchlist symbols" - a sibling of
Wi-Fi/Time/Watchlist/Updates/About, not nested under Time, since brightness
and night mode are unrelated to locale/timezone despite both needing
wall-clock time. LVGL's built-in symbol set has no sun/brightness glyph, so
`LV_SYMBOL_EYE_OPEN` is used as the row icon - the same kind of
closest-available-placeholder choice already made for "Time" (which uses
the gear icon).

### 2. `display_settings_t` grows in place (v1 -> v2), not a new domain

Five fields are appended to the existing struct rather than introducing a
parallel `night_mode` settings domain: `night_mode_enabled`,
`night_start_hour`/`_minute`, `night_end_hour`/`_minute`. This matches how
`locale_settings_t` (v1->v3) and `api_region_settings_t` (v1->v2) have grown
in previous phases, and keeps brightness and night mode - one logical
"Display" screen - under one load/save call
(`settings_store_load_display()`/`_save_display()`, unchanged API). There is
still no migration engine, so any already-persisted v1 blob resets to
defaults on first boot after this update (brightness 80%, night mode off,
22:00-07:00) - acceptable since `brightness_percent` was never actually
applied by any prior build, so nothing meaningful is lost.

### 3. A fixed night-mode floor, not a second user-configurable level

Night mode dims to a compile-time constant, `DISPLAY_NIGHT_BRIGHTNESS_PERCENT`
(10), rather than adding a second slider. This is also the slider's own
floor (`DISPLAY_BRIGHTNESS_MIN_PERCENT`, same value 10) - one constant to
reason about for "how dim can this get," and night mode is guaranteed to
never be brighter than the user's own minimum setting. A user who wants a
different night level always has the option of simply lowering their base
brightness.

### 4. New native widgets (`lv_slider`, `lv_switch`, `lv_roller`), styled like the existing textarea/keyboard

Before this phase, `display_ui.c` had no `lv_slider`, `lv_switch`, or
`lv_roller` anywhere - every existing "choice" control (Time format,
Region, Date format) is a hand-built checkmark row
(`build_time_toggle_row()`/`build_selectable_row()`), since a fixed list of
named options doesn't need a continuous, on/off, or wheel-style control.
Brightness (a continuous 10-100% range), night mode's on/off, and a
start/end clock time each need one, so this phase adds
`style_dark_slider()`/`style_dark_switch()`/`style_dark_roller()` next to
the file's existing `style_dark_textarea()`/`style_dark_keyboard()` - the
same "restyle LVGL's default light theme to match this screen's dark
palette" treatment, not a new pattern. `COLOR_ACCENT` (the file's one
bright-blue highlight, used sparingly elsewhere for a small icon chip or
checkmark) reads as glaring across the slider's fill and the switch's
checked track, so both use a new `COLOR_ACCENT_MUTED` - a desaturated,
darker version of the same hue - for any large filled area instead.

An earlier version of this phase reused `build_time_toggle_row()`'s
checkmark for night mode's on/off, matching every other choice-row in
Settings. In review this read ambiguously as a list selection rather than
an on/off control, so it was replaced with `build_switch_row()` (a title +
`lv_switch`, only the switch itself tappable) - an unambiguous toggle for a
control that is genuinely boolean, not one option among several.

### 5. A modal msgbox for the start/end time picker, not a dedicated screen

An earlier version of this phase gave the Start/End rows their own
`SETTINGS_VIEW_NIGHT_TIME` sub-screen, following the same
built-once-shown-on-demand pattern as every other Settings sub-screen. In
review this was more page than two rollers justify, so it was replaced with
`lv_msgbox` (LVGL's native modal dialog, `LV_USE_MSGBOX`, already enabled):
built fresh on every Start/End tap via `night_time_open(bool editing_start)`
and deleted on close, rather than pre-built and hidden/shown like the rest
of Settings.

The dialog went through two more review rounds before landing on its final
shape:

- **No title, close button, or footer buttons.** `lv_msgbox_add_title()` /
  `_add_close_button()` / `_add_footer_button()` were tried first (with
  every part - root, header, close button, content, footer, footer buttons
  - needing its own dark-theme override, since LVGL's default theme is
  light); all were dropped as pure chrome around what is really just two
  rollers. There is no explicit confirm/cancel action at all: tapping
  anywhere outside the rollers commits the roller selection and closes the
  dialog (`night_time_backdrop_click_cb()`, wired to `LV_EVENT_CLICKED` on
  the auto-created backdrop object `lv_msgbox_create(NULL)` returns as the
  msgbox's parent) - matching a wheel picker's usual "scroll, then dismiss"
  interaction. A tap lands on the backdrop's handler only when it doesn't
  land on the rollers (or the msgbox/content container around them) first,
  since LVGL doesn't bubble a consumed click up to a parent by default -
  that's what makes "tap outside the rollers" and "tap the backdrop"
  equivalent here.
- **No card background.** The msgbox root and its content area are both set
  fully transparent (`LV_OPA_TRANSP`); only the rollers (already
  dark-styled via `style_dark_roller()`) are visible, floating directly on
  the dimmed backdrop instead of sitting inside a second, redundant panel.

The backdrop itself keeps an explicit dark override -
`lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0)` at `LV_OPA_80` -
since LVGL's default modal backdrop is a light gray that would otherwise
clash with the rest of this screen's dark palette.

Which field is being edited (Start or End) is tracked in a module-static
`s_night_time_editing_start`, set when the dialog opens and read back on
commit. The minute roller steps in 5s (00, 05, ..., 55) rather than every
minute - a night-mode window doesn't need minute-level precision, and a
12-item wheel is easier to scan/scroll than 60.

Because `lv_msgbox_create(NULL)` parents the dialog to `lv_layer_top()` - a
sibling of the one root screen every other panel in this file lives under,
not a descendant of it - `dev_screenshot_console.c`'s `cmd_screenshot()`
needed a small change: it now snapshots `lv_layer_top()` instead of
`lv_screen_active()` whenever the top layer actually has a child, so the
open dialog is visible in a captured screenshot instead of being silently
skipped.

### 6. Brightness/night-mode application follows the existing clock idiom, applied every second

`display_apply_brightness()` computes the effective target percent (the
user's `brightness_percent`, or `DISPLAY_NIGHT_BRIGHTNESS_PERCENT` when
night mode is enabled, synced, and the local time falls in
`[start, end)`) using the exact `time_sync_is_synced()` +
`localtime_r(time(NULL), &tm)` idiom `update_statusbar()` already uses to
render the clock - no new time-reading code path. A `start > end` window
(e.g. 22:00 -> 07:00) is treated as spanning midnight. It is called from
`update_timer_cb()` (already ticking every `UPDATE_PERIOD_MS` = 1s) instead
of a second dedicated `lv_timer`, and only actually writes to the LEDC duty
when the computed target changes from the last-applied value
(`s_display_last_applied_percent`), making the per-second call a cheap
comparison in the common case. It is also called once after
`settings_store_load_display()` in `display_ui_render()`, to correct the
always-full-duty `board_jc4880p443c_backlight_on()` boot sequence
(`display_ui_start()`) to the persisted/effective level as soon as settings
are loaded.

### 7. A new `board_jc4880p443c_backlight_set_percent()` API, reusing the existing LEDC channel

`board_jc4880p443c.c` already configures a 10-bit LEDC PWM channel for the
backlight (`backlight_pwm_init()`), but only ever drove it to full (1023)
or zero duty. The new function maps a 0-100 percent (clamped) to a 10-bit
duty via `percent * 1023 / 100` and reuses the same
`ledc_set_duty`+`ledc_update_duty` pair `backlight_on()`/`backlight_off()`
already call - no new channel, timer, or GPIO.

### 8. Dev-screenshot `nav` gains `display` and `night_time` targets

`cmd_nav()` (`main/display_ui.c`, gated by `CONFIG_DEV_SCREENSHOT_CONSOLE`)
and `tools/dev_screenshot.py`'s `--nav` choices both gain `display` and
`night_time`, following the existing pattern for every other Settings
sub-screen - needed to visually verify the new screens on hardware without
a physical tap.

## Consequences

- `settings_store` host tests (`test_settings_codec_display.c`) gain cases
  for the new fields' roundtrip, defaults, and out-of-range rejection
  (hour > 23, minute > 59, `night_mode_enabled` > 1).
- Any device already in the field with a persisted `display_settings_t`
  blob resets to v2 defaults on first boot after this update (version
  bump), same category of consequence as 0007's and 0009's prior bumps.
- `nav`'s console help text and `dev_screenshot.py`'s `--nav` choices both
  needed updating in lockstep - a Settings sub-screen (or, for `night_time`,
  a msgbox-opening target) added without touching both leaves it
  screenshot-unreachable without a physical tap.
- The night-mode time picker's modal is genuinely modal (its backdrop
  intercepts all input), so a stray `nav` console command issued while it's
  open (e.g. `nav display` right after `nav night_time`) leaves it stuck on
  screen rather than switching views - matches real touch behavior (nothing
  behind a modal is reachable until it closes) but is a footgun for anyone
  scripting `nav` calls back-to-back without an intervening tap outside the
  dialog to dismiss it.

## Alternatives considered

- **A user-configurable night brightness level (a second slider)**:
  rejected as unnecessary scope for the first version of this feature - a
  fixed floor tied to the same constant as the main slider's minimum is
  simpler to reason about and covers the stated requirement ("minimum
  brightness at night").
- **A per-minute roller (60 items) instead of 5-minute steps**: rejected -
  a night-mode window doesn't need minute precision, and a shorter wheel is
  easier to scroll to on a touchscreen.
- **A dedicated `lv_timer` for the brightness/night-mode recheck**:
  rejected - `update_timer_cb()` already runs every second and already
  reads local time for the clock; a second timer on the same cadence would
  be redundant.
- **A checkmark row for night mode's on/off (matching every other
  choice-row in Settings)**: tried first, then rejected in review - it read
  as one selectable option rather than an unambiguous on/off toggle. A
  genuine `lv_switch` removes the ambiguity at the cost of being this file's
  first use of that widget.
- **A dedicated `SETTINGS_VIEW_NIGHT_TIME` sub-screen for the time
  picker**: tried first, then rejected in review as disproportionate to its
  content (two rollers) - a modal `lv_msgbox` covers the same need without a
  full page, at the cost of the dev-screenshot tool needing to know about
  `lv_layer_top()`.
- **Explicit Cancel/Set footer buttons on the msgbox**: tried next (styled
  large, ~160x64px, for an easy touch target), then rejected in review as
  unnecessary given the dialog is just two rollers - a tap outside them
  already reads clearly as "I'm done," so a confirm button was pure
  redundancy and Cancel had nothing to protect against (there's no
  destructive action here, just a chosen time).
