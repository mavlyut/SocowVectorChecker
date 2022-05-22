#!/bin/bash
set -euo pipefail
IFS=$' \t\n'

cmake-build-$1/tests
