#include "dodo/schema.hpp"

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "dodo/generators.hpp"

namespace dodo {
namespace {

using nlohmann::json;

[[noreturn]] void fail(const I18n& i18n, std::string_view key,
                       std::initializer_list<std::string_view> args) {
    throw DodoError(format_message(i18n.tr(key), args));
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }
    return text;
}

std::string strip_quotes(std::string_view text) {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text.remove_prefix(1);
        text.remove_suffix(1);
    }
    return std::string(text);
}

std::vector<std::string> split_top_level(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    for (char c : text) {
        if (c == '"') {
            in_quotes = !in_quotes;
            current.push_back(c);
        } else if (c == ',' && !in_quotes) {
            tokens.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    tokens.push_back(current);
    return tokens;
}

void assign_range(Field& field, std::string_view range) {
    std::size_t separator = range.find("..");
    std::string_view low = trim(range.substr(0, separator));
    std::string_view high = trim(range.substr(separator + 2));
    const bool is_temporal = field.type == "date" || field.type == "datetime";
    field.params[is_temporal ? "start" : "min"] = std::string(low);
    field.params[is_temporal ? "end" : "max"] = std::string(high);
}

void assign_positional(Field& field, int index, std::string_view value, const I18n& i18n) {
    std::string_view type = canonical_type(field.type);
    if (type == "sequence") {
        field.params[index == 0 ? "start" : "step"] = std::string(value);
    } else if (type == "bool") {
        field.params["p"] = std::string(value);
    } else if (type == "string") {
        field.params["len"] = std::string(value);
    } else if (type == "words" || type == "slug") {
        field.params["n"] = std::string(value);
    } else if (type == "pattern") {
        field.params["pattern"] = strip_quotes(value);
    } else if (type == "template") {
        field.params["template"] = strip_quotes(value);
    } else if (type == "constant") {
        field.params["value"] = strip_quotes(value);
    } else {
        fail(i18n, "error.inline_field_syntax", {field.name});
    }
}

void parse_inline_args(Field& field, std::string_view args, const I18n& i18n) {
    std::string_view type = canonical_type(field.type);
    int positional = 0;
    for (const std::string& raw_token : split_top_level(args)) {
        std::string_view token = trim(raw_token);
        if (token.empty()) {
            continue;
        }
        if (type == "enum") {
            std::size_t colon = token.find(':');
            if (colon != std::string_view::npos) {
                field.enum_values.emplace_back(trim(token.substr(0, colon)));
                std::string weight(trim(token.substr(colon + 1)));
                field.enum_weights.push_back(std::strtod(weight.c_str(), nullptr));
            } else {
                field.enum_values.emplace_back(token);
            }
            continue;
        }
        if (token.find("..") != std::string_view::npos) {
            assign_range(field, token);
            continue;
        }
        std::size_t equals = token.find('=');
        if (equals != std::string_view::npos) {
            std::string key(trim(token.substr(0, equals)));
            field.params[key] = strip_quotes(trim(token.substr(equals + 1)));
            continue;
        }
        assign_positional(field, positional++, token, i18n);
    }
}

Field parse_inline_field(std::string_view spec, const I18n& i18n) {
    std::size_t colon = spec.find(':');
    if (colon == std::string_view::npos) {
        fail(i18n, "error.inline_field_syntax", {spec});
    }
    Field field;
    field.name = std::string(trim(spec.substr(0, colon)));
    std::string_view rest = trim(spec.substr(colon + 1));
    if (field.name.empty() || rest.empty()) {
        fail(i18n, "error.inline_field_syntax", {spec});
    }
    if (rest.back() == '?') {
        field.nullable = true;
        rest = trim(rest.substr(0, rest.size() - 1));
    }
    std::size_t open = rest.find('(');
    if (open == std::string_view::npos) {
        field.type = std::string(rest);
        return field;
    }
    if (rest.back() != ')') {
        fail(i18n, "error.inline_field_syntax", {spec});
    }
    field.type = std::string(trim(rest.substr(0, open)));
    std::string_view args = rest.substr(open + 1, rest.size() - open - 2);
    parse_inline_args(field, args, i18n);
    return field;
}

std::string scalar_to_string(const json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "false";
    }
    if (value.is_number_integer() || value.is_number_unsigned()) {
        return std::to_string(value.get<long long>());
    }
    // dump() renders a float without quotes and round-trips cleanly (0.05).
    return value.dump();
}

Field field_from_json(const json& node, const I18n& i18n) {
    if (!node.is_object()) {
        fail(i18n, "error.field_not_object", {});
    }
    Field field;
    if (!node.contains("name") || !node.at("name").is_string() ||
        node.at("name").get<std::string>().empty()) {
        fail(i18n, "error.field_name_missing", {});
    }
    field.name = node.at("name").get<std::string>();
    if (!node.contains("type") || !node.at("type").is_string()) {
        fail(i18n, "error.field_type_missing", {field.name});
    }
    field.type = node.at("type").get<std::string>();

    for (auto it = node.begin(); it != node.end(); ++it) {
        const std::string& key = it.key();
        if (key == "name" || key == "type") {
            continue;
        }
        if (key == "nullable") {
            field.nullable = it.value().is_boolean() && it.value().get<bool>();
        } else if (key == "unique") {
            field.unique = it.value().is_boolean() && it.value().get<bool>();
        } else if (key == "values" && it.value().is_array()) {
            for (const json& value : it.value()) {
                field.enum_values.push_back(scalar_to_string(value));
            }
        } else if (key == "weights" && it.value().is_array()) {
            for (const json& weight : it.value()) {
                field.enum_weights.push_back(weight.get<double>());
            }
        } else {
            field.params[key] = scalar_to_string(it.value());
        }
    }
    return field;
}

void validate_unique_names(const Schema& schema, const I18n& i18n) {
    std::set<std::string> seen;
    for (const Field& field : schema.fields) {
        if (field.name.empty()) {
            fail(i18n, "error.field_name_missing", {});
        }
        if (!seen.insert(field.name).second) {
            fail(i18n, "error.field_name_duplicate", {field.name});
        }
    }
}

} // namespace

