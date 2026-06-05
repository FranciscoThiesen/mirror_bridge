#!/bin/bash
# Harness entry point for the inheritance test. The inherit_mod module is
# produced by the CLI generator (not a checked-in binding .cpp), so the
# generic binding-build step can't create it; build_and_test.sh does the
# generate + test cycle.
set -e
exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/build_and_test.sh"
