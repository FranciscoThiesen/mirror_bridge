#!/bin/bash
# Harness entry point: the geom module is produced by the CLI generator (the
# template plan is the thing under test), so the generic binding-build step
# can't create it; build_and_test.sh does the generate + test cycle.
set -e
exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/build_and_test.sh"
