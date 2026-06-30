#include "dodo/generators.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>
#include <utility>

namespace dodo {
namespace {

void append_int(std::string& out, std::int64_t value) {
    char buf[24];
    int n = std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
    out.append(buf, static_cast<std::size_t>(n));
}

[[noreturn]] void fail(const I18n& i18n, std::string_view key,
                       std::initializer_list<std::string_view> args) {
    throw DodoError(format_message(i18n.tr(key), args));
}

const std::string* param(const Field& f, std::string_view key) {
    auto it = f.params.find(std::string(key));
    return it == f.params.end() ? nullptr : &it->second;
}

std::int64_t param_int(const Field& f, std::string_view key, std::int64_t fallback,
                       const I18n& i18n) {
    const std::string* raw = param(f, key);
    if (!raw)
        return fallback;
    char* end = nullptr;
    long long v = std::strtoll(raw->c_str(), &end, 10);
    if (end == raw->c_str() || *end != '\0')
        fail(i18n, "error.param_int", {key, f.name});
    return v;
}

double param_double(const Field& f, std::string_view key, double fallback,
                    const I18n& i18n) {
    const std::string* raw = param(f, key);
    if (!raw)
        return fallback;
    char* end = nullptr;
    double v = std::strtod(raw->c_str(), &end);
    if (end == raw->c_str() || *end != '\0')
        fail(i18n, "error.param_number", {key, f.name});
    return v;
}

std::string_view param_str(const Field& f, std::string_view key,
                           std::string_view fallback) {
    const std::string* raw = param(f, key);
    return raw ? std::string_view(*raw) : fallback;
}

Span<std::string_view> pick(const Span<std::string_view>& preferred,
                            const Span<std::string_view>& base) {
    return preferred.empty() ? base : preferred;
}

// Drop the accent from a UTF-8 0xC3-prefixed Latin-1 letter, or return 0 to skip
// it. Enough to keep generated emails/usernames/slugs valid ASCII across the
// bundled locales.
char fold_latin1(unsigned char b) {
    switch (b) {
    case 0xA0:
    case 0xA1:
    case 0xA2:
    case 0xA3:
    case 0xA4:
    case 0xA5:
        return 'a';
    case 0xA6:
        return 'a'; // ae -> a (good enough for slugs)
    case 0xA7:
        return 'c';
    case 0xA8:
    case 0xA9:
    case 0xAA:
    case 0xAB:
        return 'e';
    case 0xAC:
    case 0xAD:
    case 0xAE:
    case 0xAF:
        return 'i';
    case 0xB0:
        return 'd';
    case 0xB1:
        return 'n';
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
    case 0xB8:
        return 'o';
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
        return 'u';
    case 0xBD:
    case 0xBF:
        return 'y';
    case 0x9F:
        return 's'; // sharp s -> s
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
        return 'A';
    case 0x87:
        return 'C';
    case 0x88:
    case 0x89:
    case 0x8A:
    case 0x8B:
        return 'E';
    case 0x8C:
    case 0x8D:
    case 0x8E:
    case 0x8F:
        return 'I';
    case 0x91:
        return 'N';
    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x96:
    case 0x98:
        return 'O';
    case 0x99:
    case 0x9A:
    case 0x9B:
    case 0x9C:
        return 'U';
    case 0x9D:
        return 'Y';
    default:
        return 0;
    }
}

// Lowercase, accent-free, [a-z0-9]-only rendering of `text`, with runs of other
// characters collapsed into a single `sep`. No leading/trailing separator.
void slugify(std::string& out, std::string_view text, char sep) {
    bool started = false;
    bool pending = false;
    auto push_alnum = [&](char c) {
        if (pending && started)
            out.push_back(sep);
        pending = false;
        started = true;
        out.push_back(c);
    };
    for (std::size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == 0xC3 && i + 1 < text.size()) {
            char folded = fold_latin1(static_cast<unsigned char>(text[i + 1]));
            i += 2;
            if (folded) {
                char low =
                    static_cast<char>(std::tolower(static_cast<unsigned char>(folded)));
                if (std::isalnum(static_cast<unsigned char>(low)))
                    push_alnum(low);
                else
                    pending = true;
            } else {
                pending = true;
            }
        } else if (c < 0x80) {
            if (std::isalnum(c))
                push_alnum(static_cast<char>(std::tolower(c)));
            else
                pending = true;
            i += 1;
        } else {
            i += 1;
            pending = true;
        }
    }
}

// --- date math: Howard Hinnant's day <-> civil algorithms ---

std::int64_t days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

void civil_from_days(std::int64_t z, int& y, unsigned& m, unsigned& d) {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int yy = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    y = yy + (m <= 2);
}

bool parse_date(std::string_view text, std::int64_t& out_days) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(std::string(text).c_str(), "%d-%d-%d", &y, &m, &d) != 3)
        return false;
    if (m < 1 || m > 12 || d < 1 || d > 31)
        return false;
    out_days = days_from_civil(y, static_cast<unsigned>(m), static_cast<unsigned>(d));
    return true;
}

