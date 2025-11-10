#!/bin/bash

echo -ne "Correct Solutions: \r"
same=true
count=0
while [ same ]; do
    out=$(./test_one.sh)
    same=$([ "$out" = "eq" ])
    count=$((count + 1))
    echo -ne "Correct Solutions:" $count "\r"
done
