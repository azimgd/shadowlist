# One entry point at the repo root. JS tasks run in the yarn workspace root, packages/.
# Native tasks run here.

YARN_ROOT := packages
YARN := cd $(YARN_ROOT) && yarn

CORE_TESTS := packages/shadowlist-core-tests

.PHONY: install lint lint-fix typecheck test test-wasm build build-wasm clean format format-check docs-check check test-core

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

test-core:
	cmake -S $(CORE_TESTS) -B $(CORE_TESTS)/build
	cmake --build $(CORE_TESTS)/build
	ctest --test-dir $(CORE_TESTS)/build --output-on-failure
