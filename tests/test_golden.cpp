#include <doctest/doctest.h>

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "dodo/cli.hpp"
#include "dodo/generators.hpp"
#include "dodo/i18n.hpp"
#include "dodo/locale.hpp"
#include "dodo/rng.hpp"
#include "dodo/schema.hpp"
#include "dodo/writer.hpp"

using namespace dodo;

namespace {

// Same pipeline as the binary, using only locale-independent types so the golden
// bytes don't depend on the bundled name/city lists.
std::string render_csv(std::uint64_t seed, std::int64_t rows) {
    I18n i18n(Language::English);
    Schema schema = build_schema_from_inline(
        {
            "id:sequence",
            "score:int(1..1000)",
            "u:uuid",
            "code:pattern(\"@@-####\")",
            "ip:ipv4",
            "color:hex_color",
            "when:date(2020-01-01..2020-12-31)",
            "ts:datetime(2020-01-01..2020-12-31)",
            "flag:bool(0.3)",
            "status:enum(active:0.6, trial:0.1, churned:0.3)",
            "amount:float(0..100, decimals=2)",
        },
        i18n);
    validate_schema(schema, i18n);

    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < schema.fields.size(); ++i)
        index.emplace(schema.fields[i].name, i);

    std::vector<GeneratorPtr> gens;
    for (std::size_t i = 0; i < schema.fields.size(); ++i) {
        GeneratorContext ctx{get_locale("en_US"), base_locale(), 0.0, i18n, index, i};
        gens.push_back(make_generator(schema.fields[i], ctx));
    }

    CliOptions o;
    o.format = OutputFormat::Csv;
    std::FILE* file = std::tmpfile();
    REQUIRE(file != nullptr);
    {
        OutputBuffer buffer(file);
        auto writer = make_writer(o, buffer);
        writer->begin(schema.fields);
        Rng rng(seed);
        std::vector<Value> row(gens.size());
        for (std::int64_t r = 0; r < rows; ++r) {
            for (std::size_t f = 0; f < gens.size(); ++f)
                row[f] = gens[f]->generate(rng, row);
            writer->write_row(row);
        }
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

const char* kGoldenSeed42 =
    "id,score,u,code,ip,color,when,ts,flag,status,amount\n"
    "1,743,6104d986-6d11-4a7e-ae17-533239e499a1,NC-4478,61.113.101.182,#b42742,"
    "2020-09-10,2020-11-06 13:56:10,false,churned,70.75\n"
    "2,899,17c5ad53-9dbb-48d9-add4-705aaba5de2b,PN-2844,250.218.253.58,#b09168,"
    "2020-07-03,2020-01-30 10:44:26,false,churned,4.48\n"
    "3,715,f8dae1de-eb08-490b-896c-10e1f50e7c45,TZ-2130,151.169.169.187,#32a0ec,"
    "2020-07-19,2020-04-05 04:44:52,true,active,56.93\n"
    "4,21,81dbd737-ae28-4209-9eb0-80fc911ead60,QZ-5061,232.39.142.187,#30d717,"
    "2020-05-03,2020-12-10 19:26:24,false,trial,32.36\n"
    "5,233,12a3bc1c-89b7-4769-96bd-abc6b717acaa,OA-9689,56.54.160.215,#92df83,"
    "2020-05-10,2020-11-19 22:16:12,false,active,97.80\n";

} // namespace

TEST_CASE("a fixed seed is byte-for-byte reproducible") {
    CHECK(render_csv(42, 25) == render_csv(42, 25));
}

TEST_CASE("different seeds differ") {
    CHECK(render_csv(1, 25) != render_csv(2, 25));
}

TEST_CASE("output matches the captured golden bytes") {
    CHECK(render_csv(42, 5) == std::string(kGoldenSeed42));
}
