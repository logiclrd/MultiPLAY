#include "Output-DirectX.h"

#ifdef DIRECTX

#include <windows.h>
#include <dsound.h>

namespace MultiPLAY
{
	namespace
	{
		HANDLE dsNotifyEvent;
		HWND dsEventSinkWindow;
		LPDIRECTSOUND8 dsHandle;
		LPDIRECTSOUNDBUFFER dsBuffer;
		int directx_channels, directx_sample_bits, directx_quarter_buffer_size;
		int directx_buffer_index = 0;

		HWND or(HWND left, HWND right)
		{
			if (left)
				return left;
			else
				return right;
		}

		char *current_directx_buffer_ptr;
		DWORD directx_buffer_bytes_left;

		LPVOID dsBufPtr1, dsBufPtr2;
		DWORD dsBufLen1, dsBufLen2;

		bool directx_playing = false, directx_locked_buffer = false;

		inline void lock_directx_buffer(LPDIRECTSOUNDBUFFER dsBuffer)
		{
			for (int retry=0; retry<2; retry++)
				switch (dsBuffer->Lock(
									directx_buffer_index * directx_quarter_buffer_size,
									directx_quarter_buffer_size,
									&dsBufPtr1, &dsBufLen1,
									&dsBufPtr2, &dsBufLen2,
									0))
				{
					case DSERR_BUFFERLOST:
						dsBuffer->Restore();
						continue;
					case DSERR_PRIOLEVELNEEDED:
						Sleep(500);
						retry = 0;
						continue;
					default:
						retry = 3;
				}

			current_directx_buffer_ptr = (char *)dsBufPtr1;
			directx_buffer_bytes_left = dsBufLen1;

			directx_buffer_index = (directx_buffer_index + 1) & 3;
		}

		inline void unlock_directx_buffer(LPDIRECTSOUNDBUFFER dsBuffer)
		{
			dsBuffer->Unlock(dsBufPtr1, dsBufLen1, dsBufPtr2, dsBufLen2);
		}

		inline void swap_directx_buffer_halves(LPDIRECTSOUNDBUFFER buffer)
		{
			unlock_directx_buffer(buffer);
			if (directx_playing)
			{
				DWORD play_cursor, write_cursor;

				buffer->GetCurrentPosition(&play_cursor, &write_cursor);

				int play_cursor_buffer_index = play_cursor / directx_quarter_buffer_size;

				// the currently locked section is directx_buffer_index - 1
				// thus the currently playing section will typically be
				// directx_buffer_index
				if (play_cursor_buffer_index == directx_buffer_index)
				{
					ResetEvent(dsNotifyEvent);
					WaitForSingleObject(dsNotifyEvent, INFINITE);
				}
				if (play_cursor_buffer_index == ((directx_buffer_index - 1) & 3))
					cerr << "(DirectX) broken record detected (insufficient CPU time)" << endl;
			}
			else
			{
				buffer->Play(0, 0, DSBPLAY_LOOPING);
				directx_playing = true;
				ResetEvent(dsNotifyEvent);
			}
			lock_directx_buffer(buffer);
		}

		inline void emit_byte_directx(unsigned char byte)
		{
			current_directx_buffer_ptr[0] = byte;
			current_directx_buffer_ptr++;
			directx_buffer_bytes_left--;

			if ((directx_buffer_bytes_left == 0) && ((current_directx_buffer_ptr - dsBufLen2) != dsBufPtr2))
			{
				current_directx_buffer_ptr = (char *)dsBufPtr2;
				directx_buffer_bytes_left = dsBufLen2;
			}

			if (directx_buffer_bytes_left == 0)
				swap_directx_buffer_halves(dsBuffer);
		}
	}

