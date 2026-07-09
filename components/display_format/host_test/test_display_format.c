#include "display_format.h"
#include "test_common.h"

static void test_price(void)
{
    char buf[32];

    display_format_price(43250.5, buf, sizeof(buf));
    CHECK_STREQ(buf, "43250.50");

    display_format_price(1.0, buf, sizeof(buf));
    CHECK_STREQ(buf, "1.00");

    display_format_price(100.0, buf, sizeof(buf));
    CHECK_STREQ(buf, "100.00");

    display_format_price(0.0, buf, sizeof(buf));
    CHECK_STREQ(buf, "0.00");

    // Sub-$1 adaptive precision: decimals grow as magnitude shrinks.
    display_format_price(0.4521, buf, sizeof(buf));
    CHECK_STREQ(buf, "0.4521");

    display_format_price(0.04521, buf, sizeof(buf));
    CHECK_STREQ(buf, "0.04521");

    display_format_price(0.0004521, buf, sizeof(buf));
    CHECK_STREQ(buf, "0.0004521");

    // Micro-cap price (e.g. SHIB/PEPE) - hits the 8-decimal cap.
    display_format_price(0.00000734, buf, sizeof(buf));
    CHECK_STREQ(buf, "0.00000734");

    // Even smaller than the cap can represent precisely - still clamps at 8.
    display_format_price(0.0000000123, buf, sizeof(buf));
    CHECK_STREQ(buf, "0.00000001");

    // Exact power-of-ten boundary: guards against fp log10() rounding
    // landing just under the integer threshold (off-by-one in decimals).
    display_format_price(0.01, buf, sizeof(buf));
    CHECK_STREQ(buf, "0.010000");

    // >9-integer-digit threshold: exactly at 999,999,999.99 stays unabbreviated...
    display_format_price(999999999.99, buf, sizeof(buf));
    CHECK_STREQ(buf, "999999999.99");

    // ...but 1e9 (10 integer digits) abbreviates.
    display_format_price(1e9, buf, sizeof(buf));
    CHECK_STREQ(buf, "1.00B");

    display_format_price(1.25e12, buf, sizeof(buf));
    CHECK_STREQ(buf, "1.25T");
}

static void test_abbreviate(void)
{
    char buf[32];

    display_format_abbreviate(999.0, buf, sizeof(buf));
    CHECK_STREQ(buf, "999.00");

    display_format_abbreviate(1000.0, buf, sizeof(buf));
    CHECK_STREQ(buf, "1.00K");

    display_format_abbreviate(1234567.0, buf, sizeof(buf));
    CHECK_STREQ(buf, "1.23M");

    display_format_abbreviate(5e9, buf, sizeof(buf));
    CHECK_STREQ(buf, "5.00B");

    display_format_abbreviate(1.25e12, buf, sizeof(buf));
    CHECK_STREQ(buf, "1.25T");

    display_format_abbreviate(-1234567.0, buf, sizeof(buf));
    CHECK_STREQ(buf, "-1.23M");
}

static void test_normalize_value(void)
{
    // Degenerate range (flat market): every value maps to the centered mid-point.
    CHECK(display_format_normalize_value(1.0, 1.0, 1.0, 10000) == 5000);
    CHECK(display_format_normalize_value(0.0, 0.0, 0.0, 10000) == 5000);
    CHECK(display_format_normalize_value(999.0, 5.0, 4.0, 10000) == 5000); // hi < lo guard

    // Normal range: endpoints and midpoint map exactly.
    CHECK(display_format_normalize_value(0.0, 0.0, 100.0, 10000) == 0);
    CHECK(display_format_normalize_value(100.0, 0.0, 100.0, 10000) == 10000);
    CHECK(display_format_normalize_value(50.0, 0.0, 100.0, 10000) == 5000);

    // Out-of-range inputs (fp rounding at the boundary) clamp instead of
    // wrapping/overflowing.
    CHECK(display_format_normalize_value(-1.0, 0.0, 100.0, 10000) == 0);
    CHECK(display_format_normalize_value(101.0, 0.0, 100.0, 10000) == 10000);

    // Micro-cap price range that a fixed "* 100 cents" scale would have
    // collapsed to a single integer - relative normalization still resolves
    // the movement within the tight band.
    CHECK(display_format_normalize_value(0.00000735, 0.00000730, 0.00000740, 10000) == 5000);
}

int main(void)
{
    test_price();
    test_abbreviate();
    test_normalize_value();
    return test_summary("display_format");
}
