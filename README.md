<!-- Read this in: English | Español (README.es.md) -->

```
       __
      <(o )___    dodo-datagen
       ( ._> /
        `---'
```

# Dodo Datagen

A command-line generator of realistic fake data, written in C++. Streams rows with constant memory and is quick about it.

Read this in: **English** | [Español](README.es.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/dodoindustries/dodo-datagen/actions/workflows/ci.yml/badge.svg)](https://github.com/dodoindustries/dodo-datagen/actions/workflows/ci.yml)

---

## What it is

You need a few million rows of plausible-looking data to seed a database, exercise a load test, or fill out a demo. Dodo Datagen writes them. It produces names, addresses, emails, timestamps, and a couple dozen other field types in CSV, JSON, JSONL, or SQL.

The two properties worth calling out: it streams, and it's fast. Rows are generated and flushed one at a time, so 100 rows and 100 million rows use about the same RAM. On a recent laptop it does roughly 5-6 million simple rows per second, single-threaded. It's a single self-contained binary — the name, city, and company datasets are compiled in, and there are no runtime dependencies.

## Features

- Output as CSV, TSV, JSON, JSONL, or SQL `INSERT` statements
- Around three dozen field types: identifiers, numbers, people, places, internet, finance, dates/times, text, and structured choices
- Within-row referential integrity: emails and usernames derived from a name column, and a `template` type that stitches earlier columns together
- Inline `--field` definitions or a reusable JSON schema file
- Five data locales (en_US, en_GB, es_ES, fr_FR, de_DE), with a fallback to en_US for anything a locale doesn't cover
- Bilingual interface (English/Spanish), independent of the data locale
- Nullable and unique modifiers, weighted enums, configurable null rate
- Deterministic output with `--seed`

## Build from source

You need a C++17 compiler and CMake 3.16 or newer. Two dependencies (`nlohmann/json` and `doctest`) are pulled in by CMake via `FetchContent` the first time you configure, so the **first configure needs network access**. After that you can build offline.

### Linux / macOS (gcc or clang)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/dodo-datagen --version
```

### Windows (MSVC)

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\dodo-datagen.exe --version
```

On MSVC the binary lands in `build/Release/`. Elsewhere it's directly in `build/`.

Run the tests with:

```bash
ctest --test-dir build --output-on-failure
```

There's a `DODO_WERROR` CMake option if you want warnings treated as errors (it's on in CI).

The binary is installed both as `dodo-datagen` and under the shorter alias `dodo`. The examples below use `dodo-datagen`; `dodo` is interchangeable.

## Quick start

```bash
# Five rows of CSV to the terminal
dodo-datagen -n 5 --field id:sequence --field name:full_name --field email:email

# A million rows of JSONL to a file, reproducible
dodo-datagen -n 1000000 -f jsonl --seed 42 -o users.jsonl \
  --field "id:sequence" \
  --field "name:full_name" \
  --field "email:email(from=name)" \
  --field "age:int(18..90)" \
  --field "signup:date(2020-01-01..2024-12-31)"

