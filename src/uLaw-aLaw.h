#ifndef ULAW_ALAW_H
#define ULAW_ALAW_H

namespace Telephony
{
	extern void init_ulaw_alaw();

	extern unsigned char ulaw_encode_sample(short sample);
	extern unsigned char alaw_encode_sample(short sample);
	extern short ulaw_decode_sample(unsigned char sample);
	extern short alaw_decode_sample(unsigned char sample);
}

#endif // ULAW_ALAW_H