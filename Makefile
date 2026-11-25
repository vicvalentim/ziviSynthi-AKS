CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off
CPPFLAGS += -Iapp/core -Iplatform/smk37_pro
CORE = app/core/synth.c app/core/ks.c app/core/modules.c app/core/presets.c
HEADERS = app/core/synth.h app/core/ks.h app/core/modules.h app/core/presets.h app/core/matrix.def app/core/parameters.def app/core/scales.def app/core/dsp_real.h app/core/dsp_curve.h app/core/aks_calibration.h app/core/aks_calibration.def
CATALOG = $(wildcard app/core/*_presets.json) app/core/usb_effects.json app/core/parameters.def tools/build_presets.py
FACTORY = build/factory-presets.inc
FELUCCA_FACTORY = build/felucca-presets.inc
FLASH = platform/smk37_pro/flash_policy.c
WEB = $(wildcard platform/web/* platform/web/fonts/* platform/web/felucca-updater/*)

.PHONY: all host host-fm1 smk-pro-object web test test-fm1 test-browser serve
all: host web

host: build/zivisynthi-host

host-fm1: $(FACTORY)
	@test -n "$(FM1_BASE)" || { echo 'Informe FM1_BASE com o caminho do fm1-base auditado.'; exit 2; }
	python3 "$(FM1_BASE)/tools/build.py" platform/fm1_base --host

SMK_RATE = 44100
SMK_FLAGS = -DZIVI_MAX_RATE=$(SMK_RATE) -DZIVI_SMK_NATIVE
SMK_CORE = $(CORE) platform/smk37_pro/zivi_smk37.c
SMK_HEADERS = $(wildcard platform/smk37_pro/*.h)
ifneq ($(SMK37_SDK_INCLUDE),)
SMK_FLAGS += -DZIVI_SMK_CHECK_SDK -I"$(SMK37_SDK_INCLUDE)"
endif

smk-pro-object: $(FACTORY)
	@test -x "$(PI32V2)/bin/clang" || { echo 'Informe PI32V2 com o caminho do toolchain JieLi para Linux.'; exit 2; }
	mkdir -p build/smk37-pro
	for src in $(SMK_CORE); do \
		name=$$(basename "$$src" .c); \
		"$(PI32V2)/bin/clang" -target pi32v2 -mcpu=r3 -integrated-as -std=c11 -Wall -Wextra -Werror \
			-ffreestanding -O2 -ffp-contract=off $(CPPFLAGS) $(SMK_FLAGS) -isystem "$(PI32V2)/include" \
			-c "$$src" -o "build/smk37-pro/$$name.o" || exit; \
	done

.PHONY: smk-pro-link-check bench-smk-pro
smk-pro-link-check: smk-pro-object
	@test -f "$(JIELI_NEWLIB)/libm.a" || { echo 'Informe JIELI_NEWLIB com o diretório pi32v2-lib do AC79 SDK.'; exit 2; }
	"$(PI32V2)/bin/ld" -r -o build/smk37-pro/zivi-core-linked.o \
		build/smk37-pro/synth.o build/smk37-pro/ks.o build/smk37-pro/modules.o \
		build/smk37-pro/presets.o build/smk37-pro/zivi_smk37.o \
		--start-group "$(JIELI_NEWLIB)/libm.a" "$(JIELI_NEWLIB)/libc.a" "$(JIELI_NEWLIB)/libcompiler_rt.a" --end-group
	"$(PI32V2)/bin/nm" -u build/smk37-pro/zivi-core-linked.o > build/smk37-pro/unresolved.txt
	test ! -s build/smk37-pro/unresolved.txt

bench-smk-pro: build/bench-smk37
	./build/bench-smk37

build/bench-smk37: tools/bench_smk37.c $(SMK_CORE) $(HEADERS) $(FACTORY) $(SMK_HEADERS)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DZIVI_MAX_RATE=$(SMK_RATE) $(LDFLAGS) tools/bench_smk37.c $(SMK_CORE) -lm -o $@

build/zivisynthi-host: platform/host/main.c $(CORE) $(HEADERS) $(FACTORY)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) platform/host/main.c $(CORE) -lm -o $@

build/test-synth: tests/test_synth.c $(CORE) $(HEADERS) $(FACTORY) $(FLASH) platform/smk37_pro/flash_policy.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) tests/test_synth.c $(CORE) $(FLASH) -lm -o $@

$(FACTORY): $(CATALOG)
	python3 tools/build_presets.py --native $@

.PHONY: presets
build/zivisynthi-host-felucca: platform/host/main.c $(CORE) $(HEADERS) $(FELUCCA_FACTORY)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DZIVI_FELUCCA_CATALOG $(LDFLAGS) platform/host/main.c $(CORE) -lm -o $@

presets: build/zivisynthi-host-felucca
	python3 tools/build_presets.py --felucca --json-dir build/presets
	mkdir -p build/previews
	for patch in build/presets/*.json; do \
		key=$$(basename "$$patch" .json); ./build/zivisynthi-host-felucca --aks "build/previews/$$key.wav" 60 "$$key" || exit; \
	done

build/test-smk37: tests/test_smk37.c $(SMK_CORE) $(HEADERS) $(FACTORY) $(SMK_HEADERS)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DZIVI_MAX_RATE=$(SMK_RATE) $(LDFLAGS) tests/test_smk37.c $(SMK_CORE) -lm -o $@

web: build/web/index.html

.PHONY: web-release
web-release: web
	@test -f "$(FIRMWARE_ROLLBACK)" || { echo 'Informe FIRMWARE_ROLLBACK com o FWSC da 3.9.4.'; exit 2; }
	python3 tools/build_web.py build/web/release.html \
		--embed-firmware 3.9.5 build/smk37-felucca/zivisynthi-felucca-smk37.fwsc \
		--embed-firmware 3.9.4 "$(FIRMWARE_ROLLBACK)"

build/web/index.html: tools/build_web.py tools/build_vernier.py platform/module_panel.def platform/smk37_fm1_base/vernier.def app/core/matrix.def app/core/scales.def app/core/aks_calibration.def $(CATALOG) $(WEB)
	mkdir -p build/web
	python3 tools/build_web.py $@

serve: web
	python3 -m http.server 8000 --bind 127.0.0.1 --directory build/web

test: all presets build/test-synth build/test-smk37
	./build/test-synth
	./build/test-smk37
	./build/zivisynthi-host build/demo-native.wav
	./build/zivisynthi-host build/demo-filter.wav 69 filter
	./build/zivisynthi-host build/demo-ring.wav 69 ring
	./build/zivisynthi-host build/demo-spring.wav 69 spring
	./build/zivisynthi-host build/demo-resonance.wav 69 resonance
	./build/zivisynthi-host build/demo-repeat.wav 69 repeat
	node tests/test_studio.cjs
	node tests/test_web.cjs

test-fm1: host-fm1
	mkdir -p build/fm1-host
	platform/fm1_base/build/host/zivisynthi_host tests/fm1_host.txt build/fm1-host
	platform/fm1_base/build/host/zivisynthi_host tests/fm1_host_ks.txt build/fm1-host
	platform/fm1_base/build/host/zivisynthi_host tests/fm1_host_m4.txt build/fm1-host
	platform/fm1_base/build/host/zivisynthi_host tests/fm1_host_m5.txt build/fm1-host
	platform/fm1_base/build/host/zivisynthi_host tests/fm1_host_presets.txt build/fm1-host

test-browser: web
	node tests/test_browser.cjs

.PHONY: smk-pro-firmware
SMK_DSP_TABLES = build/smk37-dsp-tables.inc

$(SMK_DSP_TABLES): tools/build_dsp_tables.py
	python3 $< $@

smk-pro-firmware: $(FACTORY) $(SMK_DSP_TABLES)
	python3 tools/build_smk37.py --sloop "$(SLOOP)" --fm1-base "$(FM1_BASE)" --toolchain "$(JIELI_TOOLCHAIN)" --sdk "$(AC79_SDK)"

.PHONY: smk-open-firmware
smk-open-firmware: $(FACTORY) $(SMK_DSP_TABLES)
	@test -f "$(SMK37_OPEN_SDK)/sdk/manifest.json" || { echo 'Informe SMK37_OPEN_SDK com a pasta do SDK V01.'; exit 2; }
	python3 tools/build_smk37_open.py --open-sdk "$(SMK37_OPEN_SDK)" --toolchain "$(JIELI_TOOLCHAIN)" --libm "$(JIELI_NEWLIB)/libm.a"

.PHONY: smk-felucca-firmware test-smk-felucca
FELUCCA_VERNIER = build/felucca-vernier.inc

$(FELUCCA_VERNIER): tools/build_vernier.py platform/smk37_fm1_base/vernier.def
	mkdir -p build
	python3 tools/build_vernier.py $@
$(FELUCCA_FACTORY): $(CATALOG)
	python3 tools/build_presets.py --felucca --native $@

smk-felucca-firmware: $(FELUCCA_FACTORY) $(SMK_DSP_TABLES) $(FELUCCA_VERNIER)
	python3 tools/build_smk37_felucca.py --fm1-base "$(FM1_BASE)" --sdk "$(AC79_SDK)" --toolchain "$(JIELI_TOOLCHAIN)" $(if $(FELUCCA_SOURCE_KIT),--source-kit "$(FELUCCA_SOURCE_KIT)")

test-smk-felucca: $(FELUCCA_FACTORY) $(SMK_DSP_TABLES) $(FELUCCA_VERNIER)
	python3 tools/build_smk37_felucca.py --fm1-base "$(FM1_BASE)" --host
	mkdir -p build/felucca-host
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_felucca_master.c -o build/felucca-host/test-master
	build/felucca-host/test-master
	platform/smk37_fm1_base/build/host/zivisynthi-felucca-smk37_host tests/felucca_smk37.txt build/felucca-host
	platform/smk37_fm1_base/build/host/zivisynthi-felucca-smk37_host tests/felucca_pro_controls.txt build/felucca-host
	platform/smk37_fm1_base/build/host/zivisynthi-felucca-smk37_host tests/felucca_modules.txt build/felucca-host
	platform/smk37_fm1_base/build/host/zivisynthi-felucca-smk37_host tests/felucca_clock_scenes.txt build/felucca-host
	python3 tests/felucca_panel_map.py platform/smk37_fm1_base/build/host/zivisynthi-felucca-smk37_host build/felucca-host
	python3 tests/felucca_studio.py platform/smk37_fm1_base/build/host/zivisynthi-felucca-smk37_host build/felucca-host
	platform/smk37_fm1_base/build/host/zivisynthi-felucca-smk37_host tests/felucca_audio38.txt build/felucca-host

.PHONY: test-smk-pro-dsp
test-smk-pro-dsp: build/test-smk37-fp32
	./build/test-smk37-fp32

SMK_HOST_FP32_FLAGS ?= -fsingle-precision-constant

build/test-smk37-fp32: tests/test_smk37.c $(SMK_CORE) $(HEADERS) $(FACTORY) $(SMK_DSP_TABLES) $(SMK_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SMK_HOST_FP32_FLAGS) -DZIVI_MAX_RATE=$(SMK_RATE) -DZIVI_REAL32 $(LDFLAGS) tests/test_smk37.c $(SMK_CORE) -lm -o $@

.PHONY: test-smk-open
test-smk-open: build/test-smk-open $(FACTORY) $(SMK_DSP_TABLES)
	./build/test-smk-open
	python3 tests/test_open_display.py "$(SMK37_OPEN_SDK)" "$(CC)"

build/test-smk-open: tests/test_smk37.c $(SMK_CORE) $(HEADERS) $(FACTORY) $(SMK_DSP_TABLES) $(SMK_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SMK_OPEN_HOST_FLAGS) $(LDFLAGS) tests/test_smk37.c $(SMK_CORE) -lm -o $@

SMK_OPEN_HOST_FLAGS = $(SMK_HOST_FP32_FLAGS) -DZIVI_MAX_RATE=44118 -DZIVI_SMK_RENDER_RATE=44118 \
	-DZIVI_REAL32 -DZIVI_SMK_OPEN_AUDIO -DZIVI_SMK_AUDIO_SLOTS=9 -DZIVI_SMK_MIDI_PORTS=3

.PHONY: stress_aks
stress_aks: build/stress_aks
	./build/stress_aks

build/stress_aks build/open_preset_regression: build/%: tests/%.c $(SMK_CORE) $(HEADERS) $(FACTORY) $(SMK_DSP_TABLES) $(SMK_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SMK_OPEN_HOST_FLAGS) $(LDFLAGS) $< $(SMK_CORE) -lm -o $@