# From a schema file, emitted as Postgres INSERTs with a CREATE TABLE
dodo-datagen -s examples/users.json -n 100 -f sql --create-table --table users > users.sql
```

Use `--field` flags or a `--schema` file, not both. For large `N`, progress is printed to stderr (silence it with `--quiet`); the data on stdout is never touched by it.

## CLI flags

| Flag | Description | Default |
|---|---|---|
| `-n`, `--rows N` | rows to generate | `10` |
| `-f`, `--format FMT` | `csv`, `tsv`, `json`, `jsonl`, `sql` | `csv` |
| `-o`, `--output PATH` | output file | stdout |
| `-s`, `--schema FILE` | JSON schema file | — |
| `--field SPEC` | inline field, e.g. `name:full_name`; repeatable | — |
| `--seed N` | seed the PRNG for reproducible output | random (printed to stderr) |
| `--locale CODE` | locale of the generated data | `en_US` |
| `--lang en\|es` | language of the program's own messages | from `$LANG`/`$LC_ALL` |
| `--delimiter C` | field delimiter (CSV) | `,` |
| `--no-header` | omit the header row | header on |
| `--null-rate 0..1` | probability a nullable field is null | `0` |
| `--table NAME` | table name for SQL output | `data` |
| `--batch N` | rows per `INSERT` (SQL) | `500` |
| `--dialect D` | `postgres`, `mysql`, `sqlite` (SQL) | `postgres` |
| `--create-table` | emit `CREATE TABLE` before the inserts | off |
| `--pretty` | indent JSON output | off |
| `--quiet` | suppress progress and the seed line on stderr | off |
| `--list-types` | list field types and exit | — |
| `--list-locales` | list data locales and exit | — |
| `-h`, `--help` | help | — |
| `-v`, `--version` | version | — |

## Field types

Inline arguments go in parentheses. The same keys are used in a JSON schema. Run `dodo-datagen --list-types` for the list as the binary sees it.

| Category | Type | Notes |
|---|---|---|
| identifiers | `sequence(start=1, step=1)` (alias `id`) | auto-incrementing integer |
| | `uuid` | random UUID |
| numbers | `int(min..max)` | integer in range |
| | `float(min..max, decimals=2)` | decimal in range |
| | `price(min..max, decimals=2)` | like `float`, formatted as a price |
| | `bool(p=0.5)` | `true` with probability `p` |
| people | `first_name`, `last_name`, `full_name` | locale-aware |
| | `username(from=field)` | derived from another column |
| | `email(domain=..., from=field)` | derived from another column |
| | `phone` | locale phone format |
| | `company`, `job_title` | |
| places | `city`, `state`, `country`, `country_code` | locale-aware |
| | `street_address`, `postal_code` | |
| | `latitude`, `longitude` | |
| internet | `ipv4`, `ipv6`, `mac_address` | |
| | `domain`, `url`, `slug(n=3)`, `hex_color` | |
| finance | `currency_code` | ISO code |
| | `credit_card` | 16 digits, valid Luhn checksum |
| time | `date(start..end, fmt=%Y-%m-%d)` | |
| | `datetime(start..end, fmt=%Y-%m-%d %H:%M:%S)` | |
| | `time` | |
| text | `word`, `words(n)`, `sentence`, `paragraph` (alias `text`) | filler text |
| | `string(len)` | random characters of length `len` |
| structured | `enum(a, b, c)` (alias `oneof`) | optional weights, e.g. `enum(active:0.6, trial:0.3)` |
| | `pattern("###-@@")` | `#` is a digit, `@` a letter, anything else literal |
| | `template("{col} ...")` | interpolates earlier columns |
| | `constant(value)` | always the same value |

### Modifiers

A trailing `?` makes a field nullable inline (`notes:sentence?`), or set `"nullable": true` in a schema. Nullable fields emit null with probability `--null-rate` (global) or a per-field `"null_rate"`.

`"unique": true` is guaranteed for `sequence` and `uuid`. For everything else it's best-effort: the generator keeps a set of seen values in memory (giving up the constant-memory property), and once a small domain is exhausted it disambiguates with a `-N` suffix.

## Referential integrity

References are resolved within a single row, against columns defined earlier. Two mechanisms:

`email` and `username` take `from=<column>` and derive their value from it, so the email actually matches the person's name. `template` interpolates earlier columns by name.

```bash
dodo-datagen -n 3 \
  --field "first:first_name" \
  --field "last:last_name" \
  --field "email:email(from=first, domain=acme.io)" \
  --field "label:template({first} {last})"
```

Referenced columns must come earlier in the list. Accented characters are folded to ASCII for emails, usernames, and slugs (`José` becomes `jose`). There is no referential integrity *between* separate tables — see the roadmap.

## Schema files

A schema is a JSON object with an optional `locale` and a `fields` array. Each field has a `name`, a `type`, and the type's parameters as sibling keys. Optional per-field keys: `nullable`, `null_rate`, `unique`, and for enums `values` / `weights`.

