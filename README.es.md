<!-- Léelo en: [English](README.md) | **Español** -->

```
       __
      <(o )___    dodo-datagen
       ( ._> /
        `---'
```

# Dodo Datagen

Generador de datos falsos por línea de comandos, escrito en C++. Genera filas en
streaming con memoria constante, y rápido.

Léelo en: [English](README.md) | **Español**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/dodoindustries/dodo-datagen/actions/workflows/ci.yml/badge.svg)](https://github.com/dodoindustries/dodo-datagen/actions/workflows/ci.yml)

---

## Qué es y por qué

`dodo-datagen` genera datos de prueba realistas desde la terminal: para sembrar
bases de datos, montar demos, alimentar pruebas de carga o crear *fixtures*.
Está escrito en C++ con dos objetivos que normalmente se pelean entre sí:
velocidad y memoria plana.

- **Streaming con memoria constante.** Las filas se generan y se vuelcan una a
  una. Pedir 100 filas o 100 millones consume aproximadamente la misma RAM.
- **Rápido.** Alrededor de 5–6 millones de filas simples por segundo en un
  portátil moderno, en un solo hilo (ver [Rendimiento](#rendimiento)).
- **Un binario autocontenido.** Sin dependencias en tiempo de ejecución. Los
  datasets de nombres, ciudades y empresas van compilados dentro del binario.
- **Reproducible.** La misma `--seed` produce una salida byte a byte idéntica.
- **Bilingüe.** La interfaz (ayuda, errores, progreso) habla inglés o español, y
  los datos generados se controlan por separado con el locale.

## Características

- Formatos de salida: **CSV**, **TSV**, **JSON** (array), **JSONL/NDJSON** y
  sentencias **SQL** `INSERT`.
- Más de 40 tipos de campo: identificadores, números, personas, lugares,
  internet, finanzas, tiempo, texto y elecciones estructuradas.
- Campos en línea con `--field` o un archivo de esquema `--schema` en JSON.
- Modificadores anulable y único; `enum` con pesos; tasa de nulos configurable.
- Integridad referencial dentro de la fila: emails y usernames derivados del
  nombre, `template(...)` que cose columnas anteriores.
- PRNG sembrable para reproducibilidad (con tests *golden*).
- Locales de datos `en_US`, `en_GB`, `es_ES`, `fr_FR` y `de_DE`, con cadena de
  *fallback* a `en_US`.
- Interfaz en inglés o español, según `--lang` o el entorno.

## Compilar desde el código fuente

Necesitas un compilador de C++17 y CMake ≥ 3.16. Las cabeceras de terceros
(`nlohmann/json` y `doctest`) las descarga CMake con `FetchContent` la primera
vez que configuras, así que **la primera configuración necesita red**.

### Linux / macOS (gcc o clang)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/dodo-datagen --help
```

### Windows (MSVC)

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
.\build\Release\dodo-datagen.exe --help
```

En MSVC el binario queda en `build/Release/`. Ejecuta los tests con:

```bash
ctest --test-dir build --output-on-failure
```

Hay una opción de CMake `DODO_WERROR` para compilar con `-Werror`.

## Inicio rápido

```bash
# 5 filas de CSV a stdout
dodo-datagen -n 5 --field id:sequence --field name:full_name --field email:email

# 1000 filas, varios tipos de campo, reproducible
dodo-datagen -n 1000 -f csv --seed 42 \
  --field "id:sequence" \
  --field "name:full_name" \
  --field "email:email" \
  --field "age:int(18..90)" \
  --field "signup:date(2020-01-01..2024-12-31)"

# JSON Lines (recomendado para grandes volúmenes) a un archivo
dodo-datagen -n 1000000 -f jsonl -o users.jsonl \
  --field id:sequence --field name:full_name --field email:email

