default: release

OUTPUT := MultiPLAY

SOURCES = MultiPLAY.cc bit_memory_stream.cc bit_value.cc unix_break_handler.cc channel.cc conversion.cc
HEADERS = MultiPLAY.h  bit_memory_stream.h  bit_value.h       break_handler.h  channel.h  conversion.h

SOURCES += effect.cc envelope.cc formatting.cc math.cc module.cc mod_finetune.cc notes.cc one_sample.cc
HEADERS += effect.h  envelope.h  formatting.h  math.h  module.h  mod_finetune.h  notes.h  one_sample.h

SOURCES += pattern.cc progress.cc sample.cc sample_builtintype.cc sample_instrument.cc wave_file.cc
HEADERS += pattern.h  progress.h  sample.h  sample_builtintype.h  sample_instrument.h  wave_file.h

SOURCES += Load_Sample.cc Load_MOD.cc Load_MTM.cc Load_S3M.cc Load_IT.cc Load_XM.cc Load_UMX.cc
HEADERS += Load_Sample.h  Load_MOD.h  Load_MTM.h  Load_S3M.h  Load_IT.h  Load_XM.h  Load_UMX.h

SOURCES += Channel_DYNAMIC.cc Channel_PLAY.cc Channel_MODULE.cc
HEADERS += Channel_DYNAMIC.h  Channel_PLAY.h  Channel_MODULE.h

SOURCES += DSP.cc
HEADERS += DSP.h

SOURCES += Output-DirectX.cc Output-SDL.cc
HEADERS += Output-DirectX.h  Output-SDL.h

SOURCES += uLaw-aLaw.cc
HEADERS += uLaw-aLaw.h

SOURCES += Profile.cc
HEADERS += Profile.h

HEADERS += RAII.h

OBJECT_DIR := obj
OBJECTS := $(patsubst %.cc,$(OBJECT_DIR)/%.o,$(SOURCES))

DEPS := $(patsubst %.o,%.d,$(OBJECTS))

-include $(DEPS)

WARNINGS := -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Woverloaded-virtual -Wredundant-decls -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wundef -Werror -Wno-unused -Wno-reorder -Wno-comment -Wno-missing-field-initializers

OPTIMIZE := -Ofast -march=native -frename-registers -funroll-loops -fno-signed-zeros -fno-trapping-math
DEBUG := -g
SDL_CXX := -DSDL -DSDL_DEFAULT
SDL_LD := -lSDL2

$(OUTPUT): $(OBJECTS)
	g++ -o $@ $^ $(LDFLAGS)

obj/%.o: %.cc
	@mkdir -p $(dir $@)
	g++ -o $@ $< -c -MMD $(CXXFLAGS)

release: CXXFLAGS=$(SDL_CXX) $(OPTIMIZE)
release: LDFLAGS=$(SDL_LD)
release: $(OUTPUT)

debug: CXXFLAGS=$(SDL_CXX) $(DEBUG)
debug: LDFLAGS=$(SDL_LD)
debug: $(OUTPUT)

lint: CXXFLAGS=$(SDL_CXX) $(DEBUG) $(WARNINGS)
lint: LDFLAGS=$(SDL_LD)
lint: $(OUTPUT)

bare: CXXFLAGS=
bare: LDFLAGS=
bare: $(OUTPUT)

clean:
	rm -f $(OUTPUT)
	rm -f $(OBJECTS) $(DEPS)
	[ -d $(OBJECT_DIR) ] && rmdir --ignore-fail-on-non-empty $(OBJECT_DIR) || true

