#include <vector>
#include <string>
#include "strings.hpp"


// to_string(model) == "x y -z"
std::string to_string(tclause model, bool neg) {
    std::string str;

    for (int lit : model) {
        if (neg) {
            lit = -lit;
        }

        str += std::to_string(lit) + ' ';
    }

    if (str.size() > 1 && str[str.size()-1] == ' ') {
        str.resize(str.size()-1);
    }

    return str;
}


// to_string(models) == "model1 | model2"
std::string to_string(tcnf models, bool neg) {

    std::string str;

    for (tclause model : models) {
        str += to_string(model, neg) + " | ";
    }

    if (str.size() > 2 && str[str.size()-2] == '|') {
        str.resize(str.size()-2);
    }

    return str;
}
