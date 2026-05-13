#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

mkdir -p "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR"

cd "$BUILD_DIR"

positive_tests=(
  01_return_literal
  02_arithmetic
  03_if_else
  04_for
  05_ternary_true
  06_ternary_false
  07_nested_ternary
  10_short_circuit
  11_function_call
  12_sum_primes
  test
)

for name in "${positive_tests[@]}"; do
  ./mini_cc "../tests/${name}.mc" -o "${name}.o" --emit-ir "${name}.ll"
  clang ../runtime/main.c "${name}.o" -o "${name}.app"
  printf "%s: " "$name"
  "./${name}.app"
done

set +e
./mini_cc ../tests/08_type_error.mc -o bad_type.o --emit-ir bad_type.ll >/tmp/mini_cc_bad_type.out 2>&1
bad_type_status=$?
./mini_cc ../tests/09_undeclared_variable.mc -o bad_name.o --emit-ir bad_name.ll >/tmp/mini_cc_bad_name.out 2>&1
bad_name_status=$?
set -e

cat /tmp/mini_cc_bad_type.out
cat /tmp/mini_cc_bad_name.out

if [[ $bad_type_status -eq 0 || $bad_name_status -eq 0 ]]; then
  echo "Expected semantic tests to fail" >&2
  exit 1
fi
