#include "Output-SDL.h"

#ifdef SDL

#ifdef WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

namespace MultiPLAY
{
	namespace
	{
		SDL_AudioSpec sdl_audio_spec;
		unsigned char *sdl_buffer;
		int sdl_buffer_size;
		SDL_cond *sdl_buffer_available_event, *sdl_finished_event;
		SDL_mutex *sdl_buffer_lock, *sdl_silly_lock;
		int sdl_play_cursor = -1, sdl_write_cursor = 0;
		bool sdl_playing = false, sdl_shutting_down = false;
		bool sdl_buffer_full = false;

		void sdl_callback(void *userdata, Uint8 *stream, int len);

		struct sdl_mutex_lock_release;

		struct sdl_mutex_lock
		{
			SDL_mutex *mutex;
			int capture_level;

			sdl_mutex_lock(SDL_mutex *m, bool capture = true);

		private:
			sdl_mutex_lock(sdl_mutex_lock &other); // unavailable
		public:

			sdl_mutex_lock_release temp_release();
			void release();
			void capture();
			~sdl_mutex_lock();
		};

		struct sdl_mutex_lock_release
		{
			sdl_mutex_lock *mutex_lock;

			sdl_mutex_lock_release(sdl_mutex_lock *ml)
				: mutex_lock(ml)
			{
				mutex_lock->release();
			}

			sdl_mutex_lock_release(sdl_mutex_lock_release &other)
			{
				mutex_lock = other.mutex_lock;
				mutex_lock->release();
			}

			~sdl_mutex_lock_release()
			{
				mutex_lock->capture();
			}
		};

		sdl_mutex_lock::sdl_mutex_lock(SDL_mutex *m, bool capture)
			: mutex(m), capture_level(0)
		{
			if (capture)
				this->capture();
		}

		sdl_mutex_lock::sdl_mutex_lock(sdl_mutex_lock &/*other*/) {} // private (unavailable)

		sdl_mutex_lock_release sdl_mutex_lock::temp_release()
		{
			sdl_mutex_lock_release temp(this);
			return temp;
		}

		void sdl_mutex_lock::release()
		{
			--capture_level;
			if (capture_level == 0)
				::SDL_mutexV(mutex);
		}

		void sdl_mutex_lock::capture()
		{
			capture_level++;
			if (capture_level == 1)
				::SDL_mutexP(mutex);
		}

		sdl_mutex_lock::~sdl_mutex_lock()
		{
			release();
		}

		void sdl_callback(void */*userdata*/, Uint8 *stream, int len)
		{
			int bytes_available = (sdl_write_cursor - sdl_play_cursor + sdl_buffer_size) % sdl_buffer_size;

			if (sdl_buffer_full && (bytes_available == 0))
				bytes_available = sdl_buffer_size;
			if ((bytes_available < len) && (!sdl_shutting_down))
			{
				wcerr << L"(SDL) Audio buffer underrun (sorry, decode couldn't keep up with the playback)" << endl;
				::SDL_PauseAudio(1);
				sdl_playing = false;
				::SDL_CondSignal(sdl_buffer_available_event);
			}
			else
			{
				if (bytes_available < len)
				{
					memset(&stream[bytes_available], 0, unsigned(len - bytes_available));
					len = bytes_available;
				}

				{
					sdl_mutex_lock lock(sdl_buffer_lock);

					sdl_play_cursor = (sdl_play_cursor + 1) % sdl_buffer_size;
					int remaining = sdl_buffer_size - sdl_play_cursor;
					if (len > remaining)
					{
						memcpy(stream, &sdl_buffer[sdl_play_cursor], unsigned(remaining));
						memcpy(stream + remaining, &sdl_buffer[0], unsigned(len - remaining));
					}
					else
						memcpy(stream, &sdl_buffer[sdl_play_cursor], unsigned(len));

					sdl_play_cursor = (sdl_play_cursor + len - 1) % sdl_buffer_size;
					sdl_buffer_full = false;
				} // ~sdl_mutex_lock

				::SDL_CondSignal(sdl_buffer_available_event);

				if (sdl_shutting_down && (sdl_play_cursor == sdl_write_cursor))
					::SDL_CondSignal(sdl_finished_event);
			}
		}

		unsigned char sdl_mini_buffer[128];
		int sdl_mini_buffer_position = 0, sdl_mini_buffer_start = 0;

