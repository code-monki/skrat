# Orchestration layer: wraps CMake for configure + build + run.
#
# Variables (optional):
#   BUILD_DIR   build directory (default: build)
#   GENERATOR   CMake generator, e.g. "Ninja" (default: platform default)
#   NINJA       Full path to ninja (only used when GENERATOR=Ninja); passes
#               -DCMAKE_MAKE_PROGRAM=... so CMake need not find ninja on PATH
#   CMAKE_ARGS  extra arguments passed to cmake -S . -B $(BUILD_DIR)
#
# Per-machine defaults (Qt path, cmake binary, Ninja): copy config.local.mk.example
# to config.local.mk (gitignored) or pass variables on the command line.

BUILD_DIR ?= build
CMAKE ?= cmake

-include config.local.mk

CMAKE_FLAGS :=
ifneq ($(strip $(GENERATOR)),)
	CMAKE_FLAGS += -G "$(GENERATOR)"
endif
ifeq ($(GENERATOR),Ninja)
ifneq ($(strip $(NINJA)),)
	CMAKE_FLAGS += -DCMAKE_MAKE_PROGRAM="$(NINJA)"
endif
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
	@echo "  CMAKE=/path/cmake   CMake binary (default: cmake from PATH)"
	@echo "  GENERATOR=Ninja     Use Ninja; optional NINJA=/path/to/ninja if not on PATH"
	@echo "  CMAKE_ARGS='...'    Extra -D / toolchain flags for configure"
	@echo "  (optional) config.local.mk — copy from config.local.mk.example"

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" $(CMAKE_FLAGS) $(CMAKE_ARGS) || { \
		echo ""; \
		echo "CMake configure failed (see errors above)."; \
		echo "If Qt6 was not found, point CMake at your Qt kit, for example:"; \
		echo "  make CMAKE_ARGS=\"-DCMAKE_PREFIX_PATH=\$$HOME/Qt/6.9.3/macos\""; \
		echo "Or create ./config.local.mk from config.local.mk.example (see README)."; \
		exit 1; \
	}

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel

clean distclean:
	rm -rf "$(BUILD_DIR)"

run: build
	@if [ -d "$(BUILD_DIR)/skrat.app" ]; then \
		open "$(BUILD_DIR)/skrat.app" --args "$(ARGS)"; \
	elif [ -x "$(BUILD_DIR)/skrat" ]; then \
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
