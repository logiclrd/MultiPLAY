#ifndef NOTES_H
#define NOTES_H

namespace MultiPLAY
{
	namespace NewNoteAction
	{
		enum Type
		{
			Cut          = 0,
			ContinueNote = 1,
			NoteOff      = 2,
			NoteFade     = 3,
		};
	}

	namespace DuplicateCheck
	{
		enum Type
		{
			Off        = 0,
			Note       = 1,
			Sample     = 2,
			Instrument = 3,
		};
	}

	namespace DuplicateCheckAction
	{
		enum Type
		{
			Cut      = 0,
			NoteOff  = 1,
			NoteFade = 2,
		};
	}

	extern int znote_from_snote(int snote);
	extern int snote_from_znote(int znote);
	extern int snote_from_period(int period);
	extern int snote_from_inote(int inote);
}

#endif // NOTES_H
