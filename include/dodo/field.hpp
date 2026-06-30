#ifndef DODO_FIELD_HPP
#define DODO_FIELD_HPP

#include <map>
#include <string>
#include <vector>

namespace dodo {

// A single column definition. Both the inline `--field` parser and the JSON
// schema parser normalize into this same shape, so the generator factory has a
// single representation to consume. Named parameters are kept as raw strings and
// converted to numbers/dates by the factory at setup time (never in the hot
// loop), which keeps validation and error reporting in one place.
struct Field {
    std::string name;
    std::string type;
    bool nullable = false;
    bool unique = false;

    std::map<std::string, std::string> params;

    // enum/oneof values and their optional weights, kept separate because they
    // are inherently ordered and parallel.
    std::vector<std::string> enum_values;
    std::vector<double> enum_weights;
};

} // namespace dodo

#endif // DODO_FIELD_HPP
