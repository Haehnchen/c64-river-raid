PYTHON ?= python3
BUILD_DIR ?= .build
UBSAN_BUILD_DIR ?= .build/ubsan

.DEFAULT_GOAL := help
.PHONY: help doctor configure build run test check smoke check-standalone check-native-tools check-ubsan release

help:
	@echo 'make doctor            Check native tools and compiled assets'
	@echo 'make build             Build the native port'
	@echo 'make run               Start the native port'
	@echo 'make test              Run the complete native CTest suite'
	@echo 'make check             Run the complete native CTest suite'
	@echo 'make smoke             Run the native smoke check with SDL dummy drivers'
	@echo 'make check-standalone  Build, test, and run a product-only copy'
	@echo 'make check-native-tools  Check doctor and standalone tooling'
	@echo 'make check-ubsan       Run native tests with undefined-behavior sanitizer'
	@echo 'make release           Package native release archives (RIVER_RAID_SDL_NOTICE required)'

doctor:
	$(PYTHON) tools/doctor.py

configure:
	cmake -S . -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

build: configure
	cmake --build "$(BUILD_DIR)" --parallel

run: build
	"$(BUILD_DIR)/river_raid"

test: build
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure

check: test

smoke: build
	env -u WAYLAND_DISPLAY SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy "$(BUILD_DIR)/river_raid" --smoke-test

check-standalone:
	$(PYTHON) tools/check_standalone.py

check-native-tools: doctor
	$(PYTHON) -B tools/test_doctor.py
	$(PYTHON) -B tools/test_check_standalone.py
	$(PYTHON) -B tools/test_package_release.py

release: build
	$(PYTHON) tools/package_release.py --build-dir "$(BUILD_DIR)"

check-ubsan:
	cmake -S . -B "$(UBSAN_BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON \
		-DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all" \
		-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined"
	cmake --build "$(UBSAN_BUILD_DIR)" --parallel
	ctest --test-dir "$(UBSAN_BUILD_DIR)" --output-on-failure