void format_datetime(std::string& out, std::string_view fmt, int y, unsigned mo,
                     unsigned da, int h, int mi, int s) {
    char field[8];
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%' || i + 1 >= fmt.size()) {
            out.push_back(fmt[i]);
            continue;
        }
        switch (fmt[++i]) {
        case 'Y':
            std::snprintf(field, sizeof(field), "%04d", y);
            out += field;
            break;
        case 'm':
            std::snprintf(field, sizeof(field), "%02u", mo);
            out += field;
            break;
        case 'd':
            std::snprintf(field, sizeof(field), "%02u", da);
            out += field;
            break;
        case 'H':
            std::snprintf(field, sizeof(field), "%02d", h);
            out += field;
            break;
        case 'M':
            std::snprintf(field, sizeof(field), "%02d", mi);
            out += field;
            break;
        case 'S':
            std::snprintf(field, sizeof(field), "%02d", s);
            out += field;
            break;
        case '%':
            out.push_back('%');
            break;
        default:
            out.push_back('%');
            out.push_back(fmt[i]);
            break;
        }
    }
}

constexpr char kHex[] = "0123456789abcdef";

// --- generators ---

class SequenceGenerator : public Generator {
  public:
    SequenceGenerator(std::int64_t start, std::int64_t step) : value_(start), step_(step) {}
    Value generate(Rng&, const std::vector<Value>&) override {
        buf_.clear();
        append_int(buf_, value_);
        value_ += step_;
        return Value::number(buf_);
    }

  private:
    std::int64_t value_, step_;
    std::string buf_;
};

class UuidGenerator : public Generator {
  public:
    Value generate(Rng& rng, const std::vector<Value>&) override {
        std::uint64_t hi = rng.next_u64();
        std::uint64_t lo = rng.next_u64();
        hi = (hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
        lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
        buf_.resize(36);
        int p = 0;
        auto emit = [&](std::uint64_t v, int nibbles) {
            for (int i = nibbles - 1; i >= 0; --i)
                buf_[p++] = kHex[(v >> (i * 4)) & 0xF];
        };
        emit(hi >> 32, 8);
        buf_[p++] = '-';
        emit((hi >> 16) & 0xFFFF, 4);
        buf_[p++] = '-';
        emit(hi & 0xFFFF, 4);
        buf_[p++] = '-';
        emit(lo >> 48, 4);
        buf_[p++] = '-';
        emit(lo & 0xFFFFFFFFFFFFULL, 12);
        return Value::string(buf_);
    }

  private:
    std::string buf_;
};

class IntGenerator : public Generator {
  public:
    IntGenerator(std::int64_t lo, std::int64_t hi) : lo_(lo), hi_(hi) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.clear();
        append_int(buf_, rng.next_int(lo_, hi_));
        return Value::number(buf_);
    }

  private:
    std::int64_t lo_, hi_;
    std::string buf_;
};

class FloatGenerator : public Generator {
  public:
    FloatGenerator(double lo, double hi, int decimals)
        : lo_(lo), span_(hi - lo), decimals_(decimals) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        char b[64];
        int n =
            std::snprintf(b, sizeof(b), "%.*f", decimals_, lo_ + rng.next_double() * span_);
        buf_.assign(b, static_cast<std::size_t>(n));
        return Value::number(buf_);
    }

  private:
    double lo_, span_;
    int decimals_;
    std::string buf_;
};

class BoolGenerator : public Generator {
  public:
    explicit BoolGenerator(double p) : p_(p) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        return Value::make_bool(rng.next_bool(p_));
    }

  private:
    double p_;
};

class ChoiceGenerator : public Generator {
  public:
    explicit ChoiceGenerator(Span<std::string_view> choices) : choices_(choices) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        return Value::string(choices_[rng.next_index(choices_.size())]);
    }

  private:
    Span<std::string_view> choices_;
};

class FullNameGenerator : public Generator {
  public:
    FullNameGenerator(Span<std::string_view> f, Span<std::string_view> l)
        : first_(f), last_(l) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.assign(first_[rng.next_index(first_.size())]);
        buf_.push_back(' ');
        buf_.append(last_[rng.next_index(last_.size())]);
        return Value::string(buf_);
    }

  private:
    Span<std::string_view> first_, last_;
    std::string buf_;
};

// Shared base for email/username: a local part either derived from another
// column (referential integrity) or invented from locale names.
class DerivedHandleGenerator : public Generator {
  public:
    DerivedHandleGenerator(Span<std::string_view> first, Span<std::string_view> last,
                           std::ptrdiff_t ref, int max_suffix)
        : first_(first), last_(last), ref_(ref), max_suffix_(max_suffix) {}

