#include <doctest/doctest.h>

#include <cstdio>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "dodo/cli.hpp"
#include "dodo/field.hpp"
#include "dodo/generators.hpp"
#include "dodo/writer.hpp"

using namespace dodo;

namespace {

std::vector<Field> columns(const std::vector<std::string>& names) {
    std::vector<Field> fields;
    for (const std::string& n : names) {
        Field f;
        f.name = n;
        fields.push_back(f);
    }
    return fields;
}

std::vector<Field>
typed_columns(const std::vector<std::pair<std::string, std::string>>& defs) {
    std::vector<Field> fields;
    for (const auto& [name, type] : defs) {
        Field f;
        f.name = name;
        f.type = type;
        fields.push_back(f);
    }
    return fields;
}

std::string render(const CliOptions& options, const std::vector<Field>& fields,
                   const std::vector<std::vector<Value>>& rows) {
    std::FILE* file = std::tmpfile();
    REQUIRE(file != nullptr);
    {
        OutputBuffer buffer(file);
        auto writer = make_writer(options, buffer);
        writer->begin(fields);
        for (const auto& row : rows)
            writer->write_row(row);
        writer->end();
    }
    std::fflush(file);
    std::fseek(file, 0, SEEK_SET);
    std::string out;
    char chunk[4096];
    std::size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), file)) > 0)
        out.append(chunk, n);
    std::fclose(file);
    return out;
}

} // namespace

TEST_CASE("CSV quotes per RFC 4180") {
    CliOptions o;
    o.format = OutputFormat::Csv;
    std::string a = "he,llo", b = "say \"hi\"", c = "two\nlines";
    auto out = render(o, columns({"a", "b", "c"}),
                      {{Value::string(a), Value::string(b), Value::string(c)}});
    CHECK(out == "a,b,c\n\"he,llo\",\"say \"\"hi\"\"\",\"two\nlines\"\n");
}

TEST_CASE("CSV renders bool, number and null") {
    CliOptions o;
    o.format = OutputFormat::Csv;
    auto out = render(o, columns({"flag", "n", "maybe"}),
                      {{Value::make_bool(true), Value::number("42"), Value::null()}});
    CHECK(out == "flag,n,maybe\ntrue,42,\n");
}

TEST_CASE("TSV separates with tabs") {
    CliOptions o;
    o.format = OutputFormat::Tsv;
    o.delimiter = '\t';
    std::string a = "x", b = "y";
    auto out = render(o, columns({"a", "b"}), {{Value::string(a), Value::string(b)}});
    CHECK(out == "a\tb\nx\ty\n");
}

TEST_CASE("JSON array is valid and typed") {
    CliOptions o;
    o.format = OutputFormat::Json;
    std::string name = "Ann \"A\"";
    auto out = render(o, columns({"id", "name", "active", "score", "note"}),
                      {{Value::number("1"), Value::string(name), Value::make_bool(true),
                        Value::number("3.5"), Value::null()}});
    auto j = nlohmann::json::parse(out);
    REQUIRE(j.is_array());
    CHECK(j[0]["id"] == 1);
    CHECK(j[0]["name"] == "Ann \"A\"");
    CHECK(j[0]["active"] == true);
    CHECK(j[0]["score"] == 3.5);
    CHECK(j[0]["note"].is_null());
}

TEST_CASE("pretty JSON still parses") {
    CliOptions o;
    o.format = OutputFormat::Json;
    o.pretty = true;
    auto out = render(o, columns({"id"}), {{Value::number("1")}, {Value::number("2")}});
    CHECK(out.find('\n') != std::string::npos);
    auto j = nlohmann::json::parse(out);
    CHECK(j.size() == 2);
}

TEST_CASE("JSONL is one object per line") {
    CliOptions o;
    o.format = OutputFormat::JsonLines;
    auto out = render(o, columns({"id"}), {{Value::number("1")}, {Value::number("2")}});
    CHECK(out == "{\"id\":1}\n{\"id\":2}\n");
}

TEST_CASE("SQL (postgres) quotes identifiers and batches") {
    CliOptions o;
    o.format = OutputFormat::Sql;
    o.table = "users";
    o.batch = 2;
    std::string oreilly = "O'Reilly", ann = "Ann";
    auto out = render(
        o, columns({"id", "name", "active", "note"}),
        {{Value::number("1"), Value::string(oreilly), Value::make_bool(true),
          Value::null()},
         {Value::number("2"), Value::string(ann), Value::make_bool(false), Value::null()}});
    CHECK(out == "INSERT INTO \"users\" (\"id\", \"name\", \"active\", \"note\") VALUES\n"
                 "(1, 'O''Reilly', TRUE, NULL),\n"
                 "(2, 'Ann', FALSE, NULL);\n");
}

TEST_CASE("SQL (mysql) uses backticks and escapes backslashes") {
    CliOptions o;
    o.format = OutputFormat::Sql;
    o.table = "t";
    o.batch = 10;
    o.dialect = SqlDialect::MySql;
    std::string path = "a\\b";
    auto out = render(o, columns({"p"}), {{Value::string(path)}});
    CHECK(out == "INSERT INTO `t` (`p`) VALUES\n('a\\\\b');\n");
}

TEST_CASE("SQL CREATE TABLE maps column types") {
    CliOptions o;
    o.format = OutputFormat::Sql;
    o.table = "t";
    o.create_table = true;
    o.batch = 10;
    auto fields = typed_columns(
        {{"id", "sequence"}, {"amount", "float"}, {"ok", "bool"}, {"name", "full_name"}});
    auto out = render(o, fields,
                      {{Value::number("1"), Value::number("2.5"), Value::make_bool(true),
                        Value::string("x")}});
    CHECK(out.find("CREATE TABLE \"t\" (\n") != std::string::npos);
    CHECK(out.find("\"id\" BIGINT") != std::string::npos);
    CHECK(out.find("\"amount\" DOUBLE PRECISION") != std::string::npos);
    CHECK(out.find("\"ok\" BOOLEAN") != std::string::npos);
    CHECK(out.find("\"name\" TEXT") != std::string::npos);
}
