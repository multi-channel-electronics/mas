#ifndef _MCE_FAKE_H_
#define _MCE_FAKE_H_


int mce_fake_init( void );

int mce_fake_cleanup ( void );

int mce_fake_command(char *src);

int mce_fake_host(void *dest);

int mce_fake_reset( void );

int fake_data_fill(u32 *data);

#endif
