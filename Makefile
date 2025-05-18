default: release

GENERATEDMAKEFILE := $(shell mktemp)

OUTPUT_BASE = MultiPLAY

SOURCES = MultiPLAY.cc bit_memory_stream.cc bit_value.cc unix_break_handler.cc channel.cc conversion.cc
SOURCES += effect.cc envelope.cc formatting.cc math.cc module.cc mod_finetune.cc notes.cc one_sample.cc
SOURCES += pattern.cc progress.cc sample.cc sample_builtintype.cc sample_instrument.cc wave_file.cc
SOURCES += Load_Sample.cc Load_MOD.cc Load_MTM.cc Load_S3M.cc Load_IT.cc Load_XM.cc Load_UMX.cc
SOURCES += Channel_DYNAMIC.cc Channel_PLAY.cc Channel_MODULE.cc
SOURCES += DSP.cc
SOURCES += Output-DirectX.cc Output-SDL.cc
SOURCES += uLaw-aLaw.cc
SOURCES += Profile.cc

WARNINGS := -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Woverloaded-virtual -Wredundant-decls -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wundef -Werror -Wno-unused -Wno-reorder -Wno-comment -Wno-missing-field-initializers

OPTIMIZE := -Ofast -march=native -frename-registers -funroll-loops -fno-signed-zeros -fno-trapping-math
DEBUG := -g
SDL_CXX := -DSDL -DSDL_DEFAULT
SDL_LD := -lSDL2

common-vars: FORCE
	@echo OBJECT_DIR=obj/\$$\(CONFIGURATION\) >> $(GENERATEDMAKEFILE)
	@echo OUTPUT_DIR=bin/\$$\(CONFIGURATION\) >> $(GENERATEDMAKEFILE)
	@echo OUTPUT_FILE=\$$\(OUTPUT_DIR\)/MultiPLAY-\$$\(CONFIGURATION\) >> $(GENERATEDMAKEFILE)
	@echo SOURCES=$(SOURCES) >> $(GENERATEDMAKEFILE)
	@echo OBJECTS="\$$(patsubst %.cc,\$$(OBJECT_DIR)/%.o,\$$(SOURCES))" >> $(GENERATEDMAKEFILE)
	@echo DEPS="\$$(patsubst %.o,%.d,\$$(OBJECTS))" >> $(GENERATEDMAKEFILE)
	@echo >> $(GENERATEDMAKEFILE)
	@echo -include \$$\(DEPS\) >> $(GENERATEDMAKEFILE)
	@echo >> $(GENERATEDMAKEFILE)

release: release-vars common-vars build removegeneratedmakefile

release-vars: FORCE
	@echo CXXFLAGS=$(SDL_CXX) $(OPTIMIZE) >> $(GENERATEDMAKEFILE)
	@echo LDFLAGS=$(SDL_LD) >> $(GENERATEDMAKEFILE)
	@echo CONFIGURATION=release >> $(GENERATEDMAKEFILE)

debug: debug-vars common-vars build removegeneratedmakefile

debug-vars: FORCE
	@echo CXXFLAGS=$(SDL_CXX) $(DEBUG) >> $(GENERATEDMAKEFILE)
	@echo LDFLAGS=$(SDL_LD) >> $(GENERATEDMAKEFILE)
	@echo CONFIGURATION=debug >> $(GENERATEDMAKEFILE)

lint: lint-vars common-vars build removegeneratedmakefile

lint-vars: FORCE
	@echo CXXFLAGS=$(SDL_CXX) $(DEBUG) $(WARNINGS) >> $(GENERATEDMAKEFILE)
	@echo LDFLAGS=$(SDL_LD) >> $(GENERATEDMAKEFILE)
	@echo CONFIGURATION=lint >> $(GENERATEDMAKEFILE)

bare: bare-vars common-vars build removegeneratedmakefile

bare-vars: FORCE
	@echo CXXFLAGS= >> $(GENERATEDMAKEFILE)
	@echo LDFLAGS= >> $(GENERATEDMAKEFILE)
	@echo CONFIGURATION=bare >> $(GENERATEDMAKEFILE)

build: FORCE
	@echo build: \$$\(OUTPUT_FILE\) >> $(GENERATEDMAKEFILE)
	@echo >> $(GENERATEDMAKEFILE)
	@echo \$$\(OUTPUT_FILE\): \$$\(OBJECTS\) >> $(GENERATEDMAKEFILE)
	@/usr/bin/echo -en \\t >> $(GENERATEDMAKEFILE) ; echo @mkdir -p \$$\(OUTPUT_DIR\) >> $(GENERATEDMAKEFILE)
	@/usr/bin/echo -en \\t >> $(GENERATEDMAKEFILE) ; echo g++ -o \$$@ \$$^ \$$\(LDFLAGS\) >> $(GENERATEDMAKEFILE)
	@echo >> $(GENERATEDMAKEFILE)
	@echo obj/\$$\(CONFIGURATION\)/%.o: %.cc >> $(GENERATEDMAKEFILE)
	@/usr/bin/echo -en \\t >> $(GENERATEDMAKEFILE) ; echo @mkdir -p \$$\(dir \$$@\) >> $(GENERATEDMAKEFILE)
	@/usr/bin/echo -en \\t >> $(GENERATEDMAKEFILE) ; echo g++ -o \$$@ \$$\< -c -MMD \$$\(CXXFLAGS\) >> $(GENERATEDMAKEFILE)
	@echo >> $(GENERATEDMAKEFILE)
	@echo FORCE: >> $(GENERATEDMAKEFILE)
	@echo >> $(GENERATEDMAKEFILE)
	@echo .PHONY: FORCE >> $(GENERATEDMAKEFILE)
	@make -f $(GENERATEDMAKEFILE) build

clean: FORCE
	rm -rf obj/ bin/

FORCE:

removegeneratedmakefile: FORCE
	@[ ! -z $(GENERATEDMAKEFILE) -a -f $(GENERATEDMAKEFILE) ] && rm -r $(GENERATEDMAKEFILE)

.PHONY: default common-vars release release-vars debug debug-vars lint lint-vars bare bare-vars clean bare-bars build removegeneratedmakefile FORCE