		inline void emit_sdl_byte(unsigned char byte)
		{
			sdl_mini_buffer[sdl_mini_buffer_position++] = byte;
			sdl_mini_buffer_position &= 127;

			if (sdl_mini_buffer_position == sdl_mini_buffer_start)
			{
				sdl_mutex_lock lock(sdl_buffer_lock);

				if (((sdl_write_cursor - sdl_play_cursor + sdl_buffer_size) % sdl_buffer_size) == 0)
				{
					sdl_buffer_full = true;
					if (!sdl_playing)
					{
						::SDL_PauseAudio(0);
						sdl_playing = true;
					}

					{
						sdl_mutex_lock_release release(&lock);
						sdl_mutex_lock silly_lock(sdl_silly_lock);

						::SDL_CondWait(sdl_buffer_available_event, sdl_silly_lock);
					} // ~sdl_mutex_lock, ~sdl_mutex_lock_release
				}

				int buffer_free = (sdl_play_cursor - sdl_write_cursor + 1 + sdl_buffer_size) % sdl_buffer_size;

				if (buffer_free >= 128)
				{
					if (sdl_buffer_size - sdl_write_cursor >= 128)
					{
						if (sdl_mini_buffer_start == 0)
							memcpy(&sdl_buffer[sdl_write_cursor], &sdl_mini_buffer[0], 128);
						else
						{
							int first_half_bytes = 128 - sdl_mini_buffer_start;
							memcpy(&sdl_buffer[sdl_write_cursor], &sdl_mini_buffer[sdl_mini_buffer_start], unsigned(first_half_bytes));
							memcpy(&sdl_buffer[sdl_write_cursor + first_half_bytes], &sdl_mini_buffer[0], unsigned(128 - first_half_bytes));
						}
					}
					else
					{
						if (sdl_mini_buffer_start == 0)
						{
							int first_half_bytes = sdl_buffer_size - sdl_write_cursor;
							memcpy(&sdl_buffer[sdl_write_cursor], &sdl_mini_buffer[0], unsigned(first_half_bytes));
							memcpy(&sdl_buffer[0], &sdl_mini_buffer[first_half_bytes], unsigned(128 - first_half_bytes));
						}
						else
						{
							int first_half_mini_bytes = 128 - sdl_mini_buffer_start;
							int first_half_buf_bytes = sdl_buffer_size - sdl_write_cursor;

							if (first_half_mini_bytes <= first_half_buf_bytes)
							{
								memcpy(&sdl_buffer[sdl_write_cursor], &sdl_mini_buffer[sdl_mini_buffer_start], unsigned(first_half_mini_bytes));
								if (first_half_mini_bytes != first_half_buf_bytes)
									memcpy(&sdl_buffer[sdl_write_cursor + first_half_mini_bytes], &sdl_mini_buffer[0], unsigned(first_half_buf_bytes - first_half_mini_bytes));
								memcpy(&sdl_buffer[0], &sdl_mini_buffer[first_half_buf_bytes - first_half_mini_bytes], unsigned(128 - first_half_buf_bytes));
							}
							else
							{
								memcpy(&sdl_buffer[sdl_write_cursor], &sdl_mini_buffer[sdl_mini_buffer_start], unsigned(first_half_buf_bytes));
								memcpy(&sdl_buffer[0], &sdl_mini_buffer[sdl_mini_buffer_start + first_half_buf_bytes], unsigned(first_half_mini_bytes - first_half_buf_bytes));
								memcpy(&sdl_buffer[first_half_mini_bytes - first_half_buf_bytes], &sdl_mini_buffer[0], unsigned(128 - first_half_mini_bytes));
							}
						}
					}
					sdl_write_cursor = (sdl_write_cursor + 128) % sdl_buffer_size;
				}
				else
					do
					{
						sdl_buffer[sdl_write_cursor++] = sdl_mini_buffer[sdl_mini_buffer_start++];
						if (sdl_write_cursor == sdl_buffer_size)
							sdl_write_cursor = 0;
						sdl_mini_buffer_start &= 127;

						if (((sdl_write_cursor - sdl_play_cursor + sdl_buffer_size) % sdl_buffer_size) == 0)
							break;
					} while (sdl_mini_buffer_start != sdl_mini_buffer_position);
			} // ~sdl_mutex_lock
		}
	}

