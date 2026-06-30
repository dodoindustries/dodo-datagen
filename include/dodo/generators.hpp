#ifndef DODO_GENERATORS_HPP
#define DODO_GENERATORS_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "dodo/field.hpp"
#include "dodo/i18n.hpp"
#include "dodo/locale.hpp"
#include "dodo/rng.hpp"

namespace dodo {

// A produced cell. Numbers carry a ready-made literal so every writer emits them
// unquoted and identically; booleans and nulls stay typed so each format can
// spell them its own way. `text` points into the owning generator's reusable
// buffer (or into static data) and is valid until that generator runs again.
enum class ValueType { Null, Number, Boolean, String };

struct Value {
    ValueType type = ValueType::Null;
    bool boolean = false;
    std::string_view text;

    static Value null() noexcept { return Value{}; }
    static Value number(std::string_view literal) noexcept {
        return Value{ValueType::Number, false, literal};
    }
    static Value make_bool(bool v) noexcept { return Value{ValueType::Boolean, v, {}}; }
    static Value string(std::string_view v) noexcept {
        return Value{ValueType::String, false, v};
    }
};

// Generators run left to right and may read columns already produced this row
// (the `row` argument), which is how derived fields and templates work. Apart
// from that they are independent and must never throw.
class Generator {
  public:
    virtual ~Generator() = default;
    virtual Value generate(Rng& rng, const std::vector<Value>& row) = 0;
};

using GeneratorPtr = std::unique_ptr<Generator>;

// What the factory needs to build (and validate) one generator.
struct GeneratorContext {
    const Locale& locale;
    const Locale& base;
    double default_null_rate;
    const I18n& i18n;
    const std::unordered_map<std::string, std::size_t>& field_index;
    std::size_t self_index;
};

GeneratorPtr make_generator(const Field& field, const GeneratorContext& context);

struct FieldTypeInfo {
    std::string_view name;
    std::string_view signature;
    std::string_view description_key;
};

const std::vector<FieldTypeInfo>& field_type_catalog();
bool is_known_field_type(std::string_view type);
std::string_view canonical_type(std::string_view type);

} // namespace dodo

#endif // DODO_GENERATORS_HPP
