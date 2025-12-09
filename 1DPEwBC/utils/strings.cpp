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

// to_string(values, dls, is_ds) == ..."
std::string to_string(const std::vector<int> &stack, const std::vector<int> &values, const std::vector<int> &dls, const std::vector<bool> &is_ds) {
    std::string top = "c lvl (" + std::to_string(dls[0]) + ")";
    std::string mid = "c val ( " + std::to_string(values[0]) + ")";
    std::string bot = "c why ( " + std::string(is_ds[0] ? "d" : "f") + ")";

    int old_l = -1;
    for (int var : stack) {
        int v = values[var];
        int l = dls[var];
        bool d = is_ds[var];

        if (l > old_l) {
            top += " | " + std::to_string(l) + " ";
            mid += " | " + std::to_string(v * var) + " ";
            bot += " | " + std::string((d ? "d" : "f")) + " ";
            old_l = l;
        } else {
            mid += std::to_string(v * var) + " ";
            bot += std::string((d ? "d" : "f")) + " ";
        }
        size_t m = std::max(top.size(), std::max(mid.size(), bot.size()));
        while (top.size() < m) top += " ";
        while (mid.size() < m) mid += " ";
        while (bot.size() < m) bot += " ";
    }
    top += " |";
    mid += " |";
    bot += " |";

    return top + + "\n" + mid + "\n" + bot + "\n";
}