  protected:
    void build_local(std::string& out, Rng& rng, const std::vector<Value>& row) {
        if (ref_ >= 0) {
            slugify(out, row[static_cast<std::size_t>(ref_)].text, '.');
        } else {
            slugify(out, first_[rng.next_index(first_.size())], '.');
            out.push_back('.');
            slugify(out, last_[rng.next_index(last_.size())], '.');
        }
        if (out.empty())
            out.push_back('x');
        if (max_suffix_ > 0)
            append_int(out, rng.next_int(1, max_suffix_));
    }

    Span<std::string_view> first_, last_;
    std::ptrdiff_t ref_;
    int max_suffix_;
    std::string buf_;
};

class UsernameGenerator : public DerivedHandleGenerator {
  public:
    using DerivedHandleGenerator::DerivedHandleGenerator;
    Value generate(Rng& rng, const std::vector<Value>& row) override {
        buf_.clear();
        build_local(buf_, rng, row);
        return Value::string(buf_);
    }
};

class EmailGenerator : public DerivedHandleGenerator {
  public:
    EmailGenerator(Span<std::string_view> first, Span<std::string_view> last,
                   std::ptrdiff_t ref, Span<std::string_view> domains,
                   std::string fixed_domain)
        : DerivedHandleGenerator(first, last, ref, 99), domains_(domains),
          fixed_domain_(std::move(fixed_domain)) {}
    Value generate(Rng& rng, const std::vector<Value>& row) override {
        buf_.clear();
        build_local(buf_, rng, row);
        buf_.push_back('@');
        if (!fixed_domain_.empty())
            buf_.append(fixed_domain_);
        else
            buf_.append(domains_[rng.next_index(domains_.size())]);
        return Value::string(buf_);
    }

  private:
    Span<std::string_view> domains_;
    std::string fixed_domain_;
};

// '#' -> digit, '@' -> uppercase letter, anything else literal. Backs phone,
// postal_code and the pattern type.
class PatternGenerator : public Generator {
  public:
    explicit PatternGenerator(std::string pattern) : pattern_(std::move(pattern)) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.clear();
        for (char c : pattern_) {
            if (c == '#')
                buf_.push_back(static_cast<char>('0' + rng.next_int(0, 9)));
            else if (c == '@')
                buf_.push_back(static_cast<char>('A' + rng.next_int(0, 25)));
            else
                buf_.push_back(c);
        }
        return Value::string(buf_);
    }

  private:
    std::string pattern_;
    std::string buf_;
};

class StreetAddressGenerator : public Generator {
  public:
    StreetAddressGenerator(Span<std::string_view> streets, bool number_first)
        : streets_(streets), number_first_(number_first) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        std::string_view street = streets_[rng.next_index(streets_.size())];
        std::int64_t number = rng.next_int(1, 9999);
        buf_.clear();
        if (number_first_) {
            append_int(buf_, number);
            buf_.push_back(' ');
            buf_.append(street);
        } else {
            buf_.append(street);
            buf_.push_back(' ');
            append_int(buf_, number);
        }
        return Value::string(buf_);
    }

  private:
    Span<std::string_view> streets_;
    bool number_first_;
    std::string buf_;
};

class ConstantGenerator : public Generator {
  public:
    explicit ConstantGenerator(std::string v) : value_(std::move(v)) {}
    Value generate(Rng&, const std::vector<Value>&) override {
        return Value::string(value_);
    }

  private:
    std::string value_;
};

class StringGenerator : public Generator {
  public:
    explicit StringGenerator(std::size_t len) : len_(len) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        static const char alphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        buf_.resize(len_);
        for (std::size_t i = 0; i < len_; ++i)
            buf_[i] = alphabet[rng.next_index(62)];
        return Value::string(buf_);
    }

  private:
    std::size_t len_;
    std::string buf_;
};

class WordsGenerator : public Generator {
  public:
    WordsGenerator(Span<std::string_view> words, int count)
        : words_(words), count_(count) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.clear();
        for (int i = 0; i < count_; ++i) {
            if (i)
                buf_.push_back(' ');
            buf_.append(words_[rng.next_index(words_.size())]);
        }
        return Value::string(buf_);
    }

  private:
    Span<std::string_view> words_;
    int count_;
    std::string buf_;
};

