#include "branding.h"
#include <cstring>

// Símbolos gerados pelo EMBED_FILES (components/httpd_app/CMakeLists.txt)
extern const unsigned char _binary_maxximed_png_gz_start[];
extern const unsigned char _binary_maxximed_png_gz_end[];

extern const unsigned char _binary_biosterons_png_gz_start[];
extern const unsigned char _binary_biosterons_png_gz_end[];

static const branding_entry_t BRANDING_TABLE[] = {
    {
        "MAXXIMED",
        _binary_maxximed_png_gz_start,
        _binary_maxximed_png_gz_end,
        "MAXXIMED LATIN AMERICA",
        "Maxximed",
        "Clicktest"
    },
    {
        "BIOSTERONS",
        _binary_biosterons_png_gz_start,
        _binary_biosterons_png_gz_end,
        "BIOSTERONS",
        "BioSterons",
        "BioSterons"
    }
};

const branding_entry_t* branding_get(const char* logo_key)
{
    if (!logo_key) {
        logo_key = "MAXXIMED";
    }

    for (auto &b : BRANDING_TABLE) {
        if (std::strcmp(b.key, logo_key) == 0) {
            return &b;
        }
    }

    // Fallback seguro
    return &BRANDING_TABLE[0];
}
