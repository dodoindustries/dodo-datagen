#ifndef DODO_SCHEMA_HPP
#define DODO_SCHEMA_HPP

#include <string>
#include <vector>

#include "dodo/field.hpp"
#include "dodo/i18n.hpp"

namespace dodo {

// A parsed, not-yet-validated table definition. `locale_code` is only set when
// the schema file specifies one; a CLI --locale always overrides it.
struct Schema {
    std::vector<Field> fields;
    std::string locale_code;
    bool has_locale = false;
};

Schema load_schema_from_file(const std::string& path, const I18n& i18n);
Schema build_schema_from_inline(const std::vector<std::string>& field_specs,
                                const I18n& i18n);

// Structural validation: non-empty unique names, known types, coherent enum
// weights. Numeric/date range coherence is checked when generators are built.
void validate_schema(const Schema& schema, const I18n& i18n);

} // namespace dodo

#endif // DODO_SCHEMA_HPP
