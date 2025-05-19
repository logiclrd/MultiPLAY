namespace MultiPLAY
{
	extern const double mod_finetune[16] = {
		7895.0, 7941.0, 7985.0, 8046.0, 8107.0, 8169.0, 8232.0, 8280.0,
		8363.0, 8413.0, 8463.0, 8529.0, 8581.0, 8651.0, 8723.0, 8757.0 };
	/* The above were provided by some tutorial, but I don't think they're right. */

	// These were calculated according to one finetune step being 1/8 of a
	// semitone. Thus, the first entry is one semitone lower than the 8th
	// entry.
	/*
	extern const double mod_finetune[16] = {
		7893.646721,
		7950.844085,
		8008.455902,
		8066.485173,
		8124.934925,
		8183.808203,
		8243.108077,
		8302.837638,
		8363.000000,
		8423.598298,
		8484.635691,
		8546.115361,
		8608.040513,
		8670.414375,
		8733.240198,
		8796.521256 };
	/**/
}
