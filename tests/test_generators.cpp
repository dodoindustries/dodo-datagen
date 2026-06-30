#include <doctest/doctest.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dodo/generators.hpp"
#include "dodo/i18n.hpp"
#include "dodo/locale.hpp"
#include "dodo/rng.hpp"

using namespace dodo;

namespace {

Field make_field(std::string name, std::string type) {
    Field f;
    f.name = std::move(name);
    f.type = std::move(type);
    return f;
}

// Builds generators for a list of fields exactly the way the engine does, so
// referential fields (email from=name, templates) can be exercised.
struct Built {
    std::vector<GeneratorPtr> gens;
    std::vector<Value> row;

    void step(Rng& rng) {
        for (std::size_t i = 0; i < gens.size(); ++i)
            row[i] = gens[i]->generate(rng, row);
    }
};

Built build(const std::vector<Field>& fields, std::string_view locale = "en_US",
            double null_rate = 0.0) {
    static I18n i18n(Language::English);
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < fields.size(); ++i)
        index.emplace(fields[i].name, i);
    Built built;
    built.row.resize(fields.size());
    for (std::size_t i = 0; i < fields.size(); ++i) {
        GeneratorContext ctx{get_locale(locale), base_locale(), null_rate, i18n, index, i};
        built.gens.push_back(make_generator(fields[i], ctx));
    }
    return built;
}

GeneratorPtr one(const Field& f, std::string_view locale = "en_US",
                 double null_rate = 0.0) {
    return std::move(build({f}, locale, null_rate).gens[0]);
}

} // namespace

TEST_CASE("sequence walks from start by step") {
    Field f = make_field("id", "sequence");
    f.params["start"] = "10";
    f.params["step"] = "5";
    auto g = one(f);
    Rng rng(1);
    std::vector<Value> row(1);
    CHECK(g->generate(rng, row).text == "10");
    CHECK(g->generate(rng, row).text == "15");
    CHECK(g->generate(rng, row).text == "20");
}

TEST_CASE("int stays in range") {
    Field f = make_field("age", "int");
    f.params["min"] = "18";
    f.params["max"] = "20";
    auto g = one(f);
    Rng rng(3);
    std::vector<Value> row(1);
    for (int i = 0; i < 1000; ++i) {
        Value v = g->generate(rng, row);
        CHECK(v.type == ValueType::Number);
        int n = std::stoi(std::string(v.text));
        CHECK(n >= 18);
        CHECK(n <= 20);
    }
}

TEST_CASE("uuid looks like a v4 uuid") {
    auto g = one(make_field("u", "uuid"));
    Rng rng(42);
    std::vector<Value> row(1);
    std::string v(g->generate(rng, row).text);
    REQUIRE(v.size() == 36);
    CHECK(v[14] == '4');
    CHECK((v[19] == '8' || v[19] == '9' || v[19] == 'a' || v[19] == 'b'));
}

TEST_CASE("email is ASCII and carries the domain") {
    Field f = make_field("email", "email");
    f.params["domain"] = "test.dev";
    auto g = one(f, "es_ES");
    Rng rng(11);
    std::vector<Value> row(1);
    for (int i = 0; i < 500; ++i) {
        std::string v(g->generate(rng, row).text);
        CHECK(v.find("@test.dev") != std::string::npos);
        for (char c : v)
            CHECK(static_cast<unsigned char>(c) < 0x80);
    }
}

