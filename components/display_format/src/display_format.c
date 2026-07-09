#include "display_format.h"

#include <math.h>
#include <stdio.h>

typedef struct
{
    double threshold;
    const char *suffix;
} abbrev_entry_t;

static const abbrev_entry_t kAbbrevTable[] = {
    {1e12, "T"},
    {1e9, "B"},
    {1e6, "M"},
    {1e3, "K"},
};

void display_format_abbreviate(double value, char *out, size_t out_len)
{
    double abs_value = fabs(value);

    for (size_t i = 0; i < sizeof(kAbbrevTable) / sizeof(kAbbrevTable[0]); i++)
    {
        if (abs_value >= kAbbrevTable[i].threshold)
        {
            snprintf(out, out_len, "%.2f%s", value / kAbbrevTable[i].threshold, kAbbrevTable[i].suffix);
            return;
        }
    }
    snprintf(out, out_len, "%.2f", value);
}

void display_format_price(double value, char *out, size_t out_len)
{
    double abs_value = fabs(value);

    if (abs_value >= 1e9)
    {
        display_format_abbreviate(value, out, out_len);
        return;
    }

    if (abs_value >= 1.0 || abs_value == 0.0)
    {
        snprintf(out, out_len, "%.2f", value);
        return;
    }

    // Sub-$1 values: grow decimals as the value shrinks so micro-cap prices
    // (e.g. SHIB, PEPE at $0.00000734) keep ~4 significant digits instead of
    // rounding to a fixed precision that flattens to "0.0000". Capped at 8
    // decimals. The epsilon nudge avoids an fp-rounding off-by-one at exact
    // powers of ten (e.g. -log10(0.01) landing a hair under 2.0).
    int leading_zeros = (int)floor(-log10(abs_value) + 1e-9);
    int decimals = leading_zeros + 4;
    if (decimals < 4)
    {
        decimals = 4;
    }
    if (decimals > 8)
    {
        decimals = 8;
    }
    snprintf(out, out_len, "%.*f", decimals, value);
}

int32_t display_format_normalize_value(double value, double lo, double hi, int32_t scale_max)
{
    double range = hi - lo;
    if (range <= 0.0)
    {
        return scale_max / 2;
    }

    double normalized = (value - lo) / range * (double)scale_max;
    if (normalized < 0.0)
    {
        normalized = 0.0;
    }
    else if (normalized > (double)scale_max)
    {
        normalized = (double)scale_max;
    }
    return (int32_t)lround(normalized);
}