class SentenceGenerator : public Generator {
  public:
    SentenceGenerator(Span<std::string_view> words, int lo, int hi)
        : words_(words), lo_(lo), hi_(hi) {}
    void render(std::string& out, Rng& rng) {
        int count = static_cast<int>(rng.next_int(lo_, hi_));
        for (int i = 0; i < count; ++i) {
            std::string_view w = words_[rng.next_index(words_.size())];
            if (i == 0) {
                out.push_back(
                    static_cast<char>(std::toupper(static_cast<unsigned char>(w.front()))));
                out.append(w.substr(1));
            } else {
                out.push_back(' ');
                out.append(w);
            }
        }
        out.push_back('.');
    }
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.clear();
        render(buf_, rng);
        return Value::string(buf_);
    }

  private:
    Span<std::string_view> words_;
    int lo_, hi_;
    std::string buf_;
};

class ParagraphGenerator : public Generator {
  public:
    ParagraphGenerator(Span<std::string_view> words, int sentences)
        : sentence_(words, 6, 14), sentences_(sentences) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.clear();
        for (int i = 0; i < sentences_; ++i) {
            if (i)
                buf_.push_back(' ');
            sentence_.render(buf_, rng);
        }
        return Value::string(buf_);
    }

  private:
    SentenceGenerator sentence_;
    int sentences_;
    std::string buf_;
};

class DateGenerator : public Generator {
  public:
    DateGenerator(std::int64_t lo, std::int64_t hi, std::string fmt)
        : lo_(lo), hi_(hi), fmt_(std::move(fmt)) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        int y;
        unsigned m, d;
        civil_from_days(rng.next_int(lo_, hi_), y, m, d);
        buf_.clear();
        format_datetime(buf_, fmt_, y, m, d, 0, 0, 0);
        return Value::string(buf_);
    }

  private:
    std::int64_t lo_, hi_;
    std::string fmt_, buf_;
};

class DateTimeGenerator : public Generator {
  public:
    DateTimeGenerator(std::int64_t lo, std::int64_t hi, std::string fmt)
        : lo_(lo), hi_(hi), fmt_(std::move(fmt)) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        std::int64_t s = rng.next_int(lo_, hi_);
        std::int64_t day = s / 86400;
        int tod = static_cast<int>(s - day * 86400);
        int y;
        unsigned m, d;
        civil_from_days(day, y, m, d);
        buf_.clear();
        format_datetime(buf_, fmt_, y, m, d, tod / 3600, (tod % 3600) / 60, tod % 60);
        return Value::string(buf_);
    }

  private:
    std::int64_t lo_, hi_;
    std::string fmt_, buf_;
};

class TimeGenerator : public Generator {
  public:
    explicit TimeGenerator(std::string fmt) : fmt_(std::move(fmt)) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        int tod = static_cast<int>(rng.next_int(0, 86399));
        buf_.clear();
        format_datetime(buf_, fmt_, 0, 0, 0, tod / 3600, (tod % 3600) / 60, tod % 60);
        return Value::string(buf_);
    }

  private:
    std::string fmt_, buf_;
};

class EnumGenerator : public Generator {
  public:
    EnumGenerator(std::vector<std::string> values, std::vector<double> cumulative)
        : values_(std::move(values)), cumulative_(std::move(cumulative)) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        if (cumulative_.empty())
            return Value::string(values_[rng.next_index(values_.size())]);
        double pick = rng.next_double() * cumulative_.back();
        for (std::size_t i = 0; i < cumulative_.size(); ++i)
            if (pick < cumulative_[i])
                return Value::string(values_[i]);
        return Value::string(values_.back());
    }

  private:
    std::vector<std::string> values_;
    std::vector<double> cumulative_;
};

class Ipv4Generator : public Generator {
  public:
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.clear();
        for (int i = 0; i < 4; ++i) {
            if (i)
                buf_.push_back('.');
            append_int(buf_, rng.next_int(0, 255));
        }
        return Value::string(buf_);
    }

  private:
    std::string buf_;
};

class Ipv6Generator : public Generator {
  public:
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.clear();
        for (int g = 0; g < 8; ++g) {
            if (g)
                buf_.push_back(':');
            int v = static_cast<int>(rng.next_int(0, 0xFFFF));
            for (int s = 12; s >= 0; s -= 4)
                buf_.push_back(kHex[(v >> s) & 0xF]);
        }
        return Value::string(buf_);
    }

  private:
    std::string buf_;
};

class MacGenerator : public Generator {
  public:
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.clear();
        for (int i = 0; i < 6; ++i) {
            if (i)
                buf_.push_back(':');
            int v = static_cast<int>(rng.next_int(0, 255));
            buf_.push_back(kHex[(v >> 4) & 0xF]);
            buf_.push_back(kHex[v & 0xF]);
        }
        return Value::string(buf_);
    }

  private:
    std::string buf_;
};

class HexColorGenerator : public Generator {
  public:
    Value generate(Rng& rng, const std::vector<Value>&) override {
        int v = static_cast<int>(rng.next_int(0, 0xFFFFFF));
        buf_.assign(1, '#');
        for (int s = 20; s >= 0; s -= 4)
            buf_.push_back(kHex[(v >> s) & 0xF]);
        return Value::string(buf_);
    }

