#!/bin/bash

echo -ne "Correct Solutions: \r"
count=0
while true; do
    out=$(./test_one.sh | tail -n 1)
    if [[ $out == neq* ]]; then
        echo $out
        break
    fi
    x=${out#* }
    if [[ "$x" -gt 0 ]]; then
        count=$((count + 1))
        echo -ne "Correct Solutions:" $count "\r"
    fi
done
echo
