#ifndef DODO_LOCALE_HPP
#define DODO_LOCALE_HPP

#include <cstddef>
#include <string_view>
#include <vector>

namespace dodo {

// A tiny non-owning view over a fixed array. Lets the bundled datasets live as
// `static constexpr std::string_view[]` with no copies and no C++20 std::span.
template <typename T> class Span {
  public:
    constexpr Span() = default;

    template <std::size_t N>
    constexpr Span(const T (&array)[N]) noexcept : data_(array), size_(N) {}

    constexpr const T* begin() const noexcept { return data_; }
    constexpr const T* end() const noexcept { return data_ + size_; }
    constexpr const T& operator[](std::size_t i) const noexcept { return data_[i]; }
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

  private:
    const T* data_ = nullptr;
    std::size_t size_ = 0;
};

// One locale's worth of source data. Categories left empty fall back to the
// en_US bundle (see base_locale), so a new locale can ship partial data.
struct Locale {
    std::string_view code;
    Span<std::string_view> first_names;
    Span<std::string_view> last_names;
    Span<std::string_view> cities;
    Span<std::string_view> states;
    Span<std::string_view> countries;
    Span<std::string_view> country_codes;
    Span<std::string_view> streets;
    Span<std::string_view> companies;
    Span<std::string_view> job_titles;
    Span<std::string_view> email_domains;
    Span<std::string_view> words;
    std::string_view phone_format;
    std::string_view postal_format;
    bool street_number_first;
};

bool locale_exists(std::string_view code);
const Locale& get_locale(std::string_view code);
const Locale& base_locale();
std::vector<std::string_view> available_locales();

} // namespace dodo

#endif // DODO_LOCALE_HPP
