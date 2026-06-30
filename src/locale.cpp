#include "dodo/locale.hpp"

#include "dodo/i18n.hpp"

#include "data/locales.hpp"

namespace dodo {
namespace {

struct Entry {
    std::string_view code;
    const Locale& (*get)();
};

const Entry kLocales[] = {
    {"en_US", data::en_us}, {"en_GB", data::en_gb}, {"es_ES", data::es_es},
    {"fr_FR", data::fr_fr}, {"de_DE", data::de_de},
};

} // namespace

// en_US is the base bundle other locales fall back to for empty categories.
const Locale& base_locale() {
    return data::en_us();
}

bool locale_exists(std::string_view code) {
    for (const Entry& e : kLocales)
        if (e.code == code)
            return true;
    return false;
}

const Locale& get_locale(std::string_view code) {
    for (const Entry& e : kLocales)
        if (e.code == code)
            return e.get();
    throw DodoError(std::string("unknown data locale: ") + std::string(code));
}

std::vector<std::string_view> available_locales() {
    std::vector<std::string_view> codes;
    for (const Entry& e : kLocales)
        codes.push_back(e.code);
    return codes;
}

} // namespace dodo