  private:
    std::string buf_;
};

class SlugGenerator : public Generator {
  public:
    SlugGenerator(Span<std::string_view> words, int count) : words_(words), count_(count) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        buf_.clear();
        for (int i = 0; i < count_; ++i) {
            if (i)
                buf_.push_back('-');
            slugify(buf_, words_[rng.next_index(words_.size())], '-');
        }
        return Value::string(buf_);
    }

  private:
    Span<std::string_view> words_;
    int count_;
    std::string buf_;
};

class DomainGenerator : public Generator {
  public:
    DomainGenerator(Span<std::string_view> companies, Span<std::string_view> domains)
        : companies_(companies), domains_(domains) {}
    Value generate(Rng& rng, const std::vector<Value>&) override {
        static const char* tlds[] = {"com", "net", "io", "co", "org", "dev"};
        buf_.clear();
        slugify(buf_, companies_[rng.next_index(companies_.size())], '-');
        if (buf_.empty())
            buf_.append(domains_[rng.next_index(domains_.size())]);
        else {
            buf_.push_back('.');
            buf_.append(tlds[rng.next_index(6)]);
        }
        return Value::string(buf_);
    }

  private:
    Span<std::string_view> companies_, domains_;
    std::string buf_;
};

class UrlGenerator : public Generator {
  public:
    UrlGenerator(Span<std::string_view> companies, Span<std::string_view> domains,
                 Span<std::string_view> words)
        : domain_(companies, domains), words_(words) {}
    Value generate(Rng& rng, const std::vector<Value>& row) override {
        buf_.assign("https://www.");
        buf_.append(domain_.generate(rng, row).text);
        buf_.push_back('/');
        slugify(buf_, words_[rng.next_index(words_.size())], '-');
        return Value::string(buf_);
    }

  private:
    DomainGenerator domain_;
    Span<std::string_view> words_;
    std::string buf_;
};

class CurrencyCodeGenerator : public Generator {
  public:
    Value generate(Rng& rng, const std::vector<Value>&) override {
        static const char* codes[] = {"USD", "EUR", "GBP", "JPY", "CHF", "CAD",
                                      "AUD", "CNY", "SEK", "MXN", "BRL", "INR"};
        return Value::string(codes[rng.next_index(12)]);
    }
};

// A 16-digit number with a valid Luhn check digit, grouped in fours.
class CreditCardGenerator : public Generator {
  public:
    Value generate(Rng& rng, const std::vector<Value>&) override {
        std::array<int, 16> digits{};
        digits[0] = 4;
        for (int i = 1; i < 15; ++i)
            digits[i] = static_cast<int>(rng.next_int(0, 9));
        int sum = 0;
        bool dbl = true;
        for (int i = 14; i >= 0; --i) {
            int d = digits[i];
            if (dbl) {
                d *= 2;
                if (d > 9)
                    d -= 9;
            }
            sum += d;
            dbl = !dbl;
        }
        digits[15] = (10 - sum % 10) % 10;
        buf_.clear();
        for (int i = 0; i < 16; ++i) {
            if (i && i % 4 == 0)
                buf_.push_back(' ');
            buf_.push_back(static_cast<char>('0' + digits[i]));
        }
        return Value::string(buf_);
    }

  private:
    std::string buf_;
};

// Stitches literal text and {column} references resolved at build time. The
// referenced columns must appear earlier in the field list.
class TemplateGenerator : public Generator {
  public:
    struct Part {
        bool is_ref;
        std::string literal;
        std::size_t ref;
    };
    explicit TemplateGenerator(std::vector<Part> parts) : parts_(std::move(parts)) {}
    Value generate(Rng&, const std::vector<Value>& row) override {
        buf_.clear();
        for (const Part& part : parts_) {
            if (part.is_ref) {
                const Value& v = row[part.ref];
                if (v.type != ValueType::Null)
                    buf_.append(v.text);
            } else {
                buf_.append(part.literal);
            }
        }
        return Value::string(buf_);
    }

  private:
    std::vector<Part> parts_;
    std::string buf_;
};

// Best-effort uniqueness: retry a few times, then disambiguate with a counter.
// Holds a growing set, so it trades the constant-memory guarantee for uniqueness
// (documented). sequence and uuid don't need this.
class UniqueGenerator : public Generator {
  public:
    explicit UniqueGenerator(GeneratorPtr inner) : inner_(std::move(inner)) {}
    Value generate(Rng& rng, const std::vector<Value>& row) override {
        for (int attempt = 0; attempt < 8; ++attempt) {
            Value v = inner_->generate(rng, row);
            if (v.type == ValueType::Null)
                return v;
            if (seen_.insert(std::string(v.text)).second) {
                last_ = std::string(v.text);
                return Value::string(last_);
            }
        }
        // Domain looks exhausted; disambiguate with a counter until novel.
        std::string base(inner_->generate(rng, row).text);
        do {
            last_.assign(base);
            last_.push_back('-');
            append_int(last_, ++counter_);
        } while (!seen_.insert(last_).second);
        return Value::string(last_);
    }

