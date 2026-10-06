#!/bin/bash
# Harness entry point for the export-macro discovery test. The module comes
# from the CLI generator rather than a checked-in binding .cpp, so the
# generic binding-build step cannot create it.
set -e
exec bash "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/build_and_test.sh"
