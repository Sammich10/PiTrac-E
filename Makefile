SRC_DIR=src
TEST_DIR=${SRC_DIR}/tests
BUILD_DIR=build
DEBUG_DIR=$(BUILD_DIR)/debug
CMAKEFLAGS=-DCMAKE_TOOLCHAIN_FILE=$(OECORE_NATIVE_SYSROOT)/usr/share/cmake/OEToolchainConfig.cmake \
		-G "Ninja" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
BUILD_TYPE ?= Debug

# IWYU variables
IWYU_MAPPING_FILE = tools/Formatting/iwyu.imp
IWYU_OUTPUT = build/iwyu_output.txt

default: pitrac_src

.PHONY: pitrac
pitrac: cpp-messages pitrac_src

.PHONY: pitrac_src
pitrac_src:
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(CMAKEFLAGS) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD_DIR)

.PHONY: pitrac_debug
pitrac_debug: BUILD_TYPE=Debug
pitrac_debug: pitrac_src
	@echo "Extracting debug symbols..."
	@mkdir -p $(DEBUG_DIR)
	@echo "Processing executables in $(BUILD_DIR)/bin..."
	@for binary in $(BUILD_DIR)/bin/*; do \
		if [ -f "$$binary" ] && [ -x "$$binary" ]; then \
			echo "  Extracting debug symbols from $$(basename $$binary)"; \
			$(OECORE_NATIVE_SYSROOT)/usr/bin/aarch64-pitrac-linux/aarch64-pitrac-linux-objcopy --only-keep-debug "$$binary" "$(DEBUG_DIR)/$$(basename $$binary).debug"; \
			$(OECORE_NATIVE_SYSROOT)/usr/bin/aarch64-pitrac-linux/aarch64-pitrac-linux-objcopy --strip-debug "$$binary"; \
			$(OECORE_NATIVE_SYSROOT)/usr/bin/aarch64-pitrac-linux/aarch64-pitrac-linux-objcopy --add-gnu-debuglink="$(DEBUG_DIR)/$$(basename $$binary).debug" "$$binary"; \
		fi; \
	done
	@echo "Processing shared libraries in $(BUILD_DIR)/lib..."
	@for library in $(BUILD_DIR)/lib/*.so*; do \
		if [ -f "$$library" ] && [ ! -L "$$library" ]; then \
			echo "  Extracting debug symbols from $$(basename $$library)"; \
			$(OECORE_NATIVE_SYSROOT)/usr/bin/aarch64-pitrac-linux/aarch64-pitrac-linux-objcopy --only-keep-debug "$$library" "$(DEBUG_DIR)/$$(basename $$library).debug"; \
			$(OECORE_NATIVE_SYSROOT)/usr/bin/aarch64-pitrac-linux/aarch64-pitrac-linux-objcopy --strip-debug "$$library"; \
			$(OECORE_NATIVE_SYSROOT)/usr/bin/aarch64-pitrac-linux/aarch64-pitrac-linux-objcopy --add-gnu-debuglink="$(DEBUG_DIR)/$$(basename $$library).debug" "$$library"; \
		fi; \
	done
	@echo "Debug symbols extracted to $(DEBUG_DIR)"
	@echo "Stripped binaries in $(BUILD_DIR)/bin and $(BUILD_DIR)/lib"

.PHONY: help
help:
	@echo "PiTrac Build System"
	@echo "==================="
	@echo "Main targets:"
	@echo "  pitrac           - Build the main application (default)"
	@echo "  pitrac_debug     - Build with debug configuration"
	@echo ""
	@echo "Message generation:"
	@echo "  message-types    - Generate message type enumerations"
	@echo "  cpp-messages         - Generate C++ message classes from schemas"
	@echo "  python-messages  - Generate Python message classes for Flask"
	@echo "  all-messages     - Generate message types and all message classes"
	@echo "  clean-messages   - Remove all generated message files"
	@echo "  regen-messages   - Clean and regenerate all messages"
	@echo ""
	@echo "Testing:"
	@echo "  build_tests      - Build unit tests"
	@echo "  run_tests        - Run all unit tests"
	@echo ""
	@echo "Code Quality:"
	@echo "  format           - Run all formatters (uncrustify + iwyu)"
	@echo "  uncrustify       - Format code with uncrustify"
	@echo "  iwyu             - Analyze includes (saves results to build/)"
	@echo "  iwyu-fix         - Analyze and automatically fix includes"
	@echo "  iwyu-check       - Check includes (including headers)"
	@echo ""
	@echo "Utilities:"
	@echo "  clean            - Clean build directory"
	@echo "  help             - Show this help message"
# Message generation variables
SCHEMAS_DIR = src/Infrastructure/Messaging/Messages/Schemas
GENERATED_MSG_DIR = src/Infrastructure/Messaging/Messages
CPP_MESSAGE_GENERATOR = tools/MessageGenerator/GenerateCppMessages.py
PYTHON_MESSAGE_GENERATOR = tools/MessageGenerator/GeneratePythonMessages.py
MESSAGE_TYPES_GENERATOR = tools/MessageGenerator/GenerateMessageTypes.py
FLASK_MESSAGES_DIR = PiTrac-Flask/app/messages

.PHONY: message-types
message-types:
	@echo "Generating message types from schemas..."
	/usr/bin/python3 $(MESSAGE_TYPES_GENERATOR) $(SCHEMAS_DIR) $(FLASK_MESSAGES_DIR)
	@echo "Message types generation complete!"

.PHONY: cpp-messages
cpp-messages:
	@echo "Generating unified message classes from all schemas..."
	@mkdir -p $(GENERATED_MSG_DIR)
	/usr/bin/python3 $(CPP_MESSAGE_GENERATOR) $(SCHEMAS_DIR) $(GENERATED_MSG_DIR)
	@echo "Unified message generation complete!"

.PHONY: python-messages
python-messages: message-types
	@echo "Generating Python message classes for External schemas only..."
	@mkdir -p $(FLASK_MESSAGES_DIR)
	/usr/bin/python3 $(PYTHON_MESSAGE_GENERATOR) $(SCHEMAS_DIR)/External $(FLASK_MESSAGES_DIR)/external
	/usr/bin/python3 $(PYTHON_MESSAGE_GENERATOR) $(SCHEMAS_DIR)/Common $(FLASK_MESSAGES_DIR)/common
	@echo "External and Common Python message generation complete!"

.PHONY: all-messages
all-messages: message-types cpp-messages python-messages

.PHONY: clean-messages
clean-messages:
	rm -rf $(GENERATED_MSG_DIR)/*.h
	rm -rf $(GENERATED_MSG_DIR)/*.cpp
	touch $(GENERATED_MSG_DIR)/.gitkeep
	rm -rf $(FLASK_MESSAGES_DIR)/external/*.py
	rm -rf $(FLASK_MESSAGES_DIR)/common/*.py
	rm -rf $(FLASK_MESSAGES_DIR)/message_types.py
	touch $(FLASK_MESSAGES_DIR)/external/.gitkeep
	touch $(FLASK_MESSAGES_DIR)/common/.gitkeep
	
.PHONY: regen-messages
regen-messages: clean-messages all-messages

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

.PHONY: format
format: uncrustify iwyu-fix

.PHONY: uncrustify
uncrustify:
	./tools/Formatting/uncrustify.sh --all --yes

.PHONY: iwyu
iwyu:
	@echo "Running include-what-you-use analysis..."
	@iwyu_tool -p $(BUILD_DIR) -- -Xiwyu --mapping_file=$(IWYU_MAPPING_FILE) -x c++ > $(IWYU_OUTPUT) 2>&1 || true
	@echo "IWYU analysis complete. Results saved to $(IWYU_OUTPUT)"
	@echo "Review the output and run 'make iwyu-fix' to apply fixes automatically."

.PHONY: iwyu-fix
iwyu-fix:
	@echo "Running IWYU and applying automatic fixes..."
	@iwyu_tool -p $(BUILD_DIR) -- -Xiwyu --mapping_file=$(IWYU_MAPPING_FILE) -x c++ | fix_include --comments --reorder
	@echo "Include fixes applied!"

.PHONY: iwyu-check
iwyu-check:
	@echo "Running IWYU in check-only mode (no fixes)..."
	@iwyu_tool -p $(BUILD_DIR) -- -Xiwyu --mapping_file=$(IWYU_MAPPING_FILE) -Xiwyu --check_also='*.h' -Xiwyu --check_also='*.hpp' -x c++

.PHONY: all
all: pitrac build_tests

.PHONY: clean
clean:
	cmake -E remove_directory $(BUILD_DIR)
