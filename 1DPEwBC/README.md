# Blocked Clause Enumeration with Projection

## USAGE:

Run `make` on this level to compile all (wbcp_enum and checker), run `make clean` to clean up all files produced by `make` and the execution of either.

## USAGE: wbcp_enum:

Move to `src`.

Run `./wbcp_enum --help`.

## USAGE: checker:

Move to `checker`.

Run `./checker --help` to see how to use `checker` directly or

Run `./run_checker.sh --help` to run both `wbcp_enum` and directly `checker` on it's output.

## tests:

Move to `checker`.

Run `./test_one.sh` to run wbcp_enum in counting mode and comparing it to dualiza's count. Outputs `eq` if equal else `neq`.

Run `./test_many.sh` to run `test_one.sh` until one run returns `neq`.

NOTE: `neq` can also happen if the cnf is too large and wbcp_enum fails due to too much memory consumption.

## src
## checker
## tests
## utils
