#!/usr/bin/env bash
# Builds every Dockerfile in this directory and reports PASS/FAIL per
# image. Desktop matrix images (gcc-*, clang-*) run the full ctest suite
# as part of `docker build`; embedded targets (esp32, stm32, riscv32)
# compile-check tests/noexcept_tests.cpp against the real cross-compiler
# (no link — see the comment at the top of each embedded Dockerfile for
# why). A Dockerfile's build succeeding IS the test passing.
#
# Usage:
#   docker/build-matrix.sh              # build + report on everything
#   docker/build-matrix.sh gcc-13 clang-17 esp32   # only these
#   docker/build-matrix.sh --list        # list available targets, do nothing
set -u -o pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "${script_dir}/.." && pwd)

mapfile -t all_targets < <(cd "${script_dir}" && ls *.Dockerfile | sed 's/\.Dockerfile$//' | sort -V)

if [[ "${1:-}" == "--list" ]]; then
    printf '%s\n' "${all_targets[@]}"
    exit 0
fi

targets=("$@")
if [[ ${#targets[@]} -eq 0 ]]; then
    targets=("${all_targets[@]}")
fi

declare -a results=()
overall_status=0

for name in "${targets[@]}"; do
    dockerfile="${script_dir}/${name}.Dockerfile"
    if [[ ! -f "${dockerfile}" ]]; then
        echo "skip: no such Dockerfile: docker/${name}.Dockerfile" >&2
        results+=("${name}: SKIP (not found)")
        overall_status=1
        continue
    fi

    echo "=== building ${name} ==="
    log_file=$(mktemp)
    if docker build -f "${dockerfile}" -t "ctut-${name}" "${repo_root}" >"${log_file}" 2>&1; then
        results+=("${name}: PASS")
    else
        results+=("${name}: FAIL (see ${log_file})")
        overall_status=1
        tail -n 60 "${log_file}"
    fi
done

echo
echo "=== summary ==="
printf '%s\n' "${results[@]}"

exit "${overall_status}"