  private:
    GeneratorPtr inner_;
    std::unordered_set<std::string> seen_;
    std::string last_;
    std::int64_t counter_ = 0;
};

class NullableGenerator : public Generator {
  public:
    NullableGenerator(GeneratorPtr inner, double rate)
        : inner_(std::move(inner)), rate_(rate) {}
    Value generate(Rng& rng, const std::vector<Value>& row) override {
        if (rng.next_double() < rate_)
            return Value::null();
        return inner_->generate(rng, row);
    }

  private:
    GeneratorPtr inner_;
    double rate_;
};

std::ptrdiff_t resolve_reference(const Field& f, std::string_view ref_name,
                                 const GeneratorContext& ctx) {
    auto it = ctx.field_index.find(std::string(ref_name));
    if (it == ctx.field_index.end())
        fail(ctx.i18n, "error.unknown_reference", {ref_name, f.name});
    if (it->second >= ctx.self_index)
        fail(ctx.i18n, "error.forward_reference", {ref_name, f.name});
    return static_cast<std::ptrdiff_t>(it->second);
}

GeneratorPtr build(const Field& f, const GeneratorContext& ctx) {
    const I18n& i18n = ctx.i18n;
    const Locale& loc = ctx.locale;
    const Locale& base = ctx.base;
    std::string_view type = canonical_type(f.type);

    auto first = pick(loc.first_names, base.first_names);
    auto last = pick(loc.last_names, base.last_names);
    auto words = pick(loc.words, base.words);
    auto domains = pick(loc.email_domains, base.email_domains);
    auto companies = pick(loc.companies, base.companies);

    if (type == "sequence")
        return std::make_unique<SequenceGenerator>(param_int(f, "start", 1, i18n),
                                                   param_int(f, "step", 1, i18n));
    if (type == "uuid")
        return std::make_unique<UuidGenerator>();
    if (type == "int") {
        std::int64_t lo = param_int(f, "min", 0, i18n), hi = param_int(f, "max", 100, i18n);
        if (lo > hi)
            fail(i18n, "error.range_order", {f.name});
        return std::make_unique<IntGenerator>(lo, hi);
    }
    if (type == "float" || type == "price") {
        bool price = type == "price";
        double lo = param_double(f, "min", 0.0, i18n);
        double hi = param_double(f, "max", price ? 1000.0 : 1.0, i18n);
        if (lo > hi)
            fail(i18n, "error.range_order", {f.name});
        return std::make_unique<FloatGenerator>(
            lo, hi, static_cast<int>(param_int(f, "decimals", 2, i18n)));
    }
    if (type == "latitude")
        return std::make_unique<FloatGenerator>(-90.0, 90.0, 6);
    if (type == "longitude")
        return std::make_unique<FloatGenerator>(-180.0, 180.0, 6);
    if (type == "bool")
        return std::make_unique<BoolGenerator>(param_double(f, "p", 0.5, i18n));
    if (type == "first_name")
        return std::make_unique<ChoiceGenerator>(first);
    if (type == "last_name")
        return std::make_unique<ChoiceGenerator>(last);
    if (type == "full_name")
        return std::make_unique<FullNameGenerator>(first, last);
    if (type == "city")
        return std::make_unique<ChoiceGenerator>(pick(loc.cities, base.cities));
    if (type == "state")
        return std::make_unique<ChoiceGenerator>(pick(loc.states, base.states));
    if (type == "country")
        return std::make_unique<ChoiceGenerator>(pick(loc.countries, base.countries));
    if (type == "country_code")
        return std::make_unique<ChoiceGenerator>(
            pick(loc.country_codes, base.country_codes));
    if (type == "company")
        return std::make_unique<ChoiceGenerator>(companies);
    if (type == "job_title")
        return std::make_unique<ChoiceGenerator>(pick(loc.job_titles, base.job_titles));
    if (type == "currency_code")
        return std::make_unique<CurrencyCodeGenerator>();
    if (type == "credit_card")
        return std::make_unique<CreditCardGenerator>();
    if (type == "ipv4")
        return std::make_unique<Ipv4Generator>();
    if (type == "ipv6")
        return std::make_unique<Ipv6Generator>();
    if (type == "mac_address")
        return std::make_unique<MacGenerator>();
    if (type == "hex_color")
        return std::make_unique<HexColorGenerator>();
    if (type == "slug")
        return std::make_unique<SlugGenerator>(
            words, static_cast<int>(param_int(f, "n", 3, i18n)));
    if (type == "domain")
        return std::make_unique<DomainGenerator>(companies, domains);
    if (type == "url")
        return std::make_unique<UrlGenerator>(companies, domains, words);

    if (type == "username" || type == "email") {
        std::ptrdiff_t ref = -1;
        if (const std::string* from = param(f, "from"))
            ref = resolve_reference(f, *from, ctx);
        if (type == "username")
            return std::make_unique<UsernameGenerator>(first, last, ref, 999);
        return std::make_unique<EmailGenerator>(first, last, ref, domains,
                                                std::string(param_str(f, "domain", "")));
    }
    if (type == "phone") {
        std::string_view fmt =
            loc.phone_format.empty() ? base.phone_format : loc.phone_format;
        return std::make_unique<PatternGenerator>(std::string(fmt));
    }
    if (type == "postal_code") {
        std::string_view fmt =
            loc.postal_format.empty() ? base.postal_format : loc.postal_format;
        return std::make_unique<PatternGenerator>(std::string(fmt));
    }
    if (type == "street_address") {
        bool nf = loc.streets.empty() ? base.street_number_first : loc.street_number_first;
        return std::make_unique<StreetAddressGenerator>(pick(loc.streets, base.streets),
                                                        nf);
    }
    if (type == "date" || type == "datetime") {
        std::int64_t lo = days_from_civil(2000, 1, 1), hi = days_from_civil(2025, 12, 31);
        if (const std::string* s = param(f, "start"))
            if (!parse_date(*s, lo))
                fail(i18n, "error.invalid_date", {*s, f.name});
        if (const std::string* e = param(f, "end"))
            if (!parse_date(*e, hi))
                fail(i18n, "error.invalid_date", {*e, f.name});
        if (lo > hi)
            fail(i18n, "error.date_order", {f.name});
        if (type == "date")
            return std::make_unique<DateGenerator>(
                lo, hi, std::string(param_str(f, "fmt", "%Y-%m-%d")));
        return std::make_unique<DateTimeGenerator>(
            lo * 86400, hi * 86400 + 86399,
            std::string(param_str(f, "fmt", "%Y-%m-%d %H:%M:%S")));
    }
    if (type == "time")
        return std::make_unique<TimeGenerator>(
            std::string(param_str(f, "fmt", "%H:%M:%S")));
    if (type == "word")
        return std::make_unique<WordsGenerator>(words, 1);
    if (type == "words")
        return std::make_unique<WordsGenerator>(
            words, static_cast<int>(param_int(f, "n", 3, i18n)));
    if (type == "sentence")
        return std::make_unique<SentenceGenerator>(words, 6, 14);
    if (type == "paragraph")
        return std::make_unique<ParagraphGenerator>(words, 4);
    if (type == "string")
        return std::make_unique<StringGenerator>(
            static_cast<std::size_t>(param_int(f, "len", 12, i18n)));
    if (type == "enum") {
        if (f.enum_values.empty())
            fail(i18n, "error.enum_empty", {f.name});
        std::vector<double> cumulative;
        if (!f.enum_weights.empty()) {
            if (f.enum_weights.size() != f.enum_values.size())
                fail(i18n, "error.enum_weights_length", {f.name});
            double running = 0.0;
            for (double w : f.enum_weights) {
                if (w < 0.0)
                    fail(i18n, "error.enum_weights_invalid", {f.name});
                running += w;
                cumulative.push_back(running);
            }
            if (running <= 0.0)
                fail(i18n, "error.enum_weights_invalid", {f.name});
        }
        return std::make_unique<EnumGenerator>(f.enum_values, std::move(cumulative));
    }
    if (type == "pattern") {
        const std::string* p = param(f, "pattern");
        if (!p || p->empty())
            fail(i18n, "error.pattern_missing", {f.name});
        return std::make_unique<PatternGenerator>(*p);
    }
    if (type == "constant") {
        const std::string* v = param(f, "value");
        if (!v)
            fail(i18n, "error.constant_missing", {f.name});
        return std::make_unique<ConstantGenerator>(*v);
    }
    if (type == "template") {
        const std::string* tpl = param(f, "template");
        if (!tpl || tpl->empty())
            fail(i18n, "error.template_missing", {f.name});
        std::vector<TemplateGenerator::Part> parts;
        std::string literal;
        for (std::size_t i = 0; i < tpl->size(); ++i) {
            char c = (*tpl)[i];
            if (c == '{') {
                std::size_t close = tpl->find('}', i);
                if (close == std::string::npos)
                    fail(i18n, "error.template_syntax", {f.name});
                if (!literal.empty()) {
                    parts.push_back({false, literal, 0});
                    literal.clear();
                }
                std::string ref_name = tpl->substr(i + 1, close - i - 1);
                std::ptrdiff_t ref = resolve_reference(f, ref_name, ctx);
                parts.push_back({true, {}, static_cast<std::size_t>(ref)});
                i = close;
            } else {
                literal.push_back(c);
            }
        }
        if (!literal.empty())
            parts.push_back({false, literal, 0});
        return std::make_unique<TemplateGenerator>(std::move(parts));
    }

    fail(i18n, "error.unknown_type", {f.type, f.name});
}

} // namespace

