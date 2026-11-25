#!/usr/bin/env bash
# Formats and lints the native sources: clang-format for C++, Objective-C++ and Java,
# swiftformat and swiftlint for Swift, ktlint for Kotlin.
#
#   native-format.sh                     format every tracked native file
#   native-format.sh --check             fail when a file needs formatting or breaks a lint rule
#   native-format.sh [--check] <files>   the same for the given files (pre-commit)
#
# Paths are relative to the repo root. Symlinks are skipped: the AppKit kit links to UIKit
# sources, and a formatter writing through a link would replace it with a copy.
set -euo pipefail
cd "$(dirname "$0")/../.."

check=0
if [ "${1:-}" = "--check" ]; then
  check=1
  shift
fi

find_tool() {
  if command -v "$1" >/dev/null 2>&1; then
    command -v "$1"
  elif [ -x "/opt/homebrew/opt/llvm/bin/$1" ]; then
    echo "/opt/homebrew/opt/llvm/bin/$1"
  elif [ -x "/usr/local/opt/llvm/bin/$1" ]; then
    echo "/usr/local/opt/llvm/bin/$1"
  else
    echo "missing $1: install it with \`brew install $2\`" >&2
    exit 1
  fi
}

# Tracked regular files matching the pattern, limited to the requested files if any.
files=()
collect() {
  files=()
  local pattern="$1"
  shift
  while IFS= read -r line; do
    mode="${line%% *}"
    path="${line#*$'\t'}"
    if [ "$mode" != "120000" ] && [[ "$path" =~ $pattern ]] && [ -f "$path" ]; then
      files+=("$path")
    fi
  done < <(git ls-files -s -- "$@")
}

status=0

collect '\.(c|cc|cpp|h|hpp|m|mm|java)$' "$@"
if [ "${#files[@]}" -gt 0 ]; then
  clang_format="$(find_tool clang-format llvm)"
  if [ "$check" = 1 ]; then
    "$clang_format" --dry-run -Werror "${files[@]}" || status=1
  else
    "$clang_format" -i "${files[@]}"
  fi
fi

collect '\.swift$' "$@"
if [ "${#files[@]}" -gt 0 ]; then
  swiftformat="$(find_tool swiftformat swiftformat)"
  swiftlint="$(find_tool swiftlint swiftlint)"
  if [ "$check" = 1 ]; then
    "$swiftformat" --lint --quiet "${files[@]}" || status=1
  else
    "$swiftformat" --quiet "${files[@]}"
  fi
  "$swiftlint" lint --quiet --strict "${files[@]}" || status=1
fi

collect '\.kts?$' "$@"
if [ "${#files[@]}" -gt 0 ]; then
  ktlint="$(find_tool ktlint ktlint)"
  if [ "$check" = 1 ]; then
    "$ktlint" "${files[@]}" || status=1
  else
    "$ktlint" --format "${files[@]}" || status=1
  fi
fi

exit "$status"
