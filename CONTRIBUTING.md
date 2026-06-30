# Contributing to dodo-datagen

```
      __
   <(o )___
    ( ._> /
     `---'
```

Glad you're here. Most contributions fall into one of three buckets: a new data
locale, a new field type, or a new interface language. This file walks through
each one, but read the conventions first — they apply to everything.

## Building and testing

The build is plain CMake. Dependencies (`nlohmann/json`, `doctest`) are pulled in
by `FetchContent` at configure time, so the first configure needs network access;
after that they're cached under `build/_deps`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDODO_WERROR=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`DODO_WERROR` is off by default so a clean checkout always builds, but turn it on
before you send a change. It promotes warnings to errors (`-Werror`, or `/WX` on
MSVC) and is exactly what CI runs. If your change builds clean locally with that
flag set, it'll build clean in CI.

There's a single test binary, `dodo_tests`, registered as one ctest case. To run
it directly while iterating:

```bash
./build/dodo_tests              # whole suite
./build/dodo_tests -ts=golden   # one test suite by name
```

`DODO_BUILD_TESTS` is on by default; pass `-DDODO_BUILD_TESTS=OFF` if you only
want the executables.

## Conventions

We're strict about a handful of things and relaxed about the rest.

| Rule | What it means |
|------|---------------|
| C++17 | No C++20. The `Span` type in `locale.hpp` exists because we don't have `std::span`. |
| Standard library in the core | First-party code leans on the STL. The only third-party headers are `nlohmann/json` (writers) and `doctest` (tests). |
| English only in code | Identifiers and comments are English, always. User-facing strings are bilingual (EN/ES) and live in catalogs — never hardcode Spanish in a comment. |
| Comment the why | Reserve comments for the non-obvious: format quirks (CSV/SQL escaping), bit tricks, the reason a loop is shaped the way it is. Don't narrate what the code already says. |
| No warnings | Must compile clean under `-Wall -Wextra -Wpedantic` (`/W4 /permissive-` on MSVC). No undefined behavior. |
| The hot loop never throws | `Generator::generate` is called once per cell per row. It must not throw. Exceptions are for CLI/parse/validation errors only, and there's a single handler for them in `main`. |

Formatting is handled by `.clang-format` — LLVM base, 4-space indent, 92-column
limit, pointers bound left. Run it on whatever you touch:

```bash
clang-format -i src/generators.cpp
```

Descriptive names over short ones, small functions with one job, RAII for
anything that owns a resource. Nothing exotic.

## How-to: add a data locale

A locale is a bundle of word lists (names, cities, companies, ...) baked into the
binary as static data. There's no runtime loading.

1. Create `src/data/<code>.cpp` — for example `it_it.cpp`. Copy an existing one
   like `src/data/en_us.cpp` as a template. Each category is a
   `static constexpr std::string_view[]`, and an accessor in the `dodo::data`
   namespace returns a `const Locale&` built from them. The aggregate initializer
   must list members in the **exact order** declared in the `Locale` struct in
   `include/dodo/locale.hpp`:

   ```
   code, first_names, last_names, cities, states, countries, country_codes,
   streets, companies, job_titles, email_domains, words, phone_format,
   postal_format, street_number_first
   ```

   Get that order wrong and the arrays bind to the wrong fields — it'll compile
   and produce nonsense. Leave any category array empty and it falls back to the
   `en_US` base bundle for that one category, so partial locales are fine.

2. Declare the accessor in `src/data/locales.hpp`:

   ```cpp
   const Locale& it_it();
   ```

3. Register the code in `src/locale.cpp` in three places: `locale_exists`,
   `get_locale`, and `available_locales`. Miss `available_locales` and the locale
   works but won't show up in `--list-locales`.

4. Add the file to the `dodo_lib` source list in `CMakeLists.txt`.

A note on text: save the file as UTF-8. Accented characters in the data are
fine — the `email` and `username` generators fold Latin-1 accents down to ASCII
(see `fold_latin1` in `src/generators.cpp`), so `José` yields a clean `jose`
login. If your locale uses letters that table doesn't cover yet, extend it.

Keep starter lists curated and modest (a hundred-ish entries per category is
plenty). Mark them in a comment as a seed set meant to grow.

## How-to: add a field type

Field types are `Generator` subclasses. The plumbing — factory, catalog,
i18n — has to agree on the name, so touch all of it.

1. Write the generator in `src/generators.cpp`. Derive from `Generator` and
   override `generate(Rng&, const std::vector<Value>&)`. Return numeric output via
   `Value::number(...)` so every writer emits it unquoted and identically; format
   it into a reused member `std::string` buffer rather than allocating per row.
   The `row` argument lets you read columns produced earlier in the same row if
   you need a derived value. This runs in the hot loop — it must not throw.

   ```cpp
   class DiceGenerator : public Generator {
   public:
       DiceGenerator(int sides) : sides_(sides) {}
       Value generate(Rng& rng, const std::vector<Value>&) override {
           buf_ = std::to_string(static_cast<int>(rng.next_int(1, sides_)));
           return Value::number(buf_);
       }
   private:
       int sides_;
       std::string buf_;
   };
   ```

2. Wire it into the `build()` factory in the same file, and validate its
   parameters there. Bad input throws `DodoError` with a localized message at
   setup time — that's the right place for validation, never the hot loop.

3. Register it in `field_type_catalog()` with its name, an inline signature
   string, and a `typedesc.<name>` description key (e.g. `typedesc.dice`). This
   entry is what `--list-types`, schema validation, and `is_known_field_type`
   read from.

4. Add that description key to **both** catalogs in `src/i18n.cpp`, `english()`
   and `spanish()`. A key present in one but not the other falls back to English
   at runtime, which is a quiet way to ship an untranslated string.

5. If the type takes inline arguments (the `dice(6)` form), make sure
   `parse_inline_args` / `assign_positional` in `src/schema.cpp` normalize them
   into the same `params` / `enum_*` shape the JSON schema parser produces. Both
   front ends feed the same `Field`, so the factory only sees one representation.

6. Add a test to `tests/test_generators.cpp` — cover the range or format and
   confirm output is deterministic for a fixed seed.

## How-to: add an interface language

Everything user-facing goes through `i18n.tr("key")`, so a new language is mostly
one new catalog.

1. In `src/i18n.cpp`, copy the `english()` catalog and translate every value.
   Keep the keys identical — the keys are the contract, the values are the
   translation.

2. Extend the `Language` enum in `include/dodo/i18n.hpp`, then map the new value
   in three spots: the `I18n` constructor (point `catalog_` at your new catalog),
   `parse_language` (so `--lang xx` selects it), and `detect_language_from_env`
   (so it's picked up from the environment).

If you add a brand-new user-visible string anywhere, add its key to every catalog,
not just the one you're working in.

## Submitting

Keep changes focused — one locale, one field type, or one language per change is
easiest to review. A few things to check before you open a PR:

- Update `CHANGELOG.md`.
- `ctest` passes, and the build is warning-free with `-DDODO_WERROR=ON`.
- If you intentionally changed generation output, update the golden expectation in
  `tests/test_golden.cpp`. That test pins exact bytes for a fixed seed, so it'll
  fail on any output change — which is the point. Update it deliberately, never to
  paper over an accidental change.