Schema build_schema_from_inline(const std::vector<std::string>& field_specs,
                                const I18n& i18n) {
    Schema schema;
    schema.fields.reserve(field_specs.size());
    for (const std::string& spec : field_specs) {
        schema.fields.push_back(parse_inline_field(spec, i18n));
    }
    return schema;
}

Schema load_schema_from_file(const std::string& path, const I18n& i18n) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail(i18n, "error.schema_open", {path});
    }
    std::ostringstream contents;
    contents << input.rdbuf();

    json document;
    try {
        document = json::parse(contents.str());
    } catch (const json::parse_error& error) {
        fail(i18n, "error.schema_parse", {error.what()});
    }
    if (!document.is_object()) {
        fail(i18n, "error.schema_not_object", {});
    }

    Schema schema;
    if (document.contains("locale") && document.at("locale").is_string()) {
        schema.locale_code = document.at("locale").get<std::string>();
        schema.has_locale = true;
    }
    if (!document.contains("fields") || !document.at("fields").is_array() ||
        document.at("fields").empty()) {
        fail(i18n, "error.schema_fields_missing", {});
    }
    for (const json& node : document.at("fields")) {
        schema.fields.push_back(field_from_json(node, i18n));
    }
    return schema;
}

void validate_schema(const Schema& schema, const I18n& i18n) {
    validate_unique_names(schema, i18n);
    for (const Field& field : schema.fields) {
        if (!is_known_field_type(field.type)) {
            fail(i18n, "error.unknown_type", {field.type, field.name});
        }
        if (!field.enum_weights.empty() &&
            field.enum_weights.size() != field.enum_values.size()) {
            fail(i18n, "error.enum_weights_length", {field.name});
        }
    }
}

} // namespace dodo
