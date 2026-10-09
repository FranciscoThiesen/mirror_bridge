#!/bin/bash
# Harness entry point: the modules come from the CLI generator rather than a
# checked-in binding .cpp, so the generic binding-build step cannot make them.
set -e
exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/build_and_test.sh"