# Desde un archivo de esquema, como inserts SQL
dodo-datagen -s examples/users.json -n 100 -f sql --table users > users.sql
```

El alias corto `dodo` funciona en cualquier lugar donde funcione `dodo-datagen`.

## Referencia de la CLI

| Bandera | Descripción | Por defecto |
|---|---|---|
| `--rows, -n <N>` | número de filas a generar | `10` |
| `--format, -f <fmt>` | `csv`, `tsv`, `json`, `jsonl`, `sql` | `csv` |
| `--output, -o <ruta>` | archivo de salida | stdout |
| `--schema, -s <ruta>` | archivo de esquema JSON con los campos | — |
| `--field <nombre:tipo(args)>` | definición de campo en línea; repetible | — |
| `--seed <N>` | semilla del PRNG para salida reproducible | semilla aleatoria (se imprime por stderr) |
| `--locale <código>` | locale de los **datos** (p. ej. `en_US`, `es_ES`) | `en_US` |
| `--lang <en\|es>` | idioma de la **interfaz** | de `$LANG`/`$LC_ALL`, *fallback* `en` |
| `--delimiter <car>` | delimitador para CSV | `,` |
| `--no-header` | omitir la fila de cabecera en CSV | cabecera activada |
| `--null-rate <0..1>` | probabilidad de `null` en campos anulables | `0` |
| `--table <nombre>` | nombre de tabla para la salida SQL | `data` |
| `--batch <N>` | filas por sentencia `INSERT` | `500` |
| `--dialect <postgres\|mysql\|sqlite>` | dialecto SQL (comillas y tipos en `CREATE TABLE`) | `postgres` |
| `--create-table` | emitir un `CREATE TABLE` antes de los inserts | — |
| `--pretty` | indentar la salida JSON (formato array) | compacto |
| `--quiet` | no mostrar progreso por stderr | progreso activado para N grande |
| `--list-types` | listar los tipos de campo disponibles y salir | — |
| `--list-locales` | listar los locales disponibles y salir | — |
| `--help, -h` | ayuda localizada | — |
| `--version, -v` | versión | — |

**Precedencia.** Un `--locale` en la línea de comandos sobrescribe el `locale`
definido dentro de un esquema. Define los campos con `--schema` **o** con
banderas `--field`, no ambas a la vez. El progreso para N grande va **siempre a
stderr**, así nunca contamina los datos de stdout.

## Tipos de campo

Los argumentos en línea van entre paréntesis; los mismos parámetros existen como
claves en el esquema JSON. Ejecuta `dodo-datagen --list-types` para el listado
localizado.

**Identificadores**

| Tipo | Firma | Notas |
|---|---|---|
| `sequence` (alias `id`) | `sequence(start=1, step=1)` | entero autoincremental |
| `uuid` | `uuid` | UUID aleatorio |

**Números**

| Tipo | Firma | Notas |
|---|---|---|
| `int` | `int(min..max)` | entero aleatorio en rango |
| `float` | `float(min..max, decimals=2)` | decimal aleatorio |
| `price` | `price(min..max, decimals=2)` | precio aleatorio |
| `bool` | `bool(p=0.5)` | `true` con probabilidad `p` |

**Personas**

| Tipo | Firma | Notas |
|---|---|---|
| `first_name` / `last_name` / `full_name` | — | según el locale |
| `username` | `username(from=campo)` | derivable de otra columna |
| `email` | `email(domain=..., from=campo)` | derivable del nombre |
| `phone` | `phone` | formato telefónico del locale |
| `company` | `company` | nombre de empresa |
| `job_title` | `job_title` | puesto de trabajo |

**Lugares**

| Tipo | Notas |
|---|---|
| `city` / `state` / `country` / `country_code` | según el locale |
| `street_address` / `postal_code` | dirección y código postal |
| `latitude` / `longitude` | coordenadas |

**Internet**

| Tipo | Firma | Notas |
|---|---|---|
| `ipv4` / `ipv6` / `mac_address` | — | direcciones de red |
| `domain` / `url` | — | dominio y URL |
| `slug` | `slug(n=3)` | *slug* de `n` palabras |
| `hex_color` | — | color hex |

**Finanzas**

| Tipo | Notas |
|---|---|
| `currency_code` | código de divisa |
| `credit_card` | 16 dígitos con dígito de control Luhn válido |

**Tiempo**

| Tipo | Firma | Notas |
|---|---|---|
| `date` | `date(start..end, fmt=%Y-%m-%d)` | fecha aleatoria en rango |
| `datetime` | `datetime(start..end, fmt=%Y-%m-%d %H:%M:%S)` | fecha y hora |
| `time` | `time` | hora del día |

**Texto**

| Tipo | Firma | Notas |
|---|---|---|
| `word` / `words` / `sentence` / `paragraph` (alias `text`) | `words(n)` | texto de relleno |
| `string` | `string(len)` | alfanumérico aleatorio de longitud `len` |

**Estructurados**

| Tipo | Firma | Notas |
|---|---|---|
| `enum` (alias `oneof`) | `enum(a, b, c)` o `enum(active:0.6, trial:0.3)` | pesos opcionales |
| `pattern` | `pattern("###-@@")` | `#` = dígito, `@` = letra, el resto literal |
| `template` | `template("{col} ...")` | interpola columnas anteriores |
| `constant` | `constant(valor)` | siempre el mismo valor |

