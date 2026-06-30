#ifndef DODO_CLI_HPP
#define DODO_CLI_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "dodo/i18n.hpp"

namespace dodo {

enum class OutputFormat { Csv, Tsv, Json, JsonLines, Sql };
enum class SqlDialect { Postgres, MySql, Sqlite };

// Everything resolved from argv. Parsed by hand (cxxopts would do, but rolling
// our own keeps the dependency list short and the parser is easy to follow).
struct CliOptions {
    enum class Action { Generate, Help, Version, ListTypes, ListLocales };
    Action action = Action::Generate;

    std::int64_t rows = 10;
    OutputFormat format = OutputFormat::Csv;
    std::string output_path;

    std::string schema_path;
    std::vector<std::string> inline_fields;

    bool has_seed = false;
    std::uint64_t seed = 0;

    std::string locale_code = "en_US";
    bool locale_set = false;

    Language language = Language::English;

    char delimiter = ',';
    bool delimiter_set = false;
    bool header = true;
    double null_rate = 0.0;

    std::string table = "data";
    std::int64_t batch = 500;
    SqlDialect dialect = SqlDialect::Postgres;
    bool create_table = false;

    bool pretty = false;
    bool quiet = false;
};

CliOptions parse_cli(int argc, char** argv);

std::string help_text(const I18n& i18n);
std::string version_text();
std::string list_types_text(const I18n& i18n);
std::string list_locales_text(const I18n& i18n);

} // namespace dodo

#endif // DODO_CLI_HPP
