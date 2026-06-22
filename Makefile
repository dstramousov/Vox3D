BUILD_DIR ?= build
RELEASE_BUILD_DIR ?= build-release
DEBUG_BUILD_DIR ?= build
DEPS_DIR ?= .deps
APP ?= vox3d
ARGS ?=

.PHONY: all debug release configure-debug configure-release run clean cmake-clean distclean depsclean

all: debug

debug: configure-debug
	cmake --build $(DEBUG_BUILD_DIR) -j

release: configure-release
	cmake --build $(RELEASE_BUILD_DIR) -j

configure-debug:
	cmake -S . -B $(DEBUG_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DVOX3D_DEPS_DIR=$(DEPS_DIR)

configure-release:
	cmake -S . -B $(RELEASE_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DVOX3D_DEPS_DIR=$(DEPS_DIR)

run: debug
	./$(DEBUG_BUILD_DIR)/$(APP) $(ARGS)

clean:
	@for dir in "$(DEBUG_BUILD_DIR)" "$(RELEASE_BUILD_DIR)"; do \
		if [ -d "$$dir" ]; then \
			rm -f "$$dir/$(APP)" "$$dir/$(APP).exe"; \
			if [ -d "$$dir/CMakeFiles/$(APP).dir" ]; then \
				find "$$dir/CMakeFiles/$(APP).dir" -type f \
					\( -name '*.o' -o -name '*.o.d' -o -name '*.obj' -o -name '*.d' -o -name '*.gcda' -o -name '*.gcno' \) \
					-delete; \
			fi; \
		fi; \
	done

cmake-clean:
	@if [ -d "$(DEBUG_BUILD_DIR)" ]; then cmake --build $(DEBUG_BUILD_DIR) --target clean || true; fi
	@if [ -d "$(RELEASE_BUILD_DIR)" ]; then cmake --build $(RELEASE_BUILD_DIR) --target clean || true; fi

distclean:
	rm -rf $(DEBUG_BUILD_DIR) $(RELEASE_BUILD_DIR)

depsclean:
	rm -rf $(DEPS_DIR)
