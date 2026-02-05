#!/usr/bin/env bash

SCRIPT="./stats.sh"

declare -A sum_time sum_pct cur_time cur_pct

metrics=(
  check_found_model
  notify_assignment
  notify_backtrack
  cb_decide
  notify_new_decision_level
)

runs=0

while true; do
    bad_run=0
    zero_run=1

    for m in "${metrics[@]}"; do
        unset cur_time[$m]
        unset cur_pct[$m]
    done

    while read -r line; do
        if [[ $line =~ ^([a-z_]+):[[:space:]]*$ ]]; then
            bad_run=1
        fi

        if [[ $line =~ ^([a-z_]+):[[:space:]]+([0-9.]+)[[:space:]]+([0-9.]+) ]]; then
            name="${BASH_REMATCH[1]}"
            time="${BASH_REMATCH[2]}"
            pct="${BASH_REMATCH[3]}"

            cur_time[$name]=$time
            cur_pct[$name]=$pct

            if [[ $time != 0 || $pct != 0 ]]; then
                zero_run=0
            fi
        fi
    done < <("$SCRIPT")

    if (( bad_run )) || (( zero_run )); then
        continue
    fi

    ((runs++))

    for m in "${metrics[@]}"; do
        sum_time[$m]=$(awk "BEGIN {print ${sum_time[$m]:-0} + ${cur_time[$m]}}")
        sum_pct[$m]=$(awk "BEGIN {print ${sum_pct[$m]:-0} + ${cur_pct[$m]}}")
    done

    printf "\033[H\033[J"

    echo "--- running average after $runs runs ---"
    for m in "${metrics[@]}"; do
        avg_time=$(awk "BEGIN {print ${sum_time[$m]} / $runs}")
        avg_pct=$(awk "BEGIN {print ${sum_pct[$m]} / $runs}")
        printf "%s: %.2f %.2f\n" "$m" "$avg_time" "$avg_pct"
    done
done