	extern int init_sdl(long samples_per_sec, int channels, int bits)
	{
		if (::SDL_Init(SDL_INIT_AUDIO))
		{
			wcerr << L"SDL failed to initialize." << endl;
			return 1;
		}

		SDL_AudioSpec desired;

		desired.freq = samples_per_sec;
		switch (bits)
		{
			case 8:
				desired.format = AUDIO_S8;
				break;
			case 16:
				desired.format = AUDIO_S16SYS;
				break;
			default:
				wcerr << L"The SDL module does not support " << bits << L"-bit samples." << endl;
				return 1;
		}
		if (channels > 2)
		{
			wcerr << L"The SDL module does not support more than 2 output channels." << endl;
			return 1;
		}
		desired.channels = (Uint8)channels;
		desired.samples = Uint16(samples_per_sec / 5); // 0.2 second

		desired.callback = sdl_callback;

		if (::SDL_OpenAudio(&desired, &sdl_audio_spec) == -1)
		{
			wcerr << L"SDL Audio failed to initialize" << endl;
			return 1;
		}

		sdl_buffer_size = sdl_audio_spec.samples * channels * (bits / 8) * 2; // twice the length of the output buffer
		sdl_buffer = new unsigned char[unsigned(sdl_buffer_size)];

		sdl_buffer_available_event = ::SDL_CreateCond();
		sdl_finished_event = ::SDL_CreateCond();
		sdl_buffer_lock = ::SDL_CreateMutex();
		sdl_silly_lock = ::SDL_CreateMutex();

		if (sdl_buffer_available_event
		 && sdl_finished_event
		 && sdl_buffer_lock)
			return 0;

		wcerr << L"Failed to create SDL synchronization construct." << endl;
		return 1;
	}

	extern void emit_sample_to_sdl_buffer(one_sample &sample)
	{
		sample.reset();
		while (sample.has_next())
		{
			double s = sample.next_sample();

			switch (sdl_audio_spec.format)
			{
				case AUDIO_U8:
					emit_sdl_byte(static_cast<unsigned char>(128 + s * 127));
					break;
				case AUDIO_S8:
					char s8;

					s8 = char(s * 127);
					emit_sdl_byte(*((unsigned char *)&s8));
					break;
				case AUDIO_U16:
	#if AUDIO_U16LSB != AUDIO_U16
				case AUDIO_U16LSB:
	#endif
					unsigned short u16;

					u16 = static_cast<unsigned short>(32768 + s * 32767);

					emit_sdl_byte(u16 & 0xFF);
					emit_sdl_byte(u16 >> 8);

					break;
				case AUDIO_U16MSB:
					u16 = static_cast<unsigned short>(32768 + s * 32767);

					emit_sdl_byte(u16 >> 8);
					emit_sdl_byte(u16 & 0xFF);

					break;
				case AUDIO_S16:
	#if AUDIO_S16LSB != AUDIO_S16
				case AUDIO_S16LSB:
	#endif
					signed short s16;
					unsigned short *pu16;

					s16 = short(s * 32767);
					pu16 = (unsigned short *)&s16;

					emit_sdl_byte((*pu16) & 0xFF);
					emit_sdl_byte((*pu16) >> 8);

					break;
				case AUDIO_S16MSB:
					s16 = short(s * 32767);
					pu16 = (unsigned short *)&s16;

					emit_sdl_byte((*pu16) >> 8);
					emit_sdl_byte((*pu16) & 0xFF);

					break;
			}
		}
	}

	extern void shutdown_sdl()
	{
		sdl_shutting_down = true;

		if (sdl_playing)
		{
			::SDL_mutexV(sdl_buffer_lock); // needed for some reason, dunno why. it's unlocked everywhere else.

			{
				sdl_mutex_lock lock(sdl_silly_lock);
				::SDL_CondWait(sdl_finished_event, sdl_silly_lock);
			} // ~sdl_mutex_lock

			sdl_playing = false;
		}

		::SDL_CloseAudio();

		::SDL_DestroyMutex(sdl_buffer_lock);
		::SDL_DestroyMutex(sdl_silly_lock);
		::SDL_DestroyCond(sdl_buffer_available_event);
		::SDL_DestroyCond(sdl_finished_event);

		delete[] sdl_buffer;

		::SDL_Quit();
	}
}

#endif // SDL
