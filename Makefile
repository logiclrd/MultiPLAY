OUTPUT = MultiPLAY

SOURCES = MultiPLAY.cc bit_memory_stream.cc bit_value.cc unix_break_handler.cc channel.cc conversion.cc
HEADERS = MultiPLAY.h  bit_memory_stream.h  bit_value.h       break_handler.h  channel.h  conversion.h

SOURCES += effect.cc envelope.cc math.cc module.cc mod_finetune.cc notes.cc one_sample.cc pattern.cc progress.cc
HEADERS += effect.h  envelope.h  math.h  module.h  mod_finetune.h  notes.h  one_sample.h  pattern.h  progress.h

SOURCES += sample.cc sample_builtintype.cc sample_instrument.cc wave_file.cc
HEADERS += sample.h  sample_builtintype.h  sample_instrument.h  wave_file.h

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

WARNINGS = -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Woverloaded-virtual -Wredundant-decls -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wundef -Werror -Wno-unused -Wno-reorder -Wno-comment

OPTIMIZE = -Ofast -march=native -frename-registers -funroll-loops -fno-signed-zeros -fno-trapping-math

MultiPLAY: $(SOURCES) $(HEADERS)
	g++ -o $(OUTPUT) $(SOURCES) -DSDL -DSDL_DEFAULT -lSDL2 $(OPTIMIZE)

debug: $(SOURCES) $(HEADERS)
	g++ -o $(OUTPUT) $(SOURCES) -DSDL -DSDL_DEFAULT -lSDL2 -g $(WARNINGS)

bare: $(SOURCES) $(HEADERS)
	g++ -o $(OUTPUT) $(SOURCES) $(WARNINGS)

clean:
	rm MultiPLAY
