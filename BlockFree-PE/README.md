# Enumeration with Projection

## IMPORTANT

This is not a finished Version.

To make this work following changes must be done in cadical:

In cadical/src/external_propagate.cpp:

1. In 'ask_decision' add at the end 'else return ask_decision()'

2. In 'ask_decision' add 'forced_backt_allowed = true' before 'int elit = external->propagator->cb_decide ();' and 'forced_backt_allowed = false' after.

## USAGE:

Run `make` on this level to compile all (wbcp_enum and checker), run `make clean` to clean up all files produced by `make` and the execution of either.

## USAGE: wbcp_enum:

Move to `src`.

Run `./wbcp_enum --help`.
