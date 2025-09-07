SRC_DIR=src
TEST_DIR=${SRC_DIR}/tests
BUILD_DIR=build
CMAKEFLAGS=-DCMAKE_TOOLCHAIN_FILE=$(OECORE_NATIVE_SYSROOT)/usr/share/cmake/OEToolchainConfig.cmake \
		-G "Ninja" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

default: pitrac

.PHONY: pitrac
pitrac: 
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(CMAKEFLAGS)
	cmake --build $(BUILD_DIR)

.PHONY: pitrac_debug
pitrac_debug: 
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(CMAKEFLAGS)
	cmake --build $(BUILD_DIR)

.PHONY: build_tests
build_tests: pitrac
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(CMAKEFLAGS) -DBUILD_TESTS=ON
	cmake --build $(BUILD_DIR)

.PHONY: run_tests
run_tests: build_tests
	@echo "Running individual tests with QEMU..."
	@find $(BUILD_DIR)/testbin/unit -name "test_*" -type f -executable | while read test; do \
		echo "=== Running $$test ==="; \
		qemu-aarch64 -L $(OECORE_TARGET_SYSROOT) "$$test" || echo "Test failed: $$test"; \
		echo ""; \
	done

.PHONY: all
all: pitrac build_tests

.PHONY: clean
clean:
	cmake -E remove_directory $(BUILD_DIR)

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  pitrac       - Build the main application"
	@echo "  pitrac_debug - Build the main application in debug mode"
	@echo "  build_tests  - Build all unit tests"
	@echo "  run_tests    - Build and run all tests"
	@echo "  test_colorsys- Build and run only colorsys tests"
	@echo "  all          - Build application and tests"
	@echo "  clean        - Clean build directory"
	@echo "  help         - Show this help message"
