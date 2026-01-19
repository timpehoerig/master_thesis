#include "../../cadical/src/cadical.hpp"
#include "../../cadical/src/tracer.hpp"
#include "../../cadical/src/internal.hpp"
#include "../../cadical/src/external.hpp"
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
bool SHRINK = false;
bool HELP = false;

// cnf must be global so EnumProp can use it
tcnf cnf;

// paths
const char* TERMINAL_OUT = "tmp_wbcp_terminal_out.txt";
const char* PROOF_OUT = "tmp_wbcp_proof.txt";


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


int check_literal(int v, int b, std::vector<int> T, std::vector<int> dls, std::vector<int> values, CaDiCaL::Internal *internal) {
    int internal_v = internal->external->e2i[v];
    if (VERBOSE) std::cout << "c checking literal: " + std::to_string(v) << " internal: " << std::to_string(internal_v) << std::endl;
    if (VERBOSE) std::cout << "c watched clauses:" << std::endl;
    for (auto watch : internal->watches(internal_v)) {
        if (VERBOSE) {
            std::cout << "c ";
            for (int i = 0; i < watch.size; i++) {
                std::cout << std::to_string(watch.clause->literals[i]) << " ";
            }
        }
        int il1 = watch.clause->literals[0]; // should always be internal_l but isn't
        int il2 = watch.clause->literals[1]; // second watched literal
        // other is now the other watched literal
        int other = il1;
        if (std::abs(il1) == internal_v) other = il2; 
        if (VERBOSE) std::cout << "also by: " << std::to_string(other) << std::endl;
        if (!(std::count(T.begin(), T.end(), std::abs(other)) > 0 && values[other] * other > 0)) {
            if (VERBOSE) std::cout << "c b = max(" << std::to_string(b) << "," << std::to_string(dls[v]) << ")" << std::endl;
            b = std::max(b, dls[v]);
        }
    }
    if (VERBOSE) std::cout << "c returning b = " << std::to_string(b) << std::endl;
    return b;
}


int implicant_shrinking(std::vector<int> T, std::vector<bool> is_ds, std::vector<int> dls, std::vector<int> values, CaDiCaL::Internal *internal) {
    std::vector<int> T_copy(T);
    if (VERBOSE) std::cout << "c starting implicant shrinking" << std::endl;
    int b = 0;
    while (T_copy.size()) {
        int v = T_copy.back();
        T_copy.pop_back();
        if (!is_ds[v]) {
            b = std::max(b, dls[v]);
            if (VERBOSE) std::cout << "c " << std::to_string(v * values[v]) << " is not a decision -> b = max(" << std::to_string(b) << "," << std::to_string(dls[v]) << ")" << std::endl;
        } else if (dls[v] > b) {
            b = check_literal(v, b, T_copy, dls, values, internal);
        } else if (dls[v] == 0 || dls[v] == b) {
            if (VERBOSE) std::cout << "c dl of " << std::to_string(v * values[v]) << " is " << std::to_string(dls[v]) << "(0 or b)" << std::endl;
            break;
        }
    }
    return b;
}


class EnumProp : public CaDiCaL::ExternalPropagator, public CaDiCaL::InternalTracer {

    public:

        CaDiCaL::Solver *solver;
        CaDiCaL::Internal *internal;

        // stack of assigned variables
        std::vector<int> stack;

        // arrays where index == var
        std::vector<int> dls;
        std::vector<bool> is_ds;
        std::vector<int> values;

        // decision count
        int dc = 0;
        // consecutive negative decisions count
        int cndc = 0;

        // decision level
        int dl = 0;

        // biggest variable (inclusive)
        int max_var;

        // flag if forced_backtrack was called, used for negative decisions
        bool backtrack = false;
        // flag if solver is doing stuff that is redundant and needs to be undone (with a backtrack)
        bool false_backtrack = false;

        // if we backtrack in cb_decide, the solver ignores the decision and we need to save it
        bool save_decision = false;
        // the saved decision
        int saved_decision = 0;

        // last decision (to decide wether an assignment is a decision or forced)
        int last_decision = 0;

        // decision before backtrack (to detect duplication)
        int decision_b4_backtracked = 0;

        // counting decisions on levels
        std::vector<int> decision_counts_per_level;

        tcnf all_models;

        void push(int lit, int dl, bool is_decision) {
            int var = std::abs(lit);

            stack.push_back(var);

            values[var] = (lit > 0) ? 1 : -1;
            dls[var] = dl;
            is_ds[var] = is_decision;
        };

        std::tuple<int, int, bool> pop() {
            int var = stack.back();
            stack.pop_back();

            int val = values[var];
            int level = dls[var];
            bool is_decision = is_ds[var];

            values[var] = 0;
            dls[var] = -1;
            is_ds[var] = false;

            return {val, level, is_decision};
        }