```json
{
  "locale": "es_ES",
  "fields": [
    { "name": "id",        "type": "sequence", "start": 1 },
    { "name": "name",      "type": "full_name" },
    { "name": "email",     "type": "email", "from": "name", "domain": "example.com" },
    { "name": "username",  "type": "username", "from": "name", "unique": true },
    { "name": "company",   "type": "company" },
    { "name": "status",    "type": "enum", "values": ["active", "trial", "churned"], "weights": [0.6, 0.1, 0.3] },
    { "name": "signup_at", "type": "datetime", "start": "2020-01-01", "end": "2024-12-31" },
    { "name": "notes",     "type": "sentence", "nullable": true, "null_rate": 0.4 }
  ]
}
```

A `--locale` on the command line overrides the `locale` in the schema. Validation rejects empty or duplicate names, unknown types, bad ranges (`min > max`, `start > end`, invalid dates), and enum weights that don't line up with the values — each with a clear message and a non-zero exit.

There are four schemas in [`examples/`](examples/) to copy from:

```bash
dodo-datagen -s examples/users.json        -n 100  -f jsonl
dodo-datagen -s examples/transactions.json -n 1000 -f csv   -o txns.csv
dodo-datagen -s examples/products.json      -n 500  -f json  --pretty
dodo-datagen -s examples/events.json        -n 200  -f sql   --table events
```

`users.json` shows derived email/username, company and job title, a weighted enum, and a nullable notes field. `transactions.json` covers prices, currency codes, credit cards, and IPv4. `products.json` uses slugs, URLs, and hex colors. `events.json` demonstrates `template` together with latitude/longitude.

## Output formats

Pick a format with `-f` / `--format`:

- **csv** — RFC 4180 quoting (fields with the delimiter, quotes, or newlines get quoted; embedded quotes are doubled). Change the separator with `--delimiter`, drop the header with `--no-header`.
- **tsv** — tab-separated, otherwise like CSV.
- **json** — a single streamed JSON array. Add `--pretty` to indent it.
- **jsonl** — one JSON object per line (also known as NDJSON). This is the one to use for big volumes.
- **sql** — `INSERT` statements. Options:
  - `--table NAME` sets the table name.
  - `--batch N` controls how many rows go into each `INSERT` (default 500).
  - `--dialect postgres|mysql|sqlite` changes identifier quoting and the column types used by `CREATE TABLE` (default `postgres`).
  - `--create-table` emits a matching `CREATE TABLE` before the inserts.

## Locales vs. interface language

These are two separate settings and it's worth keeping them straight.

`--locale` controls the **data**: which names, cities, phone formats, and so on come out. Available locales are `en_US`, `en_GB`, `es_ES`, `fr_FR`, and `de_DE`; list them with `--list-locales`. If a locale doesn't provide a particular category, that category falls back to `en_US`.

`--lang` controls the **program's own messages** — help text, errors, progress — in English or Spanish. It defaults from `$LANG` / `$LC_ALL`. You can generate French data while reading Spanish help; the two don't interact.

## Performance

Memory stays flat because rows are never accumulated — each is generated, written, and forgotten. To get a feel for the throughput on your own machine:

```bash
time dodo-datagen -n 5000000 -f jsonl --quiet \
  --field id:sequence --field name:full_name --field "age:int(18..90)" > /dev/null
```

A current laptop lands around 5-6 million rows per second on a schema like this, single-threaded. Heavier schemas — lots of text, many columns — scale down in proportion to the work per row.

## Reproducibility

Pass `--seed N` and you get byte-identical output across runs (there are golden tests pinning this). Without a seed, a random one is chosen and printed to stderr so you can reproduce a run after the fact.

## Roadmap

What it deliberately is not, for now:

- not a statistical synthetic-data engine — it doesn't learn distributions from real data
- not a database driver — it writes files or stdout, and you pipe or import from there
- no GUI
- no cross-table referential integrity yet; references work within a row, but there are no foreign keys between separate tables

On the list for later: more locales, cross-table relationships, parallel generation, and user-supplied datasets.

## Contributing

Patches welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE).