std::string_view canonical_type(std::string_view type) {
    if (type == "id")
        return "sequence";
    if (type == "oneof")
        return "enum";
    if (type == "boolean")
        return "bool";
    if (type == "text")
        return "paragraph";
    if (type == "zip" || type == "postcode")
        return "postal_code";
    if (type == "color")
        return "hex_color";
    if (type == "ip")
        return "ipv4";
    if (type == "lat")
        return "latitude";
    if (type == "lng" || type == "long" || type == "lon")
        return "longitude";
    if (type == "company_name")
        return "company";
    if (type == "job" || type == "title")
        return "job_title";
    if (type == "mac")
        return "mac_address";
    return type;
}

GeneratorPtr make_generator(const Field& field, const GeneratorContext& context) {
    GeneratorPtr gen = build(field, context);

    std::string_view type = canonical_type(field.type);
    bool inherently_unique = type == "sequence" || type == "uuid";
    if (field.unique && !inherently_unique)
        gen = std::make_unique<UniqueGenerator>(std::move(gen));

    if (field.nullable) {
        double rate =
            param_double(field, "null_rate", context.default_null_rate, context.i18n);
        if (rate > 0.0)
            gen = std::make_unique<NullableGenerator>(std::move(gen), rate);
    }
    return gen;
}

