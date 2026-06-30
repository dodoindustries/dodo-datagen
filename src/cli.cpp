#include "dodo/cli.hpp"

#include <cerrno>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "dodo/generators.hpp"
#include "dodo/locale.hpp"

namespace dodo {
namespace {

[[noreturn]] void fail(const I18n& i18n, std::string_view key,
                       std::initializer_list<std::string_view> args) {
    throw DodoError(format_message(i18n.tr(key), args));
}

std::int64_t parse_non_negative(const I18n& i18n, std::string_view text,
                                std::string_view key) {
    std::string copy(text);
    char* end = nullptr;
    errno = 0;
    long long v = std::strtoll(copy.c_str(), &end, 10);
    if (end == copy.c_str() || *end != '\0' || v < 0 || errno == ERANGE)
        fail(i18n, key, {text});
    return v;
}

double parse_unit_interval(const I18n& i18n, std::string_view text) {
    std::string copy(text);
    char* end = nullptr;
    double v = std::strtod(copy.c_str(), &end);
    if (end == copy.c_str() || *end != '\0' || v < 0.0 || v > 1.0)
        fail(i18n, "error.invalid_null_rate", {text});
    return v;
}

OutputFormat parse_format(const I18n& i18n, std::string_view text) {
    if (text == "csv")
        return OutputFormat::Csv;
    if (text == "tsv")
        return OutputFormat::Tsv;
    if (text == "json")
        return OutputFormat::Json;
    if (text == "jsonl" || text == "ndjson")
        return OutputFormat::JsonLines;
    if (text == "sql")
        return OutputFormat::Sql;
    fail(i18n, "error.invalid_format", {text});
}

SqlDialect parse_dialect(const I18n& i18n, std::string_view text) {
    if (text == "postgres" || text == "postgresql" || text == "pg")
        return SqlDialect::Postgres;
    if (text == "mysql" || text == "mariadb")
        return SqlDialect::MySql;
    if (text == "sqlite" || text == "sqlite3")
        return SqlDialect::Sqlite;
    fail(i18n, "error.invalid_dialect", {text});
}

Language detect_language(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--lang" && i + 1 < argc)
            return parse_language(argv[i + 1]);
        if (a.rfind("--lang=", 0) == 0)
            return parse_language(a.substr(7));
    }
    return detect_language_from_env();
}

} // namespace

CliOptions parse_cli(int argc, char** argv) {
    CliOptions o;
    o.language = detect_language(argc, argv);
    I18n i18n(o.language);

    // `has_inline` distinguishes "--flag=value" (use the inline value, even if
    // empty) from "--flag" (take the next argv). Without it, "--output=" would
    // silently swallow the following argument.
    bool has_inline = false;
    auto value_of = [&](int& i, std::string_view inlined,
                        std::string_view flag) -> std::string_view {
        if (has_inline)
            return inlined;
        if (i + 1 >= argc)
            fail(i18n, "error.missing_value", {flag});
        return argv[++i];
    };
    auto reject_value = [&](std::string_view flag) {
        if (has_inline)
            fail(i18n, "error.unexpected_value", {flag});
    };

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        std::string_view name = arg;
        std::string_view inlined;
        has_inline = false;
        if (arg.rfind("--", 0) == 0) {
            if (std::size_t eq = arg.find('='); eq != std::string_view::npos) {
                name = arg.substr(0, eq);
                inlined = arg.substr(eq + 1);
                has_inline = true;
            }
        }

        if (name == "-h" || name == "--help") {
            o.action = CliOptions::Action::Help;
            return o;
        }
        if (name == "-v" || name == "--version") {
            o.action = CliOptions::Action::Version;
            return o;
        }
        if (name == "--list-types") {
            o.action = CliOptions::Action::ListTypes;
            return o;
        }
        if (name == "--list-locales") {
            o.action = CliOptions::Action::ListLocales;
            return o;
        }

        if (name == "-n" || name == "--rows") {
            o.rows =
                parse_non_negative(i18n, value_of(i, inlined, name), "error.invalid_rows");
        } else if (name == "-f" || name == "--format") {
            o.format = parse_format(i18n, value_of(i, inlined, name));
        } else if (name == "-o" || name == "--output") {
            o.output_path = std::string(value_of(i, inlined, name));
        } else if (name == "-s" || name == "--schema") {
            o.schema_path = std::string(value_of(i, inlined, name));
        } else if (name == "--field") {
            o.inline_fields.emplace_back(value_of(i, inlined, name));
        } else if (name == "--seed") {
            std::string_view v = value_of(i, inlined, name);
            std::string copy(v);
            char* end = nullptr;
            errno = 0;
            unsigned long long s = std::strtoull(copy.c_str(), &end, 10);
            if (end == copy.c_str() || *end != '\0' || errno == ERANGE)
                fail(i18n, "error.invalid_seed", {v});
            o.seed = s;
            o.has_seed = true;
        } else if (name == "--locale") {
            o.locale_code = std::string(value_of(i, inlined, name));
            o.locale_set = true;
        } else if (name == "--lang") {
            value_of(i, inlined, name); // already applied in the pre-scan
        } else if (name == "--delimiter") {
            std::string_view v = value_of(i, inlined, name);
            if (v.size() != 1)
                fail(i18n, "error.invalid_delimiter", {v});
            o.delimiter = v.front();
            o.delimiter_set = true;
        } else if (name == "--no-header") {
            reject_value(name);
            o.header = false;
        } else if (name == "--null-rate") {
            o.null_rate = parse_unit_interval(i18n, value_of(i, inlined, name));
        } else if (name == "--table") {
            o.table = std::string(value_of(i, inlined, name));
        } else if (name == "--batch") {
            std::string_view v = value_of(i, inlined, name);
            o.batch = parse_non_negative(i18n, v, "error.invalid_batch");
            if (o.batch < 1)
                fail(i18n, "error.invalid_batch", {v});
        } else if (name == "--dialect") {
            o.dialect = parse_dialect(i18n, value_of(i, inlined, name));
        } else if (name == "--create-table") {
            reject_value(name);
            o.create_table = true;
        } else if (name == "--pretty") {
            reject_value(name);
            o.pretty = true;
        } else if (name == "--quiet") {
            reject_value(name);
            o.quiet = true;
        } else {
            fail(i18n, "error.unknown_option", {arg});
        }
    }

    // tsv is just csv with a tab, unless the user picked a delimiter explicitly.
    if (o.format == OutputFormat::Tsv && !o.delimiter_set)
        o.delimiter = '\t';
    return o;
}

std::string help_text(const I18n& i18n) {
    return std::string(i18n.tr("help.text"));
}

std::string version_text() {
    return "dodo-datagen 0.2.0\n";
}

std::string list_types_text(const I18n& i18n) {
    std::string out(i18n.tr("list.types.header"));
    out.push_back('\n');
    for (const FieldTypeInfo& info : field_type_catalog()) {
        out.append("  ");
        out.append(info.signature);
        std::size_t pad = info.signature.size() < 36 ? 36 - info.signature.size() : 2;
        out.append(pad, ' ');
        out.append(i18n.tr(info.description_key));
        out.push_back('\n');
    }
    return out;
}

std::string list_locales_text(const I18n& i18n) {
    std::string out(i18n.tr("list.locales.header"));
    out.push_back('\n');
    for (std::string_view code : available_locales()) {
        out.append("  ");
        out.append(code);
        out.push_back('\n');
    }
    return out;
}

} // namespace dodo
