MultiPLAY: MultiPLAY.cc bit_memory_stream.h Channel_DYNAMIC.h Channel_MODULE.h Channel_NIHONGOBANGOO.h Channel_PLAY.h DSP.h Load_IT.h Load_MOD.h Load_MTM.h Load_S3M.h Load_Sample.h Load_XM.h one_sample.h Output-DirectX.h Output-SDL.h RAII.h uLaw-aLaw.h unix_break_handler.h win32_break_handler.h
	g++ -o MultiPLAY MultiPLAY.cc -DSDL -DSDL_DEFAULT -lSDL2