	extern int init_directx(long samples_per_sec, int channels, int bits)
	{
		if ((bits != 8) && (bits != 16))
		{
			cerr << "DirectSound output can only use 8- or 16-bit samples" << endl;
			return 1;
		}

		if (channels > 2)
		{
			cerr << "DirectSound output for more than 2 channels not implemented" << endl;
			return 1;
		}

		if (FAILED(DirectSoundCreate8(NULL, &dsHandle, NULL)))
		{
			cerr << "Unable to initialize DirectSound" << endl;
			return 1;
		}

		if (FAILED(dsHandle->SetCooperativeLevel(or(GetForegroundWindow(), GetDesktopWindow()), DSSCL_PRIORITY)))
		{
			cerr << "Unable to set DirectSound cooperative level" << endl;
			return 1;
		}

		directx_channels = channels;
		directx_sample_bits = bits;                                                   
		directx_quarter_buffer_size = samples_per_sec * channels * ((bits + 7) / 8) / 4;
		if (4 * directx_quarter_buffer_size < DSBSIZE_MIN)
			directx_quarter_buffer_size = DSBSIZE_MIN >> 2;
		if (4 * directx_quarter_buffer_size > DSBSIZE_MAX)
			directx_quarter_buffer_size = DSBSIZE_MAX >> 2;

		DSBUFFERDESC bufferDesc;
		WAVEFORMATEX waveFormat;

		waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		waveFormat.nChannels = channels;
		waveFormat.nSamplesPerSec = samples_per_sec;
		waveFormat.nAvgBytesPerSec = samples_per_sec * channels * ((bits + 7) / 8);
		waveFormat.nBlockAlign = channels * ((bits + 7) / 8);
		waveFormat.wBitsPerSample = bits;
		waveFormat.cbSize = 0;

		bufferDesc.dwSize = sizeof(bufferDesc);
		bufferDesc.dwFlags = DSBCAPS_CTRLFREQUENCY
											| DSBCAPS_CTRLPOSITIONNOTIFY
											| DSBCAPS_GETCURRENTPOSITION2
											| DSBCAPS_GLOBALFOCUS
											| DSBCAPS_LOCSOFTWARE
											| DSBCAPS_STATIC
											| DSBCAPS_STICKYFOCUS;
		bufferDesc.dwBufferBytes = 4 * directx_quarter_buffer_size;
		bufferDesc.dwReserved = 0;
		bufferDesc.lpwfxFormat = &waveFormat;
		bufferDesc.guid3DAlgorithm = DS3DALG_DEFAULT;

		if (FAILED(dsHandle->CreateSoundBuffer(&bufferDesc, &dsBuffer, NULL)))
		{
			cerr << "Unable to create output buffer" << endl;
			return 1;
		}

		dsNotifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		DSBPOSITIONNOTIFY positionNotify[4];
		positionNotify[0].dwOffset = 0 * directx_quarter_buffer_size;
		positionNotify[0].hEventNotify = dsNotifyEvent;
		positionNotify[1].dwOffset = 1 * directx_quarter_buffer_size;
		positionNotify[1].hEventNotify = dsNotifyEvent;
		positionNotify[2].dwOffset = 2 * directx_quarter_buffer_size;
		positionNotify[2].hEventNotify = dsNotifyEvent;
		positionNotify[3].dwOffset = 3 * directx_quarter_buffer_size;
		positionNotify[3].hEventNotify = dsNotifyEvent;

		LPDIRECTSOUNDNOTIFY8 bufferNotify;

		if (FAILED(dsBuffer->QueryInterface(IID_IDirectSoundNotify8, (LPVOID *)&bufferNotify)))
		{
			cerr << "Unable to create notification point object for output buffer" << endl;
			return 1;
		}

		if (FAILED(bufferNotify->SetNotificationPositions(4, positionNotify)))
		{
			cerr << "Unable to set position notifies for output buffer" << endl;
			return 1;
		}

		bufferNotify->Release();

		cerr << "DirectX initialized!" << endl;
		return 0;
	}

	extern void emit_sample_to_directx_buffer(one_sample &sample)
	{
		sample.reset();

		while (sample.has_next())
		{
			double this_sample = sample.next_sample();

			if (directx_locked_buffer == false)
			{
				lock_directx_buffer(dsBuffer);
				directx_locked_buffer = true;
			}

			switch (directx_sample_bits)
			{
				case 8:
					emit_byte_directx(unsigned char(this_sample * 127 + 128));
					break;
				case 16:
					short sample_short = short(this_sample * 32767);

					emit_byte_directx(0[(unsigned char *)&sample_short]);
					emit_byte_directx(1[(unsigned char *)&sample_short]);
					break;
			}
		}
	}

	extern void shutdown_directx()
	{
		if (directx_locked_buffer)
		{
			unlock_directx_buffer(dsBuffer);
			directx_locked_buffer = false;
		}

		if (directx_playing)
		{
			DWORD play_cursor, write_cursor;

			dsBuffer->GetCurrentPosition(&play_cursor, &write_cursor);
			directx_buffer_index = (directx_buffer_index + 2) & 3;
			while ((play_cursor / directx_quarter_buffer_size) != directx_buffer_index)
			{
				ResetEvent(dsNotifyEvent);
				WaitForSingleObject(dsNotifyEvent, INFINITE);
				dsBuffer->GetCurrentPosition(&play_cursor, &write_cursor);
			}
			dsBuffer->Stop();
			directx_playing = false;
		}

		dsBuffer->Release();

		dsHandle->Release();

		CloseHandle(dsNotifyEvent);
	}
}

#endif // DIRECTX