TEST_CASE("email derived from a name field matches it") {
    Field name = make_field("name", "full_name");
    Field email = make_field("email", "email");
    email.params["from"] = "name";
    email.params["domain"] = "acme.test";
    Built b = build({name, email});
    Rng rng(5);
    b.step(rng);
    std::string full(b.row[0].text);
    std::string mail(b.row[1].text);
    // Local part should be the lower-cased, dotted form of the name.
    std::string expected_prefix;
    for (char c : full) {
        if (c == ' ')
            expected_prefix.push_back('.');
        else
            expected_prefix.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    CHECK(mail.rfind(expected_prefix, 0) == 0);
}

TEST_CASE("forward and unknown references are rejected") {
    Field email = make_field("email", "email");
    email.params["from"] = "missing";
    CHECK_THROWS_AS(build({email}), DodoError);

    Field a = make_field("a", "email");
    a.params["from"] = "b"; // b comes later
    Field b = make_field("b", "full_name");
    CHECK_THROWS_AS(build({a, b}), DodoError);
}

TEST_CASE("template interpolates earlier columns") {
    Field first = make_field("first", "constant");
    first.params["value"] = "Ada";
    Field last = make_field("last", "constant");
    last.params["value"] = "Lovelace";
    Field full = make_field("full", "template");
    full.params["template"] = "{first} {last} <{first}>";
    Built b = build({first, last, full});
    Rng rng(1);
    b.step(rng);
    CHECK(b.row[2].text == "Ada Lovelace <Ada>");
}

TEST_CASE("credit_card passes the Luhn check") {
    auto g = one(make_field("c", "credit_card"));
    Rng rng(9);
    std::vector<Value> row(1);
    for (int i = 0; i < 500; ++i) {
        std::string v(g->generate(rng, row).text);
        int sum = 0, pos = 0;
        for (auto it = v.rbegin(); it != v.rend(); ++it) {
            if (!std::isdigit(static_cast<unsigned char>(*it)))
                continue;
            int d = *it - '0';
            if (pos % 2 == 1) {
                d *= 2;
                if (d > 9)
                    d -= 9;
            }
            sum += d;
            ++pos;
        }
        CHECK(pos == 16);
        CHECK(sum % 10 == 0);
    }
}

TEST_CASE("ipv4 octets are in range") {
    auto g = one(make_field("ip", "ipv4"));
    Rng rng(2);
    std::vector<Value> row(1);
    for (int i = 0; i < 200; ++i) {
        std::string v(g->generate(rng, row).text);
        int a, b, c, d;
        REQUIRE(std::sscanf(v.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) == 4);
        for (int octet : {a, b, c, d}) {
            CHECK(octet >= 0);
            CHECK(octet <= 255);
        }
    }
}

TEST_CASE("hex_color is a 7-character hash color") {
    auto g = one(make_field("c", "hex_color"));
    Rng rng(4);
    std::vector<Value> row(1);
    std::string v(g->generate(rng, row).text);
    REQUIRE(v.size() == 7);
    CHECK(v[0] == '#');
    for (std::size_t i = 1; i < 7; ++i)
        CHECK(std::isxdigit(static_cast<unsigned char>(v[i])));
}

TEST_CASE("latitude and longitude stay within bounds") {
    auto lat = one(make_field("lat", "latitude"));
    auto lng = one(make_field("lng", "longitude"));
    Rng rng(6);
    std::vector<Value> row(1);
    for (int i = 0; i < 500; ++i) {
        double la = std::stod(std::string(lat->generate(rng, row).text));
        double lo = std::stod(std::string(lng->generate(rng, row).text));
        CHECK(la >= -90.0);
        CHECK(la <= 90.0);
        CHECK(lo >= -180.0);
        CHECK(lo <= 180.0);
    }
}

TEST_CASE("enum with weights only returns listed values") {
    Field f = make_field("status", "enum");
    f.enum_values = {"active", "trial", "churned"};
    f.enum_weights = {0.6, 0.1, 0.3};
    auto g = one(f);
    Rng rng(13);
    std::vector<Value> row(1);
    for (int i = 0; i < 1000; ++i) {
        std::string v(g->generate(rng, row).text);
        CHECK((v == "active" || v == "trial" || v == "churned"));
    }
}

TEST_CASE("date is formatted and in range") {
    Field f = make_field("d", "date");
    f.params["start"] = "2020-01-01";
    f.params["end"] = "2020-12-31";
    auto g = one(f);
    Rng rng(21);
    std::vector<Value> row(1);
    for (int i = 0; i < 1000; ++i) {
        std::string v(g->generate(rng, row).text);
        REQUIRE(v.size() == 10);
        CHECK(v.substr(0, 4) == "2020");
    }
}

TEST_CASE("nullable field with a high rate yields nulls") {
    Field f = make_field("notes", "sentence");
    f.nullable = true;
    auto g = one(f, "en_US", 1.0);
    Rng rng(6);
    std::vector<Value> row(1);
    for (int i = 0; i < 50; ++i)
        CHECK(g->generate(rng, row).type == ValueType::Null);
}

TEST_CASE("per-field null_rate overrides the global default") {
    Field f = make_field("x", "int");
    f.nullable = true;
    f.params["null_rate"] = "1.0";
    auto g = one(f, "en_US", 0.0); // global default 0, field overrides to 1
    Rng rng(2);
    std::vector<Value> row(1);
    for (int i = 0; i < 50; ++i)
        CHECK(g->generate(rng, row).type == ValueType::Null);
}

TEST_CASE("unique never repeats a value, even when the domain is exhausted") {
    Field f = make_field("n", "int");
    f.params["min"] = "0";
    f.params["max"] = "50"; // only 51 values, but we ask for 300
    f.unique = true;
    auto g = one(f);
    Rng rng(1);
    std::vector<Value> row(1);
    std::unordered_set<std::string> seen;
    for (int i = 0; i < 300; ++i) {
        std::string v(g->generate(rng, row).text);
        CHECK(seen.insert(v).second);
    }
}

TEST_CASE("invalid int range is rejected when built") {
    Field f = make_field("age", "int");
    f.params["min"] = "90";
    f.params["max"] = "18";
    CHECK_THROWS_AS(one(f), DodoError);
}
