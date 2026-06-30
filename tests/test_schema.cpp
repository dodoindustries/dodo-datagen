#include <doctest/doctest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "dodo/i18n.hpp"
#include "dodo/schema.hpp"

using namespace dodo;

namespace {

std::string temp_dir() {
    for (const char* name : {"TMPDIR", "TEMP", "TMP"}) {
        const char* value = std::getenv(name);
        if (value != nullptr && value[0] != '\0') {
            return value;
        }
    }
    return ".";
}

std::string write_temp_schema(const std::string& contents) {
    static int counter = 0;
    std::string path =
        temp_dir() + "/dodo_test_schema_" + std::to_string(counter++) + ".json";
    std::ofstream out(path, std::ios::binary);
    out << contents;
    out.close();
    return path;
}

} // namespace

TEST_CASE("inline int field is parsed into min/max params") {
    I18n i18n(Language::English);
    Schema schema = build_schema_from_inline({"age:int(18..90)"}, i18n);
    REQUIRE(schema.fields.size() == 1);
    CHECK(schema.fields[0].name == "age");
    CHECK(schema.fields[0].type == "int");
    CHECK(schema.fields[0].params.at("min") == "18");
    CHECK(schema.fields[0].params.at("max") == "90");
}

TEST_CASE("inline nullable suffix is recognised") {
    I18n i18n(Language::English);
    Schema schema = build_schema_from_inline({"notes:sentence?"}, i18n);
    CHECK(schema.fields[0].nullable);
    CHECK(schema.fields[0].type == "sentence");
}

TEST_CASE("inline enum with weights is parsed") {
    I18n i18n(Language::English);
    Schema schema =
        build_schema_from_inline({"status:enum(active:0.6, trial:0.1, churned:0.3)"}, i18n);
    REQUIRE(schema.fields[0].enum_values.size() == 3);
    REQUIRE(schema.fields[0].enum_weights.size() == 3);
    CHECK(schema.fields[0].enum_values[0] == "active");
    CHECK(schema.fields[0].enum_weights[0] == doctest::Approx(0.6));
}

TEST_CASE("inline date range maps to start/end") {
    I18n i18n(Language::English);
    Schema schema = build_schema_from_inline({"d:date(2020-01-01..2024-12-31)"}, i18n);
    CHECK(schema.fields[0].params.at("start") == "2020-01-01");
    CHECK(schema.fields[0].params.at("end") == "2024-12-31");
}

TEST_CASE("malformed inline spec is rejected") {
    I18n i18n(Language::English);
    CHECK_THROWS_AS(build_schema_from_inline({"no_colon"}, i18n), DodoError);
}

TEST_CASE("a valid JSON schema loads fields and locale") {
    I18n i18n(Language::English);
    std::string path = write_temp_schema(R"({
        "locale": "es_ES",
        "fields": [
            { "name": "id", "type": "sequence", "start": 1 },
            { "name": "age", "type": "int", "min": 18, "max": 90 },
            { "name": "status", "type": "enum", "values": ["a", "b"], "weights": [0.5, 0.5] }
        ]
    })");
    Schema schema = load_schema_from_file(path, i18n);
    std::remove(path.c_str());
    CHECK(schema.has_locale);
    CHECK(schema.locale_code == "es_ES");
    REQUIRE(schema.fields.size() == 3);
    CHECK(schema.fields[0].params.at("start") == "1");
    CHECK(schema.fields[1].params.at("max") == "90");
    CHECK(schema.fields[2].enum_values.size() == 2);
    validate_schema(schema, i18n);
}

TEST_CASE("duplicate field names fail validation") {
    I18n i18n(Language::English);
    Schema schema = build_schema_from_inline({"id:sequence", "id:int(1..2)"}, i18n);
    CHECK_THROWS_AS(validate_schema(schema, i18n), DodoError);
}

TEST_CASE("unknown field type fails validation") {
    I18n i18n(Language::English);
    Schema schema = build_schema_from_inline({"x:not_a_type"}, i18n);
    CHECK_THROWS_AS(validate_schema(schema, i18n), DodoError);
}

TEST_CASE("missing schema file reports an error") {
    I18n i18n(Language::English);
    CHECK_THROWS_AS(load_schema_from_file("does_not_exist_12345.json", i18n), DodoError);
}

TEST_CASE("type aliases id and oneof are accepted") {
    I18n i18n(Language::English);
    Schema schema = build_schema_from_inline({"id:id", "pick:oneof(a, b, c)"}, i18n);
    validate_schema(schema, i18n);
    CHECK(schema.fields[1].enum_values.size() == 3);
}

TEST_CASE("inline key=value args land in params") {
    I18n i18n(Language::English);
    Schema schema =
        build_schema_from_inline({"email:email(domain=acme.io, from=name)"}, i18n);
    CHECK(schema.fields[0].params.at("domain") == "acme.io");
    CHECK(schema.fields[0].params.at("from") == "name");
}

TEST_CASE("inline template keeps its braces") {
    I18n i18n(Language::English);
    Schema schema = build_schema_from_inline({"label:template(\"{a}-{b}\")"}, i18n);
    CHECK(schema.fields[0].params.at("template") == "{a}-{b}");
}

TEST_CASE("new field types validate") {
    I18n i18n(Language::English);
    Schema schema = build_schema_from_inline(
        {"ip:ipv4", "card:credit_card", "co:company", "where:latitude", "site:url"}, i18n);
    validate_schema(schema, i18n);
    CHECK(schema.fields.size() == 5);
}
