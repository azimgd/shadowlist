# One entry point at the repo root. JS tasks run in the yarn workspace root, packages/.
# Native tasks run here.

YARN_ROOT := packages
YARN := cd $(YARN_ROOT) && yarn

CORE_TESTS := packages/shadowlist-core-tests

.PHONY: install lint lint-fix typecheck test test-wasm build build-wasm clean format format-check docs-check check test-core format-native format-native-check lint-core

# JS pass-through targets

install:
	$(YARN) install

lint:
	$(YARN) lint

lint-fix:
	$(YARN) lint:fix

typecheck:
	$(YARN) typecheck

test:
	$(YARN) test

test-wasm:
	$(YARN) test:wasm

build:
	$(YARN) build

build-wasm:
	$(YARN) build:wasm

clean:
	$(YARN) clean

format:
	$(YARN) format

format-check:
	$(YARN) format:check

docs-check:
	$(YARN) docs:check

check: lint format-check typecheck docs-check test

# Native targets

# clang-format for C++, Objective-C++ and Java, swiftformat and swiftlint for Swift, ktlint for
# Kotlin. The configs are .clang-format, .swiftformat, .swiftlint.yml and .editorconfig.
format-native:
	packages/scripts/native-format.sh

format-native-check:
	packages/scripts/native-format.sh --check

test-core:
	cmake -S $(CORE_TESTS) -B $(CORE_TESTS)/build
	cmake --build $(CORE_TESTS)/build
	ctest --test-dir $(CORE_TESTS)/build --output-on-failure

# clang-tidy over the core, with the checks in .clang-tidy. It reads the compile database the
# core tests' CMake project writes. On macOS the tools come from `brew install llvm`.
lint-core:
	cmake -S $(CORE_TESTS) -B $(CORE_TESTS)/build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	PATH="$$PATH:/opt/homebrew/opt/llvm/bin:/usr/local/opt/llvm/bin" run-clang-tidy -quiet -p $(CORE_TESTS)/build '/shadowlist-core(-include/shadowlist-core)?/.*\.cpp$$'
