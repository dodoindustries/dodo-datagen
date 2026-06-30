#ifndef DODO_I18N_HPP
#define DODO_I18N_HPP

#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dodo {

enum class Language { English, Spanish };

// Thrown for every user-facing failure (CLI, schema, validation). Carries an
// already-localized message so the top-level handler in main() can print it
// verbatim. The hot generation loop never throws.
class DodoError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

Language parse_language(std::string_view code);
Language detect_language_from_env();

// Message catalog. Every user-visible string is looked up by a descriptive key
// so that adding a language means adding one catalog (see CONTRIBUTING.md).
class I18n {
  public:
    explicit I18n(Language language);

    Language language() const noexcept { return language_; }

    // Returns the message for the key, falling back to English and finally to
    // the key itself so a missing translation is visible but never fatal.
    std::string_view tr(std::string_view key) const;

  private:
    Language language_;
    const std::unordered_map<std::string_view, std::string_view>* catalog_;
};

// Replaces each "{}" placeholder in order with the provided arguments. Used to
// splice variable parts (field names, values) into localized templates.
std::string format_message(std::string_view tmpl,
                           std::initializer_list<std::string_view> args);

} // namespace dodo

#endif // DODO_I18N_HPP