### Referencias dentro de la fila

Algunos tipos pueden derivar su valor de columnas definidas **antes**:

- `email` y `username` con `from=<columna>` toman el nombre de esa columna, de
  modo que el email cuadra con la persona.
- `template("{first} {last}")` cose columnas anteriores en una sola cadena.

Las referencias deben apuntar a columnas anteriores. Para emails, usernames y
slugs los acentos se pliegan a ASCII (`José` → `jose`).

### Modificadores

- *Anulable:* sufijo `?` en línea (`notes:sentence?`) o `"nullable": true` en el
  esquema. Los campos anulables emiten `null` con la probabilidad global
  `--null-rate` o con un `"null_rate"` propio del campo.
- *Único:* `"unique": true`. Garantizado para `sequence` y `uuid`. Para todo lo
  demás es de mejor esfuerzo: mantiene un conjunto en memoria (por lo que pierde
  la propiedad de memoria constante) y, cuando un dominio pequeño se agota,
  desambigua con un sufijo `-N`.

## Archivos de esquema

Un esquema es un objeto JSON con un `locale` opcional y un array `fields`. Cada
campo tiene `name`, `type` y los parámetros del tipo como claves; además acepta
`nullable`, `null_rate`, `unique` y, para `enum`, `values`/`weights`.

```json
{
  "locale": "es_ES",
  "fields": [
    { "name": "id",        "type": "sequence", "start": 1 },
    { "name": "uuid",      "type": "uuid" },
    { "name": "name",      "type": "full_name" },
    { "name": "email",     "type": "email", "domain": "example.com", "from": "name" },
    { "name": "age",       "type": "int", "min": 18, "max": 90 },
    { "name": "city",      "type": "city" },
    { "name": "status",    "type": "enum", "values": ["active", "trial", "churned"], "weights": [0.6, 0.1, 0.3] },
    { "name": "signup_at", "type": "datetime", "start": "2020-01-01", "end": "2024-12-31" },
    { "name": "is_admin",  "type": "bool", "p": 0.05 },
    { "name": "notes",     "type": "sentence", "nullable": true }
  ]
}
```

Un `--locale` en la línea de comandos sobrescribe el `locale` del esquema. La
validación rechaza nombres vacíos o duplicados, tipos desconocidos, rangos
incoherentes (`min > max`, `start > end`, fechas inválidas) y pesos de `enum`
que no cuadran, con un mensaje claro y un código de salida distinto de cero.

En [`examples/`](examples/) hay varios esquemas listos para usar: `users.json`
(`es_ES`, email/username derivados, empresa y puesto, `enum` con pesos, notas
anulables), `transactions.json` (`price`, `currency_code`, `credit_card`,
`ipv4`), `products.json` (`slug`, `url`, `hex_color`) y `events.json`
(`template`, `latitude`/`longitude`).

