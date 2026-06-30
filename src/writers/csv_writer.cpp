#include "writers.hpp"

#include <string>
#include <vector>

namespace dodo {

OutputBuffer::OutputBuffer(std::FILE* out, std::size_t capacity)
    : out_(out), capacity_(capacity) {
    buffer_.reserve(capacity + 64);
}

OutputBuffer::~OutputBuffer() {
    flush();
}

void OutputBuffer::append(std::string_view text) {
    buffer_.append(text);
    if (buffer_.size() >= capacity_)
        flush();
}

void OutputBuffer::append(char c) {
    buffer_.push_back(c);
    if (buffer_.size() >= capacity_)
        flush();
}

void OutputBuffer::flush() {
    if (!buffer_.empty()) {
        std::fwrite(buffer_.data(), 1, buffer_.size(), out_);
        buffer_.clear();
    }
}

namespace {

// RFC 4180: quote a field if it holds the delimiter, a quote, or a line break,
// and double any embedded quotes. Works for tab-delimited output too.
void write_field(OutputBuffer& out, std::string_view text, char delimiter) {
    bool quote = false;
    for (char c : text) {
        if (c == delimiter || c == '"' || c == '\n' || c == '\r') {
            quote = true;
            break;
        }
    }
    if (!quote) {
        out.append(text);
        return;
    }
    out.append('"');
    for (char c : text) {
        if (c == '"')
            out.append('"');
        out.append(c);
    }
    out.append('"');
}

void write_cell(OutputBuffer& out, const Value& v, char delimiter) {
    switch (v.type) {
    case ValueType::Null:
        break;
    case ValueType::Boolean:
        out.append(v.boolean ? "true" : "false");
        break;
    case ValueType::Number:
        out.append(v.text);
        break;
    case ValueType::String:
        write_field(out, v.text, delimiter);
        break;
    }
}

class CsvWriter : public Writer {
  public:
    CsvWriter(OutputBuffer& out, char delimiter, bool header)
        : out_(out), delimiter_(delimiter), header_(header) {}

    void begin(const std::vector<Field>& fields) override {
        if (!header_)
            return;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (i)
                out_.append(delimiter_);
            write_field(out_, fields[i].name, delimiter_);
        }
        out_.append('\n');
    }

    void write_row(const std::vector<Value>& values) override {
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i)
                out_.append(delimiter_);
            write_cell(out_, values[i], delimiter_);
        }
        out_.append('\n');
    }

    void end() override {}

  private:
    OutputBuffer& out_;
    char delimiter_;
    bool header_;
};

} // namespace

std::unique_ptr<Writer> make_csv_writer(const CliOptions& options, OutputBuffer& output) {
    return std::make_unique<CsvWriter>(output, options.delimiter, options.header);
}

std::unique_ptr<Writer> make_writer(const CliOptions& options, OutputBuffer& output) {
    switch (options.format) {
    case OutputFormat::Csv:
    case OutputFormat::Tsv:
        return make_csv_writer(options, output);
    case OutputFormat::Json:
        return make_json_writer(options, output, false);
    case OutputFormat::JsonLines:
        return make_json_writer(options, output, true);
    case OutputFormat::Sql:
        return make_sql_writer(options, output);
    }
    return make_csv_writer(options, output);
}

} // namespace dodo