        int find_highest_dl_with_positive_decision() {
            int highest_dl = -1;
            for (int i = stack.size() - 1; i >= 0; i--) {
                int var = stack[i];
                if (is_ds[var] && values[var] > 0) {
                    highest_dl = dls[var];
                    break;
                }
            }
            if (VERBOSE) std::cout << "c highest dl with positive decision: " + std::to_string(highest_dl) << std::endl;
            return highest_dl;
        };

        bool cb_check_found_model (const tclause &model) override {

            if (VERBOSE) std::cout << "c cb_check_found_model:" << std::endl;

            int b = dl;
            bool found_model = false;
            if (!false_backtrack) {
                if (SHRINK) b = implicant_shrinking(stack, is_ds, dls, values, internal);
                tclause new_model;
                for (int lit : model) {
                    if (dls[std::abs(lit)] <= b) new_model.push_back(lit);
                }

                all_models.push_back(new_model);

                found_model = true;
                if (VERBOSE) std::cout << "c " + to_string(new_model) + "\nc" << std::endl;

            } else {
                if (VERBOSE) std::cout << "c ignoring model due to false backtrack\nc" << std::endl;
            }

            false_backtrack = false;

            if (found_model && b < dl) {
                found_model = false;
                solver->force_backtrack(b - 1);
            } else {
                // finding highest decision level with positive decision
                int highest_pos_dl = find_highest_dl_with_positive_decision();

                // backtrack to decisionlevel before that, so we can flip the decision
                solver->force_backtrack(highest_pos_dl - 1);
            }
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

            for (int lit : list) {
                if (lit == last_decision) {
                    push(lit, dl, true);
                    if (VERBOSE) std::cout << "c decided: " + std::to_string(lit) + "@" + std::to_string(dl) << std::endl;
                    if (decision_counts_per_level[dl - 1] > 2) {
                        false_backtrack = true;
                        if (VERBOSE) std::cout << "c decision was already made twice on that level, false_backtrack = true" << std::endl;
                    }
                } else {
                    push(lit, dl, false);
                    if (VERBOSE) std::cout << "c forced: " + std::to_string(lit) + "@" + std::to_string(dl) << std::endl;

                    if ((decision_b4_backtracked < 0 && abs(lit) == abs(decision_b4_backtracked))
                        || (decision_b4_backtracked > 0 && lit == decision_b4_backtracked)) {
                        false_backtrack = true;
                        if (VERBOSE) std::cout << "c forced assignment contradicts last decision before backtrack, false_backtrack = true" << std::endl;
                    }
                }
            }

            if (VERBOSE) std::cout << to_string(stack, values, dls, is_ds);
        };

        // the call of this function indicates to the user that on the trail a new decision level has started. The function does not report the actual decision that started this new level or the current decision level — it only reports that a decision happened and thus, the decision level is increased.
        void notify_new_decision_level () override {
            if (VERBOSE) std::cout << "c notify_new_decision_level:" << std::endl;
            dl++;
            if (VERBOSE) std::cout << "c " + std::to_string(dl) << std::endl;
            decision_counts_per_level.push_back(0);
        };

        // this function indicates that the solver backtracked to a lower decision level. Its single argument reports the new decision level. All assignments that were made above this target decision level must be considered as unassigned.
        void notify_backtrack (size_t new_level) override {
            if (VERBOSE) std::cout << "c notify_backtrack:" << std::endl;

            if (VERBOSE) std::cout << "c to level " + std::to_string(new_level) << std::endl;

            int pos_new_decsion_b4_backtracked = 0;

            while (stack.size() > 0 && dls[stack.back()] > (int)new_level) {
                int var = stack.back();
                auto [val, level, is_decision] = pop();
                if (is_decision) {
                    dc--;
                    last_decision = 0;
                    pos_new_decsion_b4_backtracked = val * var;
                }

                if (VERBOSE) std::cout << "c removed " + std::to_string(val * var) + "@" + std::to_string(level) << std::endl;
            }

            if (!false_backtrack && pos_new_decsion_b4_backtracked != 0) {
                decision_b4_backtracked = pos_new_decsion_b4_backtracked;
            }

            dl = new_level;


            if (VERBOSE) std::cout << to_string(stack, values, dls, is_ds);
        };

