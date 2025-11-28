#ifndef STRINGS
#define STRINGS

#include <string>
#include <vector>

using tclause = std::vector<int>;
using tcnf = std::vector<tclause>;

std::string to_string(tclause model, bool neg = false);
std::string to_string(tcnf model, bool neg = false);
std::string to_string(const std::vector<int> &values, const std::vector<int> &dls, const std::vector<bool> &is_ds);

#endif
