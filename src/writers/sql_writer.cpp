#include "writers.hpp"

#include <string>
#include <vector>

#include "dodo/generators.hpp"

namespace dodo {
namespace {

char identifier_quote(SqlDialect dialect) {
    return dialect == SqlDialect::MySql ? '`' : '"';
}

void write_identifier(OutputBuffer& out, std::string_view name, SqlDialect dialect) {
    char q = identifier_quote(dialect);
    out.append(q);
    for (char c : name) {
        if (c == q)
            out.append(q);
        out.append(c);
    }
    out.append(q);
}

// Single quotes are doubled everywhere; MySQL also treats backslash as an escape
// character in string literals, so we double it for that dialect.
void write_string(OutputBuffer& out, std::string_view text, SqlDialect dialect) {
    out.append('\'');
    for (char c : text) {
        if (c == '\'')
            out.append('\'');
        else if (c == '\\' && dialect == SqlDialect::MySql)
            out.append('\\');
        out.append(c);
    }
    out.append('\'');
}

void write_value(OutputBuffer& out, const Value& v, SqlDialect dialect) {
    switch (v.type) {
    case ValueType::Null:
        out.append("NULL");
        break;
    case ValueType::Boolean:
        out.append(v.boolean ? "TRUE" : "FALSE");
        break;
    case ValueType::Number:
        out.append(v.text);
        break;
    case ValueType::String:
        write_string(out, v.text, dialect);
        break;
    }
}

std::string_view column_type(std::string_view field_type, SqlDialect dialect) {
    std::string_view t = canonical_type(field_type);
    bool pg = dialect == SqlDialect::Postgres;
    bool my = dialect == SqlDialect::MySql;
    if (t == "sequence" || t == "int")
        return "BIGINT";
    if (t == "float" || t == "price" || t == "latitude" || t == "longitude")
        return my ? "DOUBLE" : (pg ? "DOUBLE PRECISION" : "REAL");
    if (t == "bool")
        return pg ? "BOOLEAN" : (my ? "TINYINT(1)" : "INTEGER");
    if (t == "date")
        return "DATE";
    if (t == "datetime")
        return pg ? "TIMESTAMP" : (my ? "DATETIME" : "TEXT");
    if (t == "uuid")
        return pg ? "UUID" : (my ? "CHAR(36)" : "TEXT");
    return "TEXT";
}

class SqlWriter : public Writer {
  public:
    SqlWriter(OutputBuffer& out, std::string table, std::int64_t batch, SqlDialect dialect,
              bool create_table)
        : out_(out), table_(std::move(table)), batch_(batch < 1 ? 1 : batch),
          dialect_(dialect), create_table_(create_table) {}

    void begin(const std::vector<Field>& fields) override {
        for (const Field& f : fields) {
            std::string col;
            col.push_back(identifier_quote(dialect_));
            col.append(f.name);
            col.push_back(identifier_quote(dialect_));
            columns_.push_back(col);
        }

        if (create_table_) {
            out_.append("CREATE TABLE ");
            write_identifier(out_, table_, dialect_);
            out_.append(" (\n");
            for (std::size_t i = 0; i < fields.size(); ++i) {
                out_.append("    ");
                write_identifier(out_, fields[i].name, dialect_);
                out_.append(' ');
                out_.append(column_type(fields[i].type, dialect_));
                out_.append(i + 1 < fields.size() ? ",\n" : "\n");
            }
            out_.append(");\n");
        }

        column_list_.append(" (");
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            if (i)
                column_list_.append(", ");
            column_list_.append(columns_[i]);
        }
        column_list_.append(")");
    }

    void write_row(const std::vector<Value>& values) override {
        if (rows_in_batch_ == 0) {
            out_.append("INSERT INTO ");
            write_identifier(out_, table_, dialect_);
            out_.append(column_list_);
            out_.append(" VALUES\n");
        } else {
            out_.append(",\n");
        }
        out_.append('(');
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i)
                out_.append(", ");
            write_value(out_, values[i], dialect_);
        }
        out_.append(')');
        if (++rows_in_batch_ >= batch_) {
            out_.append(";\n");
            rows_in_batch_ = 0;
        }
    }

    void end() override {
        if (rows_in_batch_ > 0) {
            out_.append(";\n");
            rows_in_batch_ = 0;
        }
    }

  private:
    OutputBuffer& out_;
    std::string table_;
    std::vector<std::string> columns_;
    std::string column_list_;
    std::int64_t batch_;
    SqlDialect dialect_;
    bool create_table_;
    std::int64_t rows_in_batch_ = 0;
};

} // namespace

std::unique_ptr<Writer> make_sql_writer(const CliOptions& options, OutputBuffer& output) {
    return std::make_unique<SqlWriter>(output, options.table, options.batch,
                                       options.dialect, options.create_table);
}

} // namespace dodo
