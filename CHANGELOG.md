# Changelog

This project follows [Semantic Versioning](https://semver.org).

## 0.2.0

First public release.

- Streaming generator with constant memory: rows are produced one at a time and
  flushed through a 256 KB buffer, so 100 rows or 100 million rows cost roughly
  the same memory.
- Output formats: CSV, TSV, JSON (array, optional `--pretty`), JSONL/NDJSON, and
  SQL `INSERT`s with `--dialect` (postgres/mysql/sqlite), `--batch`, and an
  optional `--create-table`.
- Around forty field types covering identifiers, numbers, people, companies and
  jobs, places and geo coordinates, internet (email, domain, url, ipv4/ipv6,
  mac, slug, hex color), finance (currency, credit card with a valid Luhn digit),
  dates and times, text, and structured choices (enum/oneof, pattern, constant).
- Within-row references: `email`/`username` can be derived `from` another column,
  and `template("{first} {last}")` interpolates earlier columns. This is how you
  keep an email consistent with the name in the same row.
- Modifiers: nullable columns (`?` inline or `"nullable": true`) honouring a
  global `--null-rate` or a per-field `null_rate`, and `unique` (guaranteed for
  `sequence`/`uuid`, best-effort for the rest).
- Reproducible output from a seedable xoshiro256** PRNG (`--seed`).
- Five data locales: en_US, en_GB, es_ES, fr_FR, de_DE, with en_US as the
  fallback for any missing category.
- English and Spanish interface, chosen with `--lang` or from the environment.
- Inline `--field` specs or a JSON `--schema`, both validated with clear errors.
