#include "branding.h"
#include <cstring>

// S�mbolos gerados pelo EMBED_FILES (components/httpd_app/CMakeLists.txt)
extern const unsigned char _binary_maxximed_png_gz_start[];
extern const unsigned char _binary_maxximed_png_gz_end[];

extern const unsigned char _binary_biosterons_png_gz_start[];
extern const unsigned char _binary_biosterons_png_gz_end[];

extern const unsigned char _binary_medcontrol_png_gz_start[];
extern const unsigned char _binary_medcontrol_png_gz_end[];

extern const unsigned char _binary_tech_steri_png_gz_start[];
extern const unsigned char _binary_tech_steri_png_gz_end[];

extern const unsigned char _binary_baumer_png_gz_start[];
extern const unsigned char _binary_baumer_png_gz_end[];

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
    },
    // Textos recuperados da versao anterior a refatoracao pra branding.cpp
    // (D:\Github\ECK\Maxximed\lumina4\main\printer.cpp, get_partner_name()/
    // get_partner_name_formatted()/get_type_test()) - pedido do cliente 03/09
    // pra reativar essas 2 marcas no ticket impresso.
    {
        "MEDCONTROL",
        _binary_medcontrol_png_gz_start,
        _binary_medcontrol_png_gz_end,
        "MEDCONTROL",
        "Medcontrol",
        "BI - Test"
    },
    {
        "TECHSTERI",
        _binary_tech_steri_png_gz_start,
        _binary_tech_steri_png_gz_end,
        "TECHSTERI",
        "Techsteri",
        "Clicktest"
    },
    {
        "BAUMER",
        _binary_baumer_png_gz_start,
        _binary_baumer_png_gz_end,
        "BAUMER",
        "Baumer",
        "Clicktest"
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
