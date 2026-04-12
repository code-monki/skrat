# Orchestration layer: wraps CMake for configure + build + run.
#
# Variables (optional):
#   BUILD_DIR   build directory (default: build)
#   GENERATOR   CMake generator, e.g. "Ninja" (default: platform default)
#   CMAKE_ARGS  extra arguments passed to cmake -S . -B $(BUILD_DIR)

BUILD_DIR ?= build
CMAKE ?= cmake

CMAKE_FLAGS :=
ifneq ($(strip $(GENERATOR)),)
	CMAKE_FLAGS += -G "$(GENERATOR)"
endif

.PHONY: all help configure build clean run distclean

all: build

help:
	@echo "Targets:"
	@echo "  make / make build   Configure (if needed) and compile"
	@echo "  make configure      Run CMake configure only"
	@echo "  make run            Build then launch skrat"
	@echo "  make clean          Remove $(BUILD_DIR)/"
	@echo "  make distclean      Same as clean"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_DIR=$(BUILD_DIR)"
	@echo "  CMAKE_ARGS='...'    Extra -D / toolchain flags for configure"

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" $(CMAKE_FLAGS) $(CMAKE_ARGS)

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel

clean distclean:
	rm -rf "$(BUILD_DIR)"

run: build
	@if [ -x "$(BUILD_DIR)/skrat" ]; then \
		"$(BUILD_DIR)/skrat" "$(ARGS)"; \
	elif [ -x "$(BUILD_DIR)/Release/skrat" ]; then \
		"$(BUILD_DIR)/Release/skrat" "$(ARGS)"; \
	elif [ -x "$(BUILD_DIR)/Debug/skrat" ]; then \
		"$(BUILD_DIR)/Debug/skrat" "$(ARGS)"; \
	else \
		echo "Could not find built binary under $(BUILD_DIR)."; \
		echo "On Windows MSVC builds, run: $(BUILD_DIR)\\Release\\skrat.exe"; \
		exit 1; \
	fi
