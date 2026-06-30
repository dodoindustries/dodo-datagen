#include "dodo/i18n.hpp"

#include <cstdlib>

namespace dodo {
namespace {

using Catalog = std::unordered_map<std::string_view, std::string_view>;

constexpr const char* kHelpEn =
    "dodo-datagen - generate realistic fake data, fast\n"
    "\n"
    "  usage: dodo-datagen [options] (--schema FILE | --field SPEC ...)\n"
    "\n"
    "  -n, --rows N          rows to generate (default 10)\n"
    "  -f, --format FMT      csv, tsv, json, jsonl or sql (default csv)\n"
    "  -o, --output PATH     write here instead of stdout\n"
    "  -s, --schema FILE     JSON schema describing the columns\n"
    "      --field SPEC      one column inline, e.g. age:int(18..90); repeatable\n"
    "      --seed N          fix the PRNG seed for reproducible output\n"
    "      --locale CODE     data locale: en_US, en_GB, es_ES, fr_FR, de_DE\n"
    "      --lang en|es      language of this program's messages\n"
    "      --delimiter C     field delimiter for csv (default ,)\n"
    "      --no-header       drop the header row (csv/tsv)\n"
    "      --null-rate R     chance of null in nullable columns (0..1)\n"
    "      --table NAME      table name for sql output (default data)\n"
    "      --batch N         rows per INSERT (default 500)\n"
    "      --dialect D       sql dialect: postgres, mysql or sqlite\n"
    "      --create-table    emit a CREATE TABLE before the inserts\n"
    "      --pretty          pretty-print json arrays\n"
    "      --quiet           no progress on stderr\n"
    "      --list-types      print the field types and exit\n"
    "      --list-locales    print the data locales and exit\n"
    "  -h, --help            show this help\n"
    "  -v, --version         show the version\n"
    "\n"
    "  example:\n"
    "    dodo-datagen -n 1000 -f jsonl \\\n"
    "      --field name:full_name --field 'email:email(from=name)' \\\n"
    "      --field 'age:int(18..90)' --field 'joined:date(2020-01-01..2024-12-31)'\n"
    "\n"
    "  run 'dodo-datagen --list-types' for the full list of column types.\n";

constexpr const char* kHelpEs =
    "dodo-datagen - genera datos falsos realistas, rapido\n"
    "\n"
    "  uso: dodo-datagen [opciones] (--schema ARCHIVO | --field SPEC ...)\n"
    "\n"
    "  -n, --rows N          filas a generar (por defecto 10)\n"
    "  -f, --format FMT      csv, tsv, json, jsonl o sql (por defecto csv)\n"
    "  -o, --output RUTA     escribir aqui en lugar de stdout\n"
    "  -s, --schema ARCHIVO  esquema JSON con las columnas\n"
    "      --field SPEC      una columna en linea, p. ej. age:int(18..90); repetible\n"
    "      --seed N          fija la semilla del PRNG para salida reproducible\n"
    "      --locale CODE     locale de datos: en_US, en_GB, es_ES, fr_FR, de_DE\n"
    "      --lang en|es      idioma de los mensajes del programa\n"
    "      --delimiter C     delimitador para csv (por defecto ,)\n"
    "      --no-header       quitar la fila de cabecera (csv/tsv)\n"
    "      --null-rate R     probabilidad de null en columnas anulables (0..1)\n"
    "      --table NOMBRE    nombre de tabla para la salida sql (por defecto data)\n"
    "      --batch N         filas por INSERT (por defecto 500)\n"
    "      --dialect D       dialecto sql: postgres, mysql o sqlite\n"
    "      --create-table    emitir un CREATE TABLE antes de los inserts\n"
    "      --pretty          formatear arreglos json\n"
    "      --quiet           sin progreso en stderr\n"
    "      --list-types      imprimir los tipos de campo y salir\n"
    "      --list-locales    imprimir los locales de datos y salir\n"
    "  -h, --help            mostrar esta ayuda\n"
    "  -v, --version         mostrar la version\n"
    "\n"
    "  ejemplo:\n"
    "    dodo-datagen -n 1000 -f jsonl \\\n"
    "      --field name:full_name --field 'email:email(from=name)' \\\n"
    "      --field 'age:int(18..90)' --field 'joined:date(2020-01-01..2024-12-31)'\n"
    "\n"
    "  usa 'dodo-datagen --list-types' para la lista completa de tipos.\n";

const Catalog& english() {
    static const Catalog c = {
        {"error.unknown_option", "unknown option: {}"},
        {"error.missing_value", "option {} needs a value"},
        {"error.invalid_rows", "--rows must be a non-negative integer, got: {}"},
        {"error.invalid_format", "unknown format: {} (use csv, tsv, json, jsonl or sql)"},
        {"error.invalid_seed", "--seed must be a non-negative integer, got: {}"},
        {"error.invalid_null_rate", "--null-rate must be between 0 and 1, got: {}"},
        {"error.invalid_batch", "--batch must be a positive integer, got: {}"},
        {"error.invalid_delimiter", "--delimiter must be a single character, got: {}"},
        {"error.invalid_dialect",
         "unknown SQL dialect: {} (use postgres, mysql or sqlite)"},
        {"error.schema_and_fields", "use --schema or --field, not both"},
        {"error.no_fields",
         "no fields defined; pass --schema or one or more --field options"},
        {"error.unknown_locale", "unknown data locale: {}"},
        {"error.schema_open", "cannot open schema file: {}"},
        {"error.schema_parse", "invalid JSON in schema: {}"},
        {"error.schema_not_object", "schema root must be a JSON object"},
        {"error.schema_fields_missing", "schema needs a non-empty \"fields\" array"},
        {"error.field_not_object", "every field must be a JSON object"},
        {"error.field_name_missing", "a field is missing a non-empty \"name\""},
        {"error.field_name_duplicate", "duplicate field name: {}"},
        {"error.field_type_missing", "field \"{}\" is missing a \"type\""},
        {"error.unknown_type", "unknown field type \"{}\" in field \"{}\""},
        {"error.inline_field_syntax", "could not parse --field: {}"},
        {"error.range_order", "min must be <= max in field \"{}\""},
        {"error.invalid_date", "invalid date \"{}\" in field \"{}\" (expected YYYY-MM-DD)"},
        {"error.date_order", "start date must be <= end date in field \"{}\""},
        {"error.enum_empty", "enum field \"{}\" needs at least one value"},
        {"error.enum_weights_length",
         "enum weights must match the number of values in field \"{}\""},
        {"error.enum_weights_invalid",
         "enum weights must be non-negative and not all zero in field \"{}\""},
        {"error.param_int", "parameter \"{}\" must be an integer in field \"{}\""},
        {"error.param_number", "parameter \"{}\" must be a number in field \"{}\""},
        {"error.pattern_missing", "pattern field \"{}\" needs a template"},
        {"error.constant_missing", "constant field \"{}\" needs a value"},
        {"error.template_missing", "template field \"{}\" needs a template string"},
        {"error.template_syntax", "unbalanced braces in template field \"{}\""},
        {"error.unknown_reference", "field \"{1}\" references unknown field \"{0}\""},
        {"error.forward_reference",
         "field \"{1}\" references \"{0}\", which must come earlier"},
        {"error.output_open", "cannot open output file: {}"},
        {"progress.seed", "seed: {}"},
        {"progress.rows", "{} / {} rows"},
        {"progress.done", "{} rows generated"},
        {"list.types.header", "Field types:"},
        {"list.locales.header", "Data locales:"},
        {"typedesc.sequence", "auto-incrementing integer (alias: id)"},
        {"typedesc.uuid", "random UUID v4"},
        {"typedesc.int", "random integer in a range"},
        {"typedesc.float", "random decimal number"},
        {"typedesc.price", "random monetary amount (float with 2 decimals)"},
        {"typedesc.bool", "true with probability p"},
        {"typedesc.first_name", "given name (locale-aware)"},
        {"typedesc.last_name", "family name (locale-aware)"},
        {"typedesc.full_name", "given + family name"},
        {"typedesc.username", "handle, optionally derived from another field"},
        {"typedesc.email", "email address; accents folded to ASCII"},
        {"typedesc.phone", "phone number in the locale's format"},
        {"typedesc.company", "company name"},
        {"typedesc.job_title", "job title"},
        {"typedesc.city", "city name"},
        {"typedesc.state", "state, province or region"},
        {"typedesc.country", "country name"},
        {"typedesc.country_code", "two-letter country code"},
        {"typedesc.street_address", "street address"},
        {"typedesc.postal_code", "postal / ZIP code"},
        {"typedesc.latitude", "latitude between -90 and 90"},
        {"typedesc.longitude", "longitude between -180 and 180"},
        {"typedesc.ipv4", "IPv4 address"},
        {"typedesc.ipv6", "IPv6 address"},
        {"typedesc.mac_address", "MAC address"},
        {"typedesc.domain", "domain name"},
        {"typedesc.url", "http URL"},
        {"typedesc.slug", "url-friendly slug of n words"},
        {"typedesc.hex_color", "hex color like #1a2b3c"},
        {"typedesc.currency_code", "ISO 4217 currency code"},
        {"typedesc.credit_card", "16-digit card number with a valid Luhn check"},
        {"typedesc.date", "date in a range"},
        {"typedesc.datetime", "date and time in a range"},
        {"typedesc.time", "time of day"},
        {"typedesc.word", "a single word"},
        {"typedesc.words", "n words"},
        {"typedesc.sentence", "a sentence"},
        {"typedesc.paragraph", "a paragraph (alias: text)"},
        {"typedesc.string", "random alphanumeric string of length len"},
        {"typedesc.enum", "one of the given values, optionally weighted (alias: oneof)"},
        {"typedesc.pattern", "fill a template: # digit, @ letter, else literal"},
        {"typedesc.template", "interpolate {column} references from earlier fields"},
        {"typedesc.constant", "the same value every time"},
        {"help.text", kHelpEn},
    };
    return c;
}

const Catalog& spanish() {
    static const Catalog c = {
        {"error.unknown_option", "opcion desconocida: {}"},
        {"error.missing_value", "la opcion {} necesita un valor"},
        {"error.invalid_rows", "--rows debe ser un entero no negativo, se recibio: {}"},
        {"error.invalid_format",
         "formato desconocido: {} (usa csv, tsv, json, jsonl o sql)"},
        {"error.invalid_seed", "--seed debe ser un entero no negativo, se recibio: {}"},
        {"error.invalid_null_rate", "--null-rate debe estar entre 0 y 1, se recibio: {}"},
        {"error.invalid_batch", "--batch debe ser un entero positivo, se recibio: {}"},
        {"error.invalid_delimiter",
         "--delimiter debe ser un solo caracter, se recibio: {}"},
        {"error.invalid_dialect",
         "dialecto SQL desconocido: {} (usa postgres, mysql o sqlite)"},
        {"error.schema_and_fields", "usa --schema o --field, no ambos"},
        {"error.no_fields",
         "no se definieron campos; pasa --schema o una o mas opciones --field"},
        {"error.unknown_locale", "locale de datos desconocido: {}"},
        {"error.schema_open", "no se puede abrir el archivo de esquema: {}"},
        {"error.schema_parse", "JSON invalido en el esquema: {}"},
        {"error.schema_not_object", "la raiz del esquema debe ser un objeto JSON"},
        {"error.schema_fields_missing",
         "el esquema necesita un arreglo \"fields\" no vacio"},
        {"error.field_not_object", "cada campo debe ser un objeto JSON"},
        {"error.field_name_missing", "a un campo le falta un \"name\" no vacio"},
        {"error.field_name_duplicate", "nombre de campo duplicado: {}"},
        {"error.field_type_missing", "al campo \"{}\" le falta un \"type\""},
        {"error.unknown_type", "tipo de campo desconocido \"{}\" en el campo \"{}\""},
        {"error.inline_field_syntax", "no se pudo interpretar --field: {}"},
        {"error.range_order", "min debe ser <= max en el campo \"{}\""},
        {"error.invalid_date",
         "fecha invalida \"{}\" en el campo \"{}\" (se esperaba AAAA-MM-DD)"},
        {"error.date_order", "la fecha inicial debe ser <= la final en el campo \"{}\""},
        {"error.enum_empty", "el campo enum \"{}\" necesita al menos un valor"},
        {"error.enum_weights_length",
         "los pesos deben coincidir con la cantidad de valores en el campo \"{}\""},
        {"error.enum_weights_invalid",
         "los pesos deben ser no negativos y no todos cero en el campo \"{}\""},
        {"error.param_int", "el parametro \"{}\" debe ser un entero en el campo \"{}\""},
        {"error.param_number", "el parametro \"{}\" debe ser un numero en el campo \"{}\""},
        {"error.pattern_missing", "el campo pattern \"{}\" necesita una plantilla"},
        {"error.constant_missing", "el campo constant \"{}\" necesita un valor"},
        {"error.template_missing", "el campo template \"{}\" necesita una plantilla"},
        {"error.template_syntax", "llaves desbalanceadas en el campo template \"{}\""},
        {"error.unknown_reference",
         "el campo \"{1}\" referencia un campo desconocido \"{0}\""},
        {"error.forward_reference",
         "el campo \"{1}\" referencia \"{0}\", que debe ir antes"},
        {"error.output_open", "no se puede abrir el archivo de salida: {}"},
        {"progress.seed", "semilla: {}"},
        {"progress.rows", "{} / {} filas"},
        {"progress.done", "{} filas generadas"},
        {"list.types.header", "Tipos de campo:"},
        {"list.locales.header", "Locales de datos:"},
        {"typedesc.sequence", "entero autoincremental (alias: id)"},
        {"typedesc.uuid", "UUID v4 aleatorio"},
        {"typedesc.int", "entero aleatorio en un rango"},
        {"typedesc.float", "numero decimal aleatorio"},
        {"typedesc.price", "importe monetario aleatorio (float con 2 decimales)"},
        {"typedesc.bool", "true con probabilidad p"},
        {"typedesc.first_name", "nombre de pila (segun el locale)"},
        {"typedesc.last_name", "apellido (segun el locale)"},
        {"typedesc.full_name", "nombre y apellido"},
        {"typedesc.username", "usuario, opcionalmente derivado de otro campo"},
        {"typedesc.email", "correo electronico; acentos a ASCII"},
        {"typedesc.phone", "telefono en el formato del locale"},
        {"typedesc.company", "nombre de empresa"},
        {"typedesc.job_title", "puesto de trabajo"},
        {"typedesc.city", "ciudad"},
        {"typedesc.state", "estado, provincia o region"},
        {"typedesc.country", "pais"},
        {"typedesc.country_code", "codigo de pais de dos letras"},
        {"typedesc.street_address", "direccion postal"},
        {"typedesc.postal_code", "codigo postal"},
        {"typedesc.latitude", "latitud entre -90 y 90"},
        {"typedesc.longitude", "longitud entre -180 y 180"},
        {"typedesc.ipv4", "direccion IPv4"},
        {"typedesc.ipv6", "direccion IPv6"},
        {"typedesc.mac_address", "direccion MAC"},
        {"typedesc.domain", "nombre de dominio"},
        {"typedesc.url", "URL http"},
        {"typedesc.slug", "slug de n palabras"},
        {"typedesc.hex_color", "color hex como #1a2b3c"},
        {"typedesc.currency_code", "codigo de moneda ISO 4217"},
        {"typedesc.credit_card", "tarjeta de 16 digitos con Luhn valido"},
        {"typedesc.date", "fecha en un rango"},
        {"typedesc.datetime", "fecha y hora en un rango"},
        {"typedesc.time", "hora del dia"},
        {"typedesc.word", "una palabra"},
        {"typedesc.words", "n palabras"},
        {"typedesc.sentence", "una oracion"},
        {"typedesc.paragraph", "un parrafo (alias: text)"},
        {"typedesc.string", "cadena alfanumerica de longitud len"},
        {"typedesc.enum", "uno de los valores, opcionalmente ponderado (alias: oneof)"},
        {"typedesc.pattern", "rellena una plantilla: # digito, @ letra, resto literal"},
        {"typedesc.template", "interpola referencias {columna} de campos anteriores"},
        {"typedesc.constant", "el mismo valor siempre"},
        {"help.text", kHelpEs},
    };
    return c;
}

bool looks_spanish(const char* v) {
    return v && v[0] == 'e' && v[1] == 's';
}

} // namespace

Language parse_language(std::string_view code) {
    return (code == "es" || code == "es_ES" || code == "es-ES") ? Language::Spanish
                                                                : Language::English;
}

Language detect_language_from_env() {
    if (const char* v = std::getenv("LC_ALL"); v && *v)
        return looks_spanish(v) ? Language::Spanish : Language::English;
    if (const char* v = std::getenv("LANG"); v && *v)
        return looks_spanish(v) ? Language::Spanish : Language::English;
    return Language::English;
}

I18n::I18n(Language language) : language_(language) {
    catalog_ = (language == Language::Spanish) ? &spanish() : &english();
}

std::string_view I18n::tr(std::string_view key) const {
    if (auto it = catalog_->find(key); it != catalog_->end())
        return it->second;
    if (auto it = english().find(key); it != english().end())
        return it->second;
    return key;
}

// Replaces "{}" placeholders in order, and also "{0}".."{9}" by position so a
// template can reuse or reorder arguments (used by the reference errors).
std::string format_message(std::string_view tmpl,
                           std::initializer_list<std::string_view> args) {
    std::string out;
    out.reserve(tmpl.size());
    auto next = args.begin();
    for (std::size_t i = 0; i < tmpl.size(); ++i) {
        if (tmpl[i] != '{') {
            out.push_back(tmpl[i]);
            continue;
        }
        if (i + 1 < tmpl.size() && tmpl[i + 1] == '}') {
            if (next != args.end())
                out.append(next->begin(), next->end()), ++next;
            i += 1;
        } else if (i + 2 < tmpl.size() && tmpl[i + 2] == '}' && tmpl[i + 1] >= '0' &&
                   tmpl[i + 1] <= '9') {
            std::size_t idx = static_cast<std::size_t>(tmpl[i + 1] - '0');
            if (idx < args.size()) {
                auto it = args.begin();
                std::advance(it, idx);
                out.append(it->begin(), it->end());
            }
            i += 2;
        } else {
            out.push_back('{');
        }
    }
    return out;
}

} // namespace dodo
