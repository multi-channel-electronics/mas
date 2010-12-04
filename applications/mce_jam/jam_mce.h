#include "../../defaults/config.h"

void initialize_mce(int frequency);

void close_mce(void);

int write_mce_single(int tms, int tdi, int read_tdo);

int scan_mce(int count, char *tdi, char *tdo);

extern int specified_mce;
extern int optimized_mce;
extern int fibre_card;
