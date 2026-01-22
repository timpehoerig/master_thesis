#!/bin/bash

echo -ne "Correct Solutions: \r"
count=0
while true; do
    out=$(./test_one.sh | tail -n 2)
    echo $out
done
echo
