#include "test_common.h"
#include "settings_codec.h"

static void test_roundtrip(void)
{
    api_region_settings_t cfg;
    settings_api_region_init_default(&cfg);
    cfg.region = SETTINGS_API_REGION_US;

    settings_api_region_seal(&cfg);

    CHECK(settings_api_region_validate(&cfg) == SETTINGS_CODEC_OK);
    CHECK(cfg.region == SETTINGS_API_REGION_US);
}

static void test_default_is_intl(void)
{
    api_region_settings_t cfg;
    settings_api_region_init_default(&cfg);
    settings_api_region_seal(&cfg);

    CHECK(settings_api_region_validate(&cfg) == SETTINGS_CODEC_OK);
    CHECK(cfg.region == SETTINGS_API_REGION_INTL);
}

static void test_bad_crc_rejected(void)
{
    api_region_settings_t cfg;
    settings_api_region_init_default(&cfg);
    settings_api_region_seal(&cfg);
    cfg.crc32 ^= 0xFFFFFFFFu;

    CHECK(settings_api_region_validate(&cfg) == SETTINGS_CODEC_BAD_CRC);
}

static void test_bad_magic_rejected(void)
{
    api_region_settings_t cfg;
    settings_api_region_init_default(&cfg);
    settings_api_region_seal(&cfg);
    cfg.magic = 0xDEADBEEFu;

    CHECK(settings_api_region_validate(&cfg) == SETTINGS_CODEC_BAD_MAGIC);
}

static void test_bad_version_rejected(void)
{
    api_region_settings_t cfg;
    settings_api_region_init_default(&cfg);
    settings_api_region_seal(&cfg);
    cfg.version = 99;

    CHECK(settings_api_region_validate(&cfg) == SETTINGS_CODEC_BAD_VERSION);
}

static void test_region_out_of_range_rejected(void)
{
    api_region_settings_t cfg;
    settings_api_region_init_default(&cfg);
    cfg.region = 2;
    settings_api_region_seal(&cfg);

    CHECK(settings_api_region_validate(&cfg) == SETTINGS_CODEC_BAD_RANGE);
}

int main(void)
{
    test_roundtrip();
    test_default_is_intl();
    test_bad_crc_rejected();
    test_bad_magic_rejected();
    test_bad_version_rejected();
    test_region_out_of_range_rejected();
    return test_summary("settings_codec_api_region");
}
