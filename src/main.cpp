#include <cstdio>
#include <exception>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "dodo/cli.hpp"
#include "dodo/generators.hpp"
#include "dodo/i18n.hpp"
#include "dodo/locale.hpp"
#include "dodo/schema.hpp"
#include "dodo/writer.hpp"

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

namespace dodo {
namespace {

// Put stdout in binary mode so a fixed seed gives identical bytes on every
// platform (no CRLF translation).
void use_binary_stdout() {
#if defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

std::uint64_t random_seed() {
    std::random_device dev;
    return (static_cast<std::uint64_t>(dev()) << 32) ^ static_cast<std::uint64_t>(dev());
}

std::string resolve_locale(const CliOptions& o, const Schema& schema) {
    if (o.locale_set)
        return o.locale_code;
    if (schema.has_locale)
        return schema.locale_code;
    return o.locale_code;
}

int run(int argc, char** argv) {
    CliOptions options = parse_cli(argc, argv);
    I18n i18n(options.language);

    switch (options.action) {
    case CliOptions::Action::Help:
        std::fputs(help_text(i18n).c_str(), stdout);
        return 0;
    case CliOptions::Action::Version:
        std::fputs(version_text().c_str(), stdout);
        return 0;
    case CliOptions::Action::ListTypes:
        std::fputs(list_types_text(i18n).c_str(), stdout);
        return 0;
    case CliOptions::Action::ListLocales:
        std::fputs(list_locales_text(i18n).c_str(), stdout);
        return 0;
    case CliOptions::Action::Generate:
        break;
    }

    if (!options.schema_path.empty() && !options.inline_fields.empty())
        throw DodoError(std::string(i18n.tr("error.schema_and_fields")));

    Schema schema;
    if (!options.schema_path.empty())
        schema = load_schema_from_file(options.schema_path, i18n);
    else if (!options.inline_fields.empty())
        schema = build_schema_from_inline(options.inline_fields, i18n);
    else
        throw DodoError(std::string(i18n.tr("error.no_fields")));
    validate_schema(schema, i18n);

    std::string locale_code = resolve_locale(options, schema);
    if (!locale_exists(locale_code))
        throw DodoError(format_message(i18n.tr("error.unknown_locale"), {locale_code}));

    std::unordered_map<std::string, std::size_t> field_index;
    for (std::size_t i = 0; i < schema.fields.size(); ++i)
        field_index.emplace(schema.fields[i].name, i);

    const Locale& locale = get_locale(locale_code);
    const Locale& base = base_locale();
    std::vector<GeneratorPtr> generators;
    generators.reserve(schema.fields.size());
    for (std::size_t i = 0; i < schema.fields.size(); ++i) {
        GeneratorContext context{locale, base, options.null_rate, i18n, field_index, i};
        generators.push_back(make_generator(schema.fields[i], context));
    }

    std::FILE* output = stdout;
    bool owns_output = false;
    if (options.output_path.empty()) {
        use_binary_stdout();
    } else {
        output = std::fopen(options.output_path.c_str(), "wb");
        if (!output)
            throw DodoError(
                format_message(i18n.tr("error.output_open"), {options.output_path}));
        owns_output = true;
    }

    const std::uint64_t seed = options.has_seed ? options.seed : random_seed();
    const bool show_progress = !options.quiet && options.rows >= 100000;
    if (!options.has_seed && !options.quiet)
        std::fprintf(
            stderr, "%s\n",
            format_message(i18n.tr("progress.seed"), {std::to_string(seed)}).c_str());

    {
        OutputBuffer buffer(output);
        std::unique_ptr<Writer> writer = make_writer(options, buffer);
        writer->begin(schema.fields);

        Rng rng(seed);
        std::vector<Value> row(generators.size());
        for (std::int64_t produced = 0; produced < options.rows; ++produced) {
            for (std::size_t f = 0; f < generators.size(); ++f)
                row[f] = generators[f]->generate(rng, row);
            writer->write_row(row);
            if (show_progress && (produced % 100000) == 0)
                std::fprintf(
                    stderr, "\r%s",
                    format_message(i18n.tr("progress.rows"),
                                   {std::to_string(produced), std::to_string(options.rows)})
                        .c_str());
        }
        writer->end();
    }

    if (show_progress)
        std::fprintf(
            stderr, "\r%s\n",
            format_message(i18n.tr("progress.done"), {std::to_string(options.rows)})
                .c_str());
    if (owns_output)
        std::fclose(output);
    return 0;
}

} // namespace
} // namespace dodo

int main(int argc, char** argv) {
    try {
        return dodo::run(argc, argv);
    } catch (const dodo::DodoError& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }
}