        // called before the solver makes a decision. Return your decision or 0 (solver makes one).
        int cb_decide () override {
            if (VERBOSE) std::cout << "c cb_decide:" << std::endl;

            if (save_decision) {
                if (VERBOSE) std::cout << "c found saved decision: " + std::to_string(saved_decision) << std::endl;
                save_decision = false;
                int lit = saved_decision;
                saved_decision = 0;

                if (values[std::abs(lit)] == -lit) {
                    false_backtrack = true;
                    if (VERBOSE) std::cout << "c saved decision already falsified, backtracking to avoid duplication" << std::endl;
                } else {
                    if (VERBOSE) std::cout << "c returning decision: " + std::to_string(lit) << std::endl;
                    return lit;
                }
            }

            if (false_backtrack) {
                int highest_pos_dl = find_highest_dl_with_positive_decision();

                if (highest_pos_dl < 0) {
                    if (VERBOSE) std::cout << "c no positive decisions to flip - finished" << std::endl;
                    solver->terminate();
                    return 0;
                }

                if (VERBOSE) std::cout << "c backtracking to: " + std::to_string(highest_pos_dl - 1) + " to avoid duplication" << std::endl;
                solver->force_backtrack(highest_pos_dl - 1);
                false_backtrack = false;
                backtrack = true;
                save_decision = true;
                if (VERBOSE) std::cout << "end false_backtrack" << std::endl;
            }

            // find next decision variable
            int var = 1;
            while (var <= max_var && values[var] != 0) {
                var++;
            }

            if (var > max_var) {
                if (VERBOSE) std::cout << "c all variables assigned" << std::endl;
                return 0;
            }

            int lit;
            if (backtrack) {
                backtrack = false;
                lit = -var;
                while ((int)decision_counts_per_level.size() > dl + 1) {
                    decision_counts_per_level.pop_back();
                }
                if (dc == cndc) cndc++;
            } else {
                lit = var;
            }

            dc++;
            if (decision_counts_per_level[dl] == 1 && lit > 0) {
                decision_counts_per_level[dl] = 0;
            }
            decision_counts_per_level[dl]++;

            if (VERBOSE) std::cout << "c decision counts: " << to_string(decision_counts_per_level) << std::endl;

            if (save_decision) {
                if (VERBOSE) std::cout << "c saved decision: " + std::to_string(lit) << std::endl;
                saved_decision = lit;
            }

            last_decision = lit;
            decision_b4_backtracked = lit;
            if (VERBOSE) std::cout << "c returning decision: " + std::to_string(lit) << std::endl;
            return lit;
        };

        int cb_propagate () override { return 0; };
        int cb_add_reason_clause_lit (int propagated_lit) override { return 0; };

        void connect_internal (CaDiCaL::Internal *internal_p) {
            internal = internal_p;
        };
};


void arg_parser(int argc, char* argv[], bool& count, bool& verbose, bool& shrink, bool& help) {
    std::map<std::string, std::string> parsedArgs = parseArgs(argc, argv);
    count = parsedArgs.count("count") || parsedArgs.count("c");
    verbose = parsedArgs.count("verbose") || parsedArgs.count("v");
    shrink = parsedArgs.count("shrink") || parsedArgs.count("s");
    help = parsedArgs.count("help") || parsedArgs.count("h");
}


int main(int argc, char* argv[]) {
    // arg parser
    arg_parser(argc, argv, COUNT, VERBOSE, SHRINK, HELP);

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
            "\t-v --verbose \t Returns the log and all models\n"
            "\t-s --shrink \t Performs implicant shrinking on found models\n";
        std::cout << msg;
        return 0;
    }

    if (VERBOSE) {
        std::cout << "c Runnning the solver with the following options:" << std::endl;
        if (COUNT) std::cout << "c \tCOUNT" << std::endl;
        if (VERBOSE) std::cout << "c \tVERBOSE" << std::endl;
        if (SHRINK) std::cout << "c \tSHRINK" << std::endl;
    }

    // create a new solver instance
    CaDiCaL::Solver *solver = new CaDiCaL::Solver;

    // setting options for chronological backtracking
    // how to disable preprocessing?
    solver->set("chronoalways", true);
    solver->set("restart", false);
    solver->set("inprocessing", false);
    solver->set("rephase", false);
    solver->set("log", true);


    // create a new EnumProp instance
    EnumProp *ep = new EnumProp;

    ep->solver = solver;

    // connect EnumProp to solver instance
    solver->connect_external_propagator(ep);

    // connect tracer and delete proof (so maybe no overhead) only need pointer to watches
    solver->connect_proof_tracer(ep, false);
    solver->disconnect_proof_tracer(ep);
    delete ep->internal->proof;
    ep->internal->proof = nullptr;

    // extract num of variables from dimacs file
    int numVariables = 0;

    solver->read_dimacs(argv[argc - 1], numVariables);

    // add dummy at index 0 so indices match variable numbers
    ep->values.push_back(0);
    ep->dls.push_back(-1);
    ep->is_ds.push_back(false);
    ep->decision_counts_per_level.push_back(0);

    tclause vars;
    // mark all variables as relevant for observing
    for (int var = 1; var <= numVariables; var++) {
        solver->add_observed_var(var);

        // initialize all values, dls and is_ds
        ep->values.push_back(0);
        ep->dls.push_back(-1);
        ep->is_ds.push_back(false);
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
