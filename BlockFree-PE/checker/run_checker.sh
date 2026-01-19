#!/bin/bash

echo "clean all tmps"
rm -f tmp*

# paths
path_bcp_enum_out=./tmp_wbcp_terminal_out.txt

# default path for cnf
path_fuzzed_cnf=./tmp_fuzzed.cnf

fuzz=true
# projected=false
shrink=false

while getopts "c:ps" option; do
  case "$option" in
    c)
      path_fuzzed_cnf="$OPTARG"
      fuzz=false
      ;;
    p)
      projected=false
      echo "Script: Use cnf in projected form"
      ;;
    s)
      shrink=true
      echo "Script: Allow models to be shrunken"
      ;;
    *)
      echo "This is a script for running the checker"
      echo
      echo "USAGE: ./run_schecker.sh [-c <path_to_cnf>] [-p]"
      echo
      echo "-c <path_to_cnf>    Uses the given cnf"
      # echo "-p                  Project variables"
      echo "-s                  Allow shrunken models"
      echo
      echo "If no cnf is provided, a random cnf is fuzzed with cnfuzz (--tiny option is on)"
      # echo "If projected is true but the cnf is not in projected form, random literals are projected" 
      exit 1
      ;;
  esac
done

echo "Script: make all"
make -C ../

if $fuzz; then
    echo "Script: fuzz cnf into $path_fuzzed_cnf"
    ../../cnfuzz/cnfuzz --tiny > $path_fuzzed_cnf 
fi

if $projected; then

    first_line=$(head -n 1 "$path_fuzzed_cnf")

    if [[ $first_line =~ ^c\ ([0-9]+,)*[0-9]+$ ]]; then
        echo "Script: $path_fuzzed_cnf is already in projected form"
    else
        x=$(grep -E "^p cnf [0-9]+ [0-9]+" "$path_fuzzed_cnf" | awk '{print $3}' | head -n1)

        if [ -z "$x" ]; then
            echo "No 'p cnf X Y' line found."
            exit 1
        fi

        newline="c "$(shuf -i 1-"$x" -n $(( RANDOM % $x + 1 )) | paste -sd,)

        tmpfile=$(mktemp)
        {
            echo "$newline"
            cat "$path_fuzzed_cnf"
        } > "$tmpfile"

        if ! $fuzz; then
          path_fuzzed_cnf=./tmp_projected_fuzzed.cnf
        fi
        mv "$tmpfile" "$path_fuzzed_cnf"
    echo "Script: added $newline to $path_fuzzed_cnf"
    fi
fi

if $shrink; then
  echo "Script: run wbcp_enum -s"
  ../src/wbcp_enum -s $path_fuzzed_cnf
else
  echo "Script: run wbcp_enum"
  ../src/wbcp_enum $path_fuzzed_cnf
fi

if $shrink; then
  echo "Script: run checker -s"
  ./checker -s $path_fuzzed_cnf $path_bcp_enum_out
else
  echo "Script: run checker"
  ./checker $path_fuzzed_cnf $path_bcp_enum_out
fi
