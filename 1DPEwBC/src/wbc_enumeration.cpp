#include "../../cadical/src/cadical.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include "../utils/argparser.hpp"
#include "../utils/strings.hpp"
#include "../utils/parser.hpp"
#include <unordered_set>


using tclause = std::vector<int>;
using tcnf = std::vector<tclause>;


// map dl -> d


// global meta variables
bool COUNT = false;
bool VERBOSE = false;
bool HELP = false;

// cnf must be global so EnumProp can use it
tcnf cnf;

// paths
const char* TERMINAL_OUT = "tmp_wbcp_terminal_out.txt";


// check sat_clause without index
bool sat_clause(tclause c, tclause model) {
    for (int lit : c) {
        if (std::find(model.begin(), model.end(), lit) != model.end()) {
            return true;
        }
    }
    return false;
}


bool sat(tcnf cnf, tclause model) {
    for (tclause c : cnf) {
        if (!sat_clause(c, model)) return false;
    }
    return true;
}


int get_next_decision(const std::vector<int>& v) {
    std::unordered_set<int> s;
    s.reserve(v.size() * 2);     // Performance-Boost?

    for (int x : v)
        s.insert(std::abs(x));

    int candidate = 1;
    while (s.count(candidate))
        ++candidate;
    return candidate;
}



class EnumProp : public CaDiCaL::ExternalPropagator {

    public:

        CaDiCaL::Solver *solver;

        std::vector<int> values;
        std::vector<int> dls;
        std::vector<bool> is_ds;

        // consecutive negative decisions count
        int cndc = 0;
        // decision count
        int dc = 0;
        // decision level
        int dl = 0;
        int max_var;
        bool backtrack = false;
        bool finished = false;
        bool false_backtrack = false;
        bool save_decision = false;
        int saved_decision = 0;

        tcnf all_models;

        void push(int lit, int dl, bool is_decision) {
            values.push_back(lit);
            dls.push_back(dl);
            is_ds.push_back(is_decision);
        };

        std::tuple<int, int, bool> pop() {
            int val = values.back();
            int level = dls.back();
            bool is_decision = is_ds.back();

            values.pop_back();
            dls.pop_back();
            is_ds.pop_back();

            return {val, level, is_decision};
        }

        bool cb_check_found_model (const tclause &model) override {
            if (VERBOSE) std::cout << "c cb_check_found_model:" << std::endl;
            if (finished) return false;

            if (!false_backtrack) {
                tclause new_model;
                // build new_model step by step
                for (int lit : model) {
                    new_model.push_back(lit);
                }

                all_models.push_back(new_model);

                if (VERBOSE) {
                    std::cout << "c " + to_string(model) + "\nc" << std::endl;
                }
            }

            // finding highest decision level with positive decision
            int index = values.size() - 1;
            while (true) {
                if (is_ds[index] && values[index] > 0) break;
                index--;
            }

            // backtrack to last decision level
            solver->force_backtrack(dls[index] - 1);
            backtrack = true;

            // always return false -> solver can only terminate with UNSAT: no more solutions
            return false;
        };

        // always false as we DO NOT want to add clauses
        bool cb_has_external_clause (bool &is_forgettable) override { return false; };

        int cb_add_external_clause_lit () override { return 0; };

        // this function is called when observed variables are assigned (either by BCP, Decide, or Learning a unit clause). It has a single read-only argument containing literals that became satisfied by the new assignment. In case the notification reports more than one literal, it is guaranteed that all of the reported literals were assigned on the same (current) decision level.
        void notify_assignment(const std::vector<int> &list) override {
            if (VERBOSE) std::cout << "c notify_assignment:" << std::endl;
            if (finished) return;
            for (int lit : list) {
                if (dls.back() < dl && dl > 0) {
                    push(lit, dl, true);
                    if (VERBOSE) std::cout << "c decided: " + std::to_string(lit) + "@" + std::to_string(dl) << std::endl;
                } else { // this happens if the literal is already on the stack (its a decision)
                    push(lit, dl, false);
                    if (VERBOSE) std::cout << "c forced: " + std::to_string(lit) + "@" + std::to_string(dl) << std::endl;
                };
            }
            if (VERBOSE) std::cout << to_string(values, dls, is_ds);
        };

        // the call of this function indicates to the user that on the trail a new decision level has started. The function does not report the actual decision that started this new level or the current decision level — it only reports that a decision happened and thus, the decision level is increased.
        void notify_new_decision_level () override {
            if (VERBOSE) std::cout << "c notify_new_decision_level:" << std::endl;
            dl++;
            if (VERBOSE) std::cout << "c " + std::to_string(dl) << std::endl;
        };

        // this function indicates that the solver backtracked to a lower decision level. Its single argument reports the new decision level. All assignments that were made above this target decision level must be considered as unassigned.
        void notify_backtrack (size_t new_level) override {
            if (VERBOSE) std::cout << "c notify_backtrack:" << std::endl;
            if (dc == cndc) {
                if (dl == -1) return;
                std::cout << "c\nc terminating search. " + std::to_string(cndc) + " decision(s), all negative" << std::endl;
                solver->terminate();
                finished = true;
                return;
            }

            if (VERBOSE) std::cout << "c to level " + std::to_string(new_level) << std::endl;
            false_backtrack = dl != dls.back();
            if (VERBOSE && false_backtrack) std::cout << "c false_backtrack = true" << std::endl;
            while (dls.back() > (int)new_level) {
                auto [val, level, is_decision] = pop();
                if (is_decision) {
                    dc--;
                }
                if (VERBOSE) std::cout << "c removed " + std::to_string(val) + "@" + std::to_string(level) << std::endl;
            }
            dl = new_level;
            if (VERBOSE) std::cout << to_string(values, dls, is_ds);
        };

