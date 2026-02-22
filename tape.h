#ifndef __TAPE_H__
#define __TAPE_H__

#define BLOCK_LEN 128

struct tape_FUJI_hdr {
	char chunk_type[4];
	unsigned short chunk_length;
	unsigned short irg_length;
	char data[];
};

typedef struct st_flags {
	unsigned int offset;
	unsigned char run : 1;
	unsigned char FUJI : 1;
	unsigned char turbo : 1;
} t_flags;

unsigned int send_tape_block (unsigned int offset);
void check_for_FUJI_file ();
unsigned int send_FUJI_tape_block (unsigned int offset);
extern t_flags tape_flags;

#endif