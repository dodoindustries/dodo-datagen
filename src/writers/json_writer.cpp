#include "writers.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace dodo {
namespace {

void write_json_string(OutputBuffer& out, std::string_view text) {
    out.append('"');
    for (char c : text) {
        unsigned char b = static_cast<unsigned char>(c);
        switch (c) {
        case '"':
            out.append("\\\"");
            break;
        case '\\':
            out.append("\\\\");
            break;
        case '\n':
            out.append("\\n");
            break;
        case '\r':
            out.append("\\r");
            break;
        case '\t':
            out.append("\\t");
            break;
        default:
            if (b < 0x20) {
                char esc[8];
                std::snprintf(esc, sizeof(esc), "\\u%04x", b);
                out.append(esc);
            } else {
                out.append(c);
            }
        }
    }
    out.append('"');
}

void write_value(OutputBuffer& out, const Value& v) {
    switch (v.type) {
    case ValueType::Null:
        out.append("null");
        break;
    case ValueType::Boolean:
        out.append(v.boolean ? "true" : "false");
        break;
    case ValueType::Number:
        out.append(v.text);
        break;
    case ValueType::String:
        write_json_string(out, v.text);
        break;
    }
}

// Handles json arrays, jsonl (one object per line), and pretty-printed arrays.
// jsonl ignores --pretty since the point of jsonl is one compact line per row.
class JsonWriter : public Writer {
  public:
    JsonWriter(OutputBuffer& out, bool line_delimited, bool pretty)
        : out_(out), line_(line_delimited), pretty_(pretty && !line_delimited) {}

    void begin(const std::vector<Field>& fields) override {
        for (const Field& f : fields)
            keys_.push_back(f.name);
        if (!line_)
            out_.append('[');
    }

    void write_row(const std::vector<Value>& values) override {
        if (!line_) {
            if (!first_)
                out_.append(',');
            first_ = false;
            if (pretty_)
                out_.append("\n  ");
        }
        out_.append('{');
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i)
                out_.append(pretty_ ? ", " : ",");
            write_json_string(out_, keys_[i]);
            out_.append(pretty_ ? ": " : ":");
            write_value(out_, values[i]);
        }
        out_.append('}');
        if (line_)
            out_.append('\n');
    }

    void end() override {
        if (line_)
            return;
        if (pretty_ && !first_)
            out_.append('\n');
        out_.append("]\n");
    }

  private:
    OutputBuffer& out_;
    bool line_, pretty_, first_ = true;
    std::vector<std::string> keys_;
};

} // namespace

std::unique_ptr<Writer> make_json_writer(const CliOptions& options, OutputBuffer& output,
                                         bool line_delimited) {
    return std::make_unique<JsonWriter>(output, line_delimited, options.pretty);
}

} // namespace dodo
