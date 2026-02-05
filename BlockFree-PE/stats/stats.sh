#!/bin/bash

tmp_wbcp_negated_models=./tmp_wbcp_negated_models.txt

path_fuzzed_cnf=./tmp/tmp_fuzzed.cnf
path_out_wbc=./tmp/tmp_terminal_out.txt

mkdir -p ./tmp/

echo "fuzz > $path_fuzzed_cnf"
../../cnfuzz/cnfuzz --tiny > $path_fuzzed_cnf

echo "wbcp_enum > $path_out_wbc"
../src/wbcp_enum -p -s $path_fuzzed_cnf > $path_out_wbc

mv $tmp_wbcp_negated_models ./tmp/

read check_found_model_time check_found_model_percent < <(
    awk '$NF=="check_found_model" {gsub(/%/,"",$3); print $2, $3}' "$path_out_wbc"
)

read notify_assignment_time notify_assignment_percent < <(
    awk '$NF=="notify_assignment" {gsub(/%/,"",$3); print $2, $3}' "$path_out_wbc"
)

read notify_backtrack_time notify_backtrack_percent < <(
    awk '$NF=="notify_backtrack" {gsub(/%/,"",$3); print $2, $3}' "$path_out_wbc"
)

read cb_decide_time cb_decide_percent < <(
    awk '$NF=="cb_decide" {gsub(/%/,"",$3); print $2, $3}' "$path_out_wbc"
)

read notify_new_decision_level_time notify_new_decision_level_percent < <(
    awk '$NF=="notify_new_decision_level" {gsub(/%/,"",$3); print $2, $3}' "$path_out_wbc"
)

echo "check_found_model: $check_found_model_time $check_found_model_percent"
echo "notify_assignment: $notify_assignment_time $notify_assignment_percent"
echo "notify_backtrack: $notify_backtrack_time $notify_backtrack_percent"
echo "cb_decide: $cb_decide_time $cb_decide_percent"
echo "notify_new_decision_level: $notify_new_decision_level_time $notify_new_decision_level_percent"