```bash
dodo-datagen -s examples/transactions.json -n 1000 -f jsonl -o txns.jsonl
dodo-datagen -s examples/products.json     -n 500  -f csv   -o products.csv
```

## Formatos de salida

- **CSV** — fila de cabecera salvo `--no-header`. Escape conforme a RFC 4180:
  los campos con el delimitador, comillas o saltos de línea se entrecomillan y
  las comillas internas se duplican. El delimitador se cambia con `--delimiter`.
- **TSV** — como CSV pero separado por tabuladores.
- **JSON (array)** — emite `[`, objetos separados por comas y luego `]`, en
  streaming. Compacto por defecto; `--pretty` lo indenta.
- **JSONL / NDJSON** — un objeto JSON por línea; **lo mejor para grandes
  volúmenes**.
- **SQL** — `INSERT INTO <tabla> (col, …) VALUES (…), (…);` agrupando por
  `--batch` filas (500 por defecto). Con `--table` cambias el nombre de la tabla,
  con `--dialect postgres|mysql|sqlite` ajustas el entrecomillado de
  identificadores y los tipos de `CREATE TABLE`, y con `--create-table` emites
  primero la sentencia de creación.

## Locales e idiomas

Son dos cosas distintas y vale la pena no confundirlas:

- **Idioma de interfaz** (`--lang en|es`) controla el idioma de la ayuda, los
  errores y el progreso. Si no lo indicas, se lee de `$LANG`/`$LC_ALL`, con
  *fallback* a inglés.
- **Locale de datos** (`--locale en_US|es_ES|…`) controla el idioma y el estilo
  de los valores *generados*: nombres, ciudades, formato de teléfono, etc.

Son ortogonales: puedes leer la ayuda en español mientras generas datos de EE.
UU., o al revés. Hay cinco locales de datos —`en_US`, `en_GB`, `es_ES`, `fr_FR`,
`de_DE`— y los listas con `--list-locales`. Si un locale no aporta datos para
alguna categoría, esa categoría cae a `en_US`.

## Rendimiento

Memoria constante independientemente de N: las filas nunca se acumulan, se
generan y se vuelcan una a una a través de un búfer en espacio de usuario.

Reproduce un benchmark (los números son aproximados y dependen del hardware):

```bash
time dodo-datagen -n 5000000 -f jsonl --quiet \
  --field id:sequence --field name:full_name --field "age:int(18..90)" > /dev/null
```

En un portátil moderno esto ronda **5–6 millones de filas/segundo** en un solo
hilo, con la memoria prácticamente plana. Los esquemas más pesados (más texto,
más campos) son más lentos en proporción al trabajo por fila.

## Reproducibilidad

`--seed N` hace la salida **byte a byte idéntica** entre ejecuciones; es lo que
verifican los tests *golden*. Sin `--seed`, el generador toma la semilla de una
fuente del sistema e imprime la semilla elegida por stderr, para que puedas
reproducir esa ejecución más tarde.

## Roadmap / fuera de alcance por ahora

Dodo Datagen es deliberadamente acotado. Lo que **no** intenta ser:

- Un motor de datos sintéticos estadísticos (no aprende distribuciones de datos
  reales).
- Un driver de base de datos: escribe a archivo o stdout y tú lo canalizas o
  importas.
- Una herramienta con GUI.
- Integridad referencial entre tablas: hay referencias dentro de la fila, pero
  todavía no claves foráneas entre tablas separadas.

Ideas para más adelante: más locales, relaciones entre tablas, generación en
paralelo y datasets aportados por quien lo usa.

## Contribuir

Mira [CONTRIBUTING.md](CONTRIBUTING.md) para saber cómo añadir un locale de
datos, un tipo de campo o un idioma de interfaz.

## Licencia

[MIT](LICENSE).
