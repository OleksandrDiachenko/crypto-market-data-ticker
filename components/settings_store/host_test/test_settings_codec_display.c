#include "test_common.h"
#include "settings_codec.h"

static void test_roundtrip(void)
{
    display_settings_t cfg;
    settings_display_init_default(&cfg);
    cfg.brightness_percent = 42;
    cfg.night_mode_enabled = 1;
    cfg.night_start_hour = 23;
    cfg.night_start_minute = 30;
    cfg.night_end_hour = 6;
    cfg.night_end_minute = 45;

    settings_display_seal(&cfg);

    CHECK(settings_display_validate(&cfg) == SETTINGS_CODEC_OK);
}

static void test_defaults(void)
{
    display_settings_t cfg;
    settings_display_init_default(&cfg);

    CHECK(cfg.brightness_percent == 80);
    CHECK(cfg.night_mode_enabled == 0);
    CHECK(cfg.night_start_hour == 22);
    CHECK(cfg.night_start_minute == 0);
    CHECK(cfg.night_end_hour == 7);
    CHECK(cfg.night_end_minute == 0);
}

static void test_bad_crc_rejected(void)
{
    display_settings_t cfg;
    settings_display_init_default(&cfg);
    settings_display_seal(&cfg);
    cfg.crc32 ^= 0xFFFFFFFFu;

    CHECK(settings_display_validate(&cfg) == SETTINGS_CODEC_BAD_CRC);
}

static void test_bad_magic_rejected(void)
{
    display_settings_t cfg;
    settings_display_init_default(&cfg);
    settings_display_seal(&cfg);
    cfg.magic = 0xDEADBEEFu;

    CHECK(settings_display_validate(&cfg) == SETTINGS_CODEC_BAD_MAGIC);
}

static void test_bad_version_rejected(void)
{
    display_settings_t cfg;
    settings_display_init_default(&cfg);
    settings_display_seal(&cfg);
    cfg.version = 99;

    CHECK(settings_display_validate(&cfg) == SETTINGS_CODEC_BAD_VERSION);
}

static void test_out_of_range_brightness_rejected(void)
{
    display_settings_t cfg;
    settings_display_init_default(&cfg);
    cfg.brightness_percent = 0;
    settings_display_seal(&cfg);

    CHECK(settings_display_validate(&cfg) == SETTINGS_CODEC_BAD_RANGE);
}

static void test_out_of_range_night_hour_rejected(void)
{
    display_settings_t cfg;
    settings_display_init_default(&cfg);
    cfg.night_start_hour = 24;
    settings_display_seal(&cfg);

    CHECK(settings_display_validate(&cfg) == SETTINGS_CODEC_BAD_RANGE);
}

static void test_out_of_range_night_minute_rejected(void)
{
    display_settings_t cfg;
    settings_display_init_default(&cfg);
    cfg.night_end_minute = 60;
    settings_display_seal(&cfg);

    CHECK(settings_display_validate(&cfg) == SETTINGS_CODEC_BAD_RANGE);
}

static void test_out_of_range_night_enabled_rejected(void)
{
    display_settings_t cfg;
    settings_display_init_default(&cfg);
    cfg.night_mode_enabled = 2;
    settings_display_seal(&cfg);

    CHECK(settings_display_validate(&cfg) == SETTINGS_CODEC_BAD_RANGE);
}

int main(void)
{
    test_roundtrip();
    test_defaults();
    test_bad_crc_rejected();
    test_bad_magic_rejected();
    test_bad_version_rejected();
    test_out_of_range_brightness_rejected();
    test_out_of_range_night_hour_rejected();
    test_out_of_range_night_minute_rejected();
    test_out_of_range_night_enabled_rejected();
    return test_summary("settings_codec_display");
}
