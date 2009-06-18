#ifndef _DATA_MODE_H_
#define _DATA_MODE_H_

enum mce_data_type { DATA_MODE_RAW, DATA_MODE_SCALE, DATA_MODE_EXTRACT,
		     DATA_MODE_EXTRACT_SCALE };

struct mce_data_field {
	int data_mode;
	enum mce_data_type type;
	char *name;
	int bit_start;
	int bit_count;
	double scalar;
};


#define MCE_DATA_ERROR   "error"
#define MCE_DATA_FB      "fb"
#define MCE_DATA_FILT    "filt"
#define MCE_DATA_FJ      "fj"
#define MCE_DATA_RAW     "raw"
#define MCE_DATA_ROW     "row"
#define MCE_DATA_COL     "col"


/* Walk this struct.  It is NULL terminated. */

extern struct mce_data_field* mce_data_fields[];


#endif /* _DATA_MODE_H_ */
