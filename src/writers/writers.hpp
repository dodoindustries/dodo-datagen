#ifndef DODO_WRITERS_INTERNAL_HPP
#define DODO_WRITERS_INTERNAL_HPP

#include <memory>

#include "dodo/writer.hpp"

namespace dodo {

std::unique_ptr<Writer> make_csv_writer(const CliOptions& options, OutputBuffer& output);
std::unique_ptr<Writer> make_json_writer(const CliOptions& options, OutputBuffer& output,
                                         bool line_delimited);
std::unique_ptr<Writer> make_sql_writer(const CliOptions& options, OutputBuffer& output);

} // namespace dodo

#endif // DODO_WRITERS_INTERNAL_HPP
