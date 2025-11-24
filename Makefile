SRC_DIR=src
TEST_DIR=${SRC_DIR}/tests
BUILD_DIR=build
CMAKEFLAGS=-DCMAKE_TOOLCHAIN_FILE=$(OECORE_NATIVE_SYSROOT)/usr/share/cmake/OEToolchainConfig.cmake \
		-G "Ninja" \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

default: pitrac

.PHONY: pitrac
pitrac: messages
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(CMAKEFLAGS)
	cmake --build $(BUILD_DIR)

.PHONY: pitrac_debug
pitrac_debug: 
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(CMAKEFLAGS)
	cmake --build $(BUILD_DIR)

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
	@echo "  messages         - Generate C++ message classes from schemas"
	@echo "  python-messages  - Generate Python message classes for Flask"
	@echo "  all-messages     - Generate message types and all message classes"
	@echo "  clean-messages   - Remove all generated message files"
	@echo "  regen-messages   - Clean and regenerate all messages"
	@echo ""
	@echo "Testing:"
	@echo "  build_tests      - Build unit tests"
	@echo "  run_tests        - Run all unit tests"
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

.PHONY: messages
messages:
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
all-messages: message-types messages python-messages

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

.PHONY: all
all: pitrac build_tests

.PHONY: clean
clean:
	cmake -E remove_directory $(BUILD_DIR)
