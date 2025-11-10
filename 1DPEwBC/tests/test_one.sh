#!/bin/bash

path_bcp_terminal_out=./tmp_bcp_terminal_out.txt

path_fuzzed_cnf=./tmp/tmp_fuzzed.cnf
path_out_bc=./tmp/tmp_terminal_out.txt
path_out_dualiza=./tmp/tmp_out_dualiza.txt

mkdir -p ./tmp/

echo "fuzz > $path_fuzzed_cnf"
../../cnfuzz/cnfuzz --tiny > $path_fuzzed_cnf

echo "bcp_enum > $path_out_bc"
# running bcp_enum with --minimize may lead to a different count of models
../src/bcp_enum -c $path_fuzzed_cnf > $path_out_bc

mv $path_bcp_terminal_out ./tmp/

echo "dualiza > $path_out_dualiza"
../../dualiza/dualiza $path_fuzzed_cnf > $path_out_dualiza

num_bc=$(grep -v '^[[:space:]]*$' $path_out_bc | tail -n 1)
num_dualiza=$(grep -v '^[[:space:]]*$' $path_out_dualiza | tail -n 1)

if [ "$num_bc" = "$num_dualiza" ]; then
    echo "eq"
else
    echo "neq"
fi