const std::vector<FieldTypeInfo>& field_type_catalog() {
    static const std::vector<FieldTypeInfo> catalog = {
        {"sequence", "sequence(start=1, step=1)", "typedesc.sequence"},
        {"uuid", "uuid", "typedesc.uuid"},
        {"int", "int(min..max)", "typedesc.int"},
        {"float", "float(min..max, decimals=2)", "typedesc.float"},
        {"price", "price(min..max, decimals=2)", "typedesc.price"},
        {"bool", "bool(p=0.5)", "typedesc.bool"},
        {"first_name", "first_name", "typedesc.first_name"},
        {"last_name", "last_name", "typedesc.last_name"},
        {"full_name", "full_name", "typedesc.full_name"},
        {"username", "username(from=field)", "typedesc.username"},
        {"email", "email(domain=..., from=field)", "typedesc.email"},
        {"phone", "phone", "typedesc.phone"},
        {"company", "company", "typedesc.company"},
        {"job_title", "job_title", "typedesc.job_title"},
        {"city", "city", "typedesc.city"},
        {"state", "state", "typedesc.state"},
        {"country", "country", "typedesc.country"},
        {"country_code", "country_code", "typedesc.country_code"},
        {"street_address", "street_address", "typedesc.street_address"},
        {"postal_code", "postal_code", "typedesc.postal_code"},
        {"latitude", "latitude", "typedesc.latitude"},
        {"longitude", "longitude", "typedesc.longitude"},
        {"ipv4", "ipv4", "typedesc.ipv4"},
        {"ipv6", "ipv6", "typedesc.ipv6"},
        {"mac_address", "mac_address", "typedesc.mac_address"},
        {"domain", "domain", "typedesc.domain"},
        {"url", "url", "typedesc.url"},
        {"slug", "slug(n=3)", "typedesc.slug"},
        {"hex_color", "hex_color", "typedesc.hex_color"},
        {"currency_code", "currency_code", "typedesc.currency_code"},
        {"credit_card", "credit_card", "typedesc.credit_card"},
        {"date", "date(start..end, fmt=%Y-%m-%d)", "typedesc.date"},
        {"datetime", "datetime(start..end, fmt=...)", "typedesc.datetime"},
        {"time", "time", "typedesc.time"},
        {"word", "word", "typedesc.word"},
        {"words", "words(n)", "typedesc.words"},
        {"sentence", "sentence", "typedesc.sentence"},
        {"paragraph", "paragraph", "typedesc.paragraph"},
        {"string", "string(len)", "typedesc.string"},
        {"enum", "enum(a, b, c)", "typedesc.enum"},
        {"pattern", "pattern(\"###-@@\")", "typedesc.pattern"},
        {"template", "template(\"{first} {last}\")", "typedesc.template"},
        {"constant", "constant(value)", "typedesc.constant"},
    };
    return catalog;
}

bool is_known_field_type(std::string_view type) {
    std::string_view canonical = canonical_type(type);
    for (const FieldTypeInfo& info : field_type_catalog())
        if (info.name == canonical)
            return true;
    return false;
}

} // namespace dodo