        // called before the solver makes a decision. Return your decision or 0 (solver makes one).
        int cb_decide () override {
            if (VERBOSE) std::cout << "c cb_decide:" << std::endl;
            if (save_decision) {
                // check if its already assigned
                if (VERBOSE) std::cout << "c found saved decision: " + std::to_string(saved_decision) << std::endl;
                save_decision = false;
                int lit = saved_decision;
                saved_decision = 0;
                if (VERBOSE) std::cout << "c returning decision: " + std::to_string(lit) << std::endl;
                return lit;
            }

            if (false_backtrack) {
                int index = values.size() - 1;
                while (true) {
                    if (is_ds[index] && values[index] > 0) break;
                    index--;
                }
                if (VERBOSE) std::cout << "c backtracking to: " + std::to_string(dls[index] - 1) + " to avoid duplication" << std::endl;
                solver->force_backtrack(dls[index] - 1);
                false_backtrack = false;
                backtrack = true;
                save_decision = true;
                if (VERBOSE) std::cout << "end false_backtrack" << std::endl;
            }

            int var = get_next_decision(values);

            if (var > max_var) {
                if (VERBOSE) std::cout << "c all variables assigned" << std::endl;
                return 0;
            }

            int lit;
            if (backtrack) {
                backtrack = false;
                lit = -var;
                if (dc == cndc) cndc++;
            } else {
                lit = var;
            }

            dc++;

            if (save_decision) {
                if (VERBOSE) std::cout << "c saved decision: " + std::to_string(lit) << std::endl;
                saved_decision = lit;
            }

            if (VERBOSE) std::cout << "c returning decision: " + std::to_string(lit) << std::endl;
            return lit;
        };

        int cb_propagate () override { return 0; };
        int cb_add_reason_clause_lit (int propagated_lit) override { return 0; };
};


void arg_parser(int argc, char* argv[], bool& count, bool& verbose, bool& help) {
    std::map<std::string, std::string> parsedArgs = parseArgs(argc, argv);
    count = parsedArgs.count("count") || parsedArgs.count("c");
    verbose = parsedArgs.count("verbose") || parsedArgs.count("v");
    help = parsedArgs.count("help") || parsedArgs.count("h");
}


int main(int argc, char* argv[]) {
    // arg parser
    arg_parser(argc, argv, COUNT, VERBOSE, HELP);

    if (HELP) {
        std::string msg =
            "\n\n"
            "Welcome to Projected Enumeration using IPASIR-UP without Blocking Clauses\n"
            "\n"
            "USAGE:\n"
            "\twbcp_enum [--Option (-o)] <path to cnf>\n"
            "\n"
            "OPTIONS:\n"
            "\n"
            "\t-h --help \t Show this menu\n"
            "\t-c --count \t Returns number of models\n"
            "\t-v --verbose \t Returns the log and all models\n";
        std::cout << msg;
        return 0;
    }

    if (VERBOSE) {
        std::cout << "c Runnning the solver with the following options:" << std::endl;
        if (COUNT) std::cout << "c \tCOUNT" << std::endl;
        if (VERBOSE) std::cout << "c \tVERBOSE" << std::endl;    // generate proof
    }

    // create a new solver instance
    CaDiCaL::Solver *solver = new CaDiCaL::Solver;

    // setting options for chronological backtracking
    // how to disable preprocessing?
    solver->set("chronoalways", true);
    solver->set("restart", false);
    solver->set("inprocessing", false);
    solver->set("rephase", false);


    // create a new EnumProp instance
    EnumProp *ep = new EnumProp;

    
    ep->solver = solver;

    // connect EnumProp to solver instance
    solver->connect_external_propagator(ep);

    // extract num of variables from dimacs file
    int numVariables = 0;

    solver->read_dimacs(argv[argc - 1], numVariables);

    // push one 0 so index == var
    ep->push(0, -1, false);

    tclause vars;
    // mark all variables as relevant for observing
    for (int var = 1; var <= numVariables; var++) {
        solver->add_observed_var(var);
    }

    ep->max_var = numVariables;

    // run solver
    if (VERBOSE) std::cout << "c start solving\nc" << std::endl;
    int res = solver->solve();

    if (VERBOSE) {
        std::cout << "c all models: " + to_string(ep->all_models) << std::endl;

        std::cout << res;
        std::cout << "" << std::endl;
    }

    if (COUNT) {
        std::cout << "NUMBER SATISFYING ASSIGNMENTS" << std::endl;
        std::cout << ep->all_models.size();
        std::cout << "" << std::endl;
    }

    // write negated models to file
    std::ofstream file(TERMINAL_OUT);

    if (file.is_open()) {
        int i = 1;
        for (auto model : ep->all_models) {
            file << "i " << i << " " << to_string(model, true) << " 0" << std::endl;
            i++;
        }
        file.close();
    } else {
        std::cerr << "Unable to open " << TERMINAL_OUT << "." << std::endl;
    }

    // disconnect EnumProp
    solver->disconnect_external_propagator();

    // delete the solver instance
    delete solver;

    return 0;
}
