#!/usr/bin/env bash
# Production build and automated checks; never stages or launches an emulator.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$project_dir"
test_python=${FRONTEND_TEST_PYTHON:-python3}
"$test_python" -c 'import PIL; print("Image fixture dependency: Pillow", PIL.__version__)'
make
tools/host-check.sh
"$test_python" -m unittest discover -s tests -p 'test_*.py'

"$test_python" tools/write_build_info.py
