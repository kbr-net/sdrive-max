#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>
#include "avrlibdefs.h"         // global AVRLIB defines
#include "avrlibtypes.h"        // global AVRLIB types definitions
#include "global.h"
#include "display.h"
#include "logos.h"
#include "touchscreen.h"
#include "tft.h"
#include "fat.h"
#include "tape.h"

extern unsigned char debug;
extern char atari_sector_buffer[];
extern struct FileInfoStruct FileInfo;
extern virtual_disk_t vDisk[];
//extern struct GlobalSystemValues GS;

#define	atari_bg 0x257b

unsigned char actual_page = PAGE_MAIN;
unsigned char tape_mode = 0;
unsigned int next_file_idx = 0;
unsigned int nfiles = 0;
unsigned int file_selected = -1;
char path[13] = "/";
const char ready_str[] PROGMEM = "READY";
const char PROGMEM known_extensions[][3] = { "ATR", "ATX", "CAS", "COM", "BIN", "EXE", "XEX", "XFD", "TAP", "IMG" };
struct TSPoint p;

void main_page();
void file_page();
void config_page();
void tape_page();
unsigned int debug_page();

struct display tft;

unsigned char EEMEM cfg = 0xfb;	//config byte on eeprom, initial value is all on except boot_d1
struct file_save EEMEM image_store[DEVICESNUM-1] = {[0 ... DEVICESNUM-2] = { 0xffffffff, 0xffff }};
extern u16 EEMEM MINX;
extern u16 EEMEM MINY;
extern u16 EEMEM MAXX;
extern u16 EEMEM MAXY;


unsigned int action_b0 (struct button *b) {
	struct b_flags *flags = pgm_read_ptr(&b->flags);
	flags->selected = 1;
	return(0);
}

unsigned int action_b1_4 (struct button *b) {

	if(p.x > 200) {	//file select page
		actual_page = PAGE_FILE;
		sei();
		tft.pages[actual_page].draw();
	}
	else if(p.x < 40) {	//deactivate
		return(-1);
	}
	else {	//set actual drive to
		struct b_flags *flags = pgm_read_ptr(&b->flags);
		flags->selected = 1;
	}
	return(0);
}

unsigned int action_tape (struct button *b) {
	actual_page = PAGE_FILE;
	tape_mode = 1;
	tape_flags.turbo = 0;	//start with no turbo
	sei();
	tft.pages[actual_page].draw();
	return(0);
}

unsigned int action_tape_turbo (struct button *b) {
	struct b_flags *flags = pgm_read_ptr(&b->flags);
	flags->selected = ~flags->selected;
	tape_flags.turbo = ~tape_flags.turbo;
	draw_Buttons();
	return(0);
}

unsigned int action_cancel () {
	//on file_page reset file index to same page
	if (actual_page == PAGE_FILE)
		next_file_idx -= 10;
	//on debug_page deactivate them
	else {
		debug = 0;
		tape_mode = 0;
	}
	//and reset to main_page
	actual_page = PAGE_MAIN;
	tft.pages[actual_page].draw();
	return(0);
}

void pretty_name(char *b) {	//insert dot in filename.ext
	unsigned char i = 11;

	while(i > 8) {
		b[i] = b[i-1];
		i--;
	}
	b[i] = '.';
	b[12] = 0;	//mark new end
}

unsigned int list_files () {
	unsigned int i;
	unsigned int col;
	unsigned char e;

	if(!nfiles)
		while (fatGetDirEntry(nfiles,0)) nfiles++;

	if(!fatGetDirEntry(next_file_idx,0))
		return(0);

	set_text_pos(15,32);	//page counter
	sprintf_P(atari_sector_buffer, PSTR("%i files, page %i/%i  %s            "),
		nfiles, next_file_idx/10+1, (nfiles-1)/10+1, path);
	print_ln(1,White,Black,atari_sector_buffer);

	set_text_pos(15,45);
	for(i = next_file_idx; i < next_file_idx+10; i++) {
		//print_I(0,45+(i*8*2),1,White,Black,i);
		if(fatGetDirEntry(i,0)) {
			if(FileInfo.Attr & ATTR_DIRECTORY)	//other color
				col = 0x07ff;
			else {
				col = Green - 0x1863;
				for(e=0; e < (sizeof(known_extensions)/3); e++)
				{
					if(atari_sector_buffer[8] == pgm_read_byte(&known_extensions[e][0]) &&
					   atari_sector_buffer[9] == pgm_read_byte(&known_extensions[e][1]) &&
					   atari_sector_buffer[10] == pgm_read_byte(&known_extensions[e][2]) )
					{
					    col = Green;
					    break;	//one match is enaugh
					}
				}
			}
			//if extension, insert dot
			if(atari_sector_buffer[8] != ' ')
				pretty_name(atari_sector_buffer);
			else {
				//add a space to have same length
				atari_sector_buffer[11] = ' ';
				atari_sector_buffer[12] = 0;
			}
			if(i == file_selected)
				print_ln(2,0xfff0,Light_Grey,atari_sector_buffer);
			else
				print_ln(2,col,Light_Grey,atari_sector_buffer);
		}
		else
			print_ln_P(2,Green,Light_Grey,PSTR("            "));
	}
	next_file_idx = i;
	return(0);
}

unsigned int list_files_rev () {
	if (next_file_idx >= 20) {
		next_file_idx -= 20;
		list_files();
	}
	return(0);
}

unsigned int list_files_top () {
	next_file_idx = 0;
	list_files();
	return(0);
}

unsigned int list_files_last () {
	//unsigned short i = 0;
	//while (fatGetDirEntry(i,0)) i++;
	next_file_idx = (nfiles-1)/10*10;
	list_files();
	return(0);
}

unsigned int action_select() {
	unsigned int file;
	unsigned char i;
	unsigned long odirc;

	file = p.y - 45;	// 45-280 => 0-235
	file /= 24;		// split into 10 pieces
	//print_I(130,294,1,White,atari_bg,file);
	file += next_file_idx - 10;
	//print_I(160,294,1,White,atari_bg,file);

	fatGetDirEntry(file,1);
	if(FileInfo.Attr & ATTR_DIRECTORY) {
		//set new directory to current
		FileInfo.vDisk->dir_cluster=FileInfo.vDisk->start_cluster;

		//if we have a change to previous dir
		if(atari_sector_buffer[0] == '.' && atari_sector_buffer[1] == '.') {
			//was a change to root?
			if(FileInfo.vDisk->start_cluster == 0) {
				strncpy(path, "/", 2);
				goto was_root;
			}
			//remember current dir
			odirc = FileInfo.vDisk->start_cluster;
			fatGetDirEntry(0,0);	//get prev dir entry (..)
			//and set it to actual directory
			FileInfo.vDisk->dir_cluster=FileInfo.vDisk->start_cluster;
			//find the prev dir name
			for(i = 0; i < 255; i++) {
				fatGetDirEntry(i,1);
				//where the start_cluster matches the cur. dir
				if(FileInfo.vDisk->start_cluster == odirc)
					break;
			}
			//reset directory to current
			FileInfo.vDisk->dir_cluster = odirc;
		}
		strncpy(path, atari_sector_buffer, 12);
was_root:	//outbox(path);

		next_file_idx = 0;
		nfiles = 0;
		file_selected = -1;
	}
	else {	//is a file

		//print long filename
		atari_sector_buffer[32] = 0;	//chop
		outbox(atari_sector_buffer);

		if(tft.cfg.scroll) {
			//prepare scrolling filename
			atari_sector_buffer[19] = 0;	//chop
			Draw_Rectangle(5,10,tft.width-1,26,1,SQUARE,Black,Black);
			print_str(5, 10, 2, Yellow, Black, atari_sector_buffer);
		}

		file_selected = file;
		next_file_idx -= 10;	// the same list again
	}
	list_files();
	//read file again, that we have the long name in buffer
	fatGetDirEntry(file,1);
	return(0);
}

unsigned int action_ok () {
	actual_page = PAGE_MAIN;
	next_file_idx -= 10;
	if(tape_mode) {
		actual_page = PAGE_TAPE;
		file_selected = 0;
	}
	tft.pages[actual_page].draw();
	return(file_selected);
}

unsigned int action_cfg () {
	actual_page = PAGE_CONFIG;
	tft.pages[actual_page].draw();
	return(0);
}

unsigned int action_change (struct button *b) {
	struct b_flags *flags = pgm_read_ptr(&b->flags);
	//invert selection
	flags->selected = ~flags->selected;
	draw_Buttons();
	return(0);
}

unsigned int action_save_cfg () {
	struct button *b;
	struct b_flags *flags;
	unsigned char i;

	*(char*)&tft.cfg = 0;	//clear first
	for(i = 0; i < 4; i++) {
		b = &tft.pages[actual_page].buttons[i];
		flags = pgm_read_ptr(&b->flags);
		if(i < 3)	//not save last(SaveIm) button, only load ptr
			*(char*)&tft.cfg |= flags->selected << i;
	}
	eeprom_update_byte(&cfg, *(char *)&tft.cfg);
	//check for SaveIm Button
	if(flags->selected) {
		//map D1-D4 0-indexed
		for(i = 0; i < DEVICESNUM-1; i++) {
			if(vDisk[i+1].flags & FLAGS_DRIVEON) {
				eeprom_update_dword(&image_store[i].dir_cluster, vDisk[i+1].dir_cluster);
				eeprom_update_word(&image_store[i].file_index, vDisk[i+1].file_index);
			}
			else {
				eeprom_update_dword(&image_store[i].dir_cluster, 0xffffffff);
			}
		}
	}
	TFT_set_rotation(tft.cfg.rot);
	action_cancel();
	return(0);
}

unsigned int action_cal () {
	unsigned int x1,x2,y1,y2,diff;
	TFT_fill(Black);

	Draw_H_Line(10,30,20,White);
	Draw_V_Line(20,10,30,White);
	while(isTouching());	//wait for release
	while(!isTouching());
	p = getRawPoint();
	x1 = p.x;
	y1 = p.y;
	//print_I(10,50,1,White,Black,p.x);
	//print_I(40,50,1,White,Black,p.y);
	while(isTouching());

	Draw_H_Line(tft.width-10,tft.width-30,20,White);
	Draw_V_Line(tft.width-20,10,30,White);
	while(!isTouching());
	p = getRawPoint();
	x2 = p.x;
	y1 = (y1 + p.y) / 2;	//middle
	//print_I(tft.width-60,50,1,White,Black,p.x);
	//print_I(tft.width-30,50,1,White,Black,p.y);
	while(isTouching());

	Draw_H_Line(10,30,tft.heigth-20,White);
	Draw_V_Line(20,tft.heigth-10,tft.heigth-30,White);
	while(!isTouching());
	p = getRawPoint();
	x1 = (x1 + p.x) / 2;	//middle
	y2 = p.y;
	//print_I(10,tft.heigth-50,1,White,Black,p.x);
	//print_I(40,tft.heigth-50,1,White,Black,p.y);
	while(isTouching());

	Draw_H_Line(tft.width-10,tft.width-30,tft.heigth-20,White);
	Draw_V_Line(tft.width-20,tft.heigth-10,tft.heigth-30,White);
	while(!isTouching());
	p = getRawPoint();
	x2 = (x2 + p.x) / 2;	//middle
	y2 = (y2 + p.y) / 2;	//middle
	//print_I(tft.width-60,tft.heigth-50,1,White,Black,p.x);
	//print_I(tft.width-30,tft.heigth-50,1,White,Black,p.y);
/*
	print_I(20,tft.heigth/2,1,White,Black,x1);
	print_I(50,tft.heigth/2,1,White,Black,x2);
	print_I(80,tft.heigth/2,1,White,Black,y1);
	print_I(110,tft.heigth/2,1,White,Black,y2);
*/
	if(x1 > x2) {
		diff = x1;
		x1 = x2;
		x2 = diff;
	}
        diff = (x2 - x1)/(tft.width-40.0)*tft.width;
        diff -= (x2 - x1);
        diff /= 2;
	x1 -= diff;
	x2 += diff;
	if(y1 > y2) {
		diff = y1;
		y1 = y2;
		y2 = diff;
	}
        diff = (y2 - y1)/(tft.heigth-40.0)*tft.heigth;
        diff -= (y2 - y1);
        diff /= 2;
	y1 -= diff;
	y2 += diff;
/*
	print_I(20,tft.heigth/2+12,1,White,Black,x1);
	print_I(50,tft.heigth/2+12,1,White,Black,x2);
	print_I(80,tft.heigth/2+12,1,White,Black,y1);
	print_I(110,tft.heigth/2+12,1,White,Black,y2);
*/
	eeprom_update_word(&MINX, x1);
	eeprom_update_word(&MAXX, x2);
	eeprom_update_word(&MINY, y1);
	eeprom_update_word(&MAXY, y2);

	while(isTouching());
	return(0);
}

unsigned int press () {	//for buttons with no action here
	return(0);
}

const struct button PROGMEM buttons_main[] = {
	//name, x, y, width, heigth, fg-col, bg-col, font-col, type, act, sel
	{"D0:",10,200,50,30,Grey,Black,Black,&(struct b_flags){ROUND,1,1},action_b0},
	//D1:FILENAME.ATR must fit!
	{"D1:<empty>     ",10,40,240-21,30,Grey,Black,Black,&(struct b_flags){ROUND,1,0},action_b1_4},
	{"D2:<empty>     ",10,80,240-21,30,Grey,Black,Black,&(struct b_flags){ROUND,1,0},action_b1_4},
	{"D3:<empty>     ",10,120,240-21,30,Grey,Black,Black,&(struct b_flags){ROUND,1,0},action_b1_4},
	{"D4:<empty>     ",10,160,240-21,30,Grey,Black,Black,&(struct b_flags){ROUND,1,0},action_b1_4},
	{"Tape:",80,200,80,30,Grey,Black,Black,&(struct b_flags){ROUND,1,0},action_tape},
	{"New",240-61,200,50,30,Grey,Black,Green,&(struct b_flags){ROUND,1,0},press},
	{"Cfg",240-61,240,50,30,Grey,Black,Blue,&(struct b_flags){ROUND,1,0},action_cfg},
	{"Outbox",10,280,240-11,320-1,0,0,0,&(struct b_flags){0,0,0},debug_page}
};

const struct button PROGMEM buttons_file[] = {
	{"Top",164,45,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},list_files_top},
	{"Prev",164,85,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},list_files_rev},
	{"OK",164,125,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_ok},
	{"Exit",164,165,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_cancel},
	{"Next",164,205,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},list_files},
	{"Last",164,245,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},list_files_last},
	{"File",15,45,80,240,Grey,Black,White,&(struct b_flags){ROUND,0,0},action_select}
};

//keep the order analog to struct tft.cfg, otherwise read/write function
//wouldn't work correctly!!!
const struct button PROGMEM buttons_cfg[] = {
	{"Rotate",15,45,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"Scroll",15,85,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"BootD1",15,125,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"SaveIm",15,165,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
/* all 8 needs too much RAM!!!
	{"empty3",15,125,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"empty4",15,165,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"empty5",15,205,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"empty6",15,245,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"empty7",115,125,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"empty8",115,165,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
*/
	{"Save",164,125,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_save_cfg},
	{"Exit",164,165,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_cancel}
};

const struct button PROGMEM buttons_tape[] = {
	{"Start",15,165,80,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},press},
	{"Tur",105,165,50,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_tape_turbo},
	{"Exit",164,165,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_cancel}
};

const struct button PROGMEM buttons_debug[] = {
	{"Back",0,0,240,280,Grey,Black,White,&(struct b_flags){ROUND,0,0},action_cancel}
};

struct page pages[] = {
    {main_page, buttons_main, sizeof(buttons_main)/sizeof(struct button)},
    {file_page, buttons_file, sizeof(buttons_file)/sizeof(struct button)},
    {config_page, buttons_cfg, sizeof(buttons_cfg)/sizeof(struct button)},
    {tape_page, buttons_tape, sizeof(buttons_tape)/sizeof(struct button)},
    {debug_page, buttons_debug, sizeof(buttons_debug)/sizeof(struct button)}
};

struct display tft = {240, 320, {PORTRAIT_2, 0}, pages};

void draw_Buttons () {
	struct button b,*bp;
	unsigned char i,j;

	for(i = 0; i < tft.pages[actual_page].nbuttons; i++) {
		bp = &tft.pages[actual_page].buttons[i];
		//read the whole button data from pgm to struct
		for(j = 0; j < sizeof(struct button); j++) {
			*((char*)&b+j) = pgm_read_byte((char*)bp+j);
		}
		if(!b.flags->active)
			continue;
		if(b.flags->selected) {
			b.fg = Blue;
			b.fc = Yellow;
		}
		Draw_Rectangle(b.x,b.y,b.x+b.width,b.y+b.heigth,1,b.flags->type,b.fg,b.bg);

/*		//3D Effekt Test
		print_I(120,262,1,White,atari_bg,b.fg);
		print_I(150,262,1,White,atari_bg,b.fg>>1);
		Draw_H_Line(b.x,b.x+b.width,b.y+b.heigth,b.fg>>1);
		Draw_V_Line(b.x+b.width,b.y,b.y+b.heigth,b.fg>>1);
		Draw_H_Line(b.x+1,b.x+b.width+1,b.y+b.heigth+1,b.fg>>1);
		Draw_V_Line(b.x+b.width+1,b.y+1,b.y+b.heigth+1,b.fg>>1);
*/
		print_str(b.x+10,b.y+b.heigth/2-6,2,b.fc,b.fg,b.name);

		//Logos
		if(b.name[0] == 'D' && b.name[1] != '0') {
			Draw_BMP(b.x+b.width-18,b.y+8,b.x+b.width-2,b.y+8+16,disk_image);
			if(b.name[3] == '<')
				Draw_Line(b.x+b.width-18,b.y+8,b.x+b.width-2,b.y+8+16,Red);
		}
	}
}

unsigned int outx, outy;
unsigned char scroll;

/*
void outbox_P(const char *txt) {

	if (outy > tft.heigth-8) {
		outy = 284;
		scroll = 1;
	}
	print_str_P(outx,outy,1,White,atari_bg,txt);
	outy += 8;
	if (scroll)
		TFT_scroll(outy);
}
*/

void _outbox(char *txt, char P) {

	if (outy > tft.heigth-16) {
		scroll = 1;
	}

	//clear line
	Draw_Rectangle(outx,outy,tft.width-11,outy+7,1,SQUARE,atari_bg,Black);

	if(P)
		print_str_P(outx,outy,1,White,atari_bg,txt);
	else
		print_str(outx,outy,1,White,atari_bg,txt);

	outy += 8;
	if (scroll) {
		if (outy > tft.heigth-8) {
			if (actual_page == PAGE_DEBUG)
				outy = 10;
			else
				outy = 284;
		}
		TFT_scroll(outy);
	}
}

void outbox_P(const char *txt) {
	_outbox(txt,1);
}

void outbox (char *txt) {
	_outbox(txt,0);
}

void main_page () {

	outx = 20; outy = 284;

	TFT_off();	//turn off if returned to main to avoid scrollbak ef.
	TFT_scroll_init(outy,32,4);
	TFT_scroll(outy);
	TFT_on();
	TFT_fill(Black);

	//Header
	print_str_P(20, 10, 2, Orange, Black, PSTR("SDrive-MAX"));
	print_str_P(160, 18, 1, Orange, Black, PSTR("by KBr V0.7b"));
	Draw_H_Line(0,tft.width,30,Orange);

	draw_Buttons();

	//Output Box
	Draw_Rectangle(10,280,tft.width-11,tft.heigth-1,1,SQUARE,atari_bg,Black);
	scroll = 0;
	set_text_pos(outx, outy);
	outbox_P(ready_str);
	print_char(20,292,1,White,atari_bg,0x80);

}

void file_page () {

	Draw_Rectangle(10,40,tft.width-11,280,1,SQUARE,Light_Grey,Black);
	Draw_Rectangle(10,40,tft.width-11,280,0,SQUARE,Grey,Black);
	Draw_Rectangle(11,41,tft.width-12,279,0,SQUARE,Grey,Black);
	//Draw_Rectangle(12,42,tft.width-13,278,0,SQUARE,Grey,Black);
	draw_Buttons();
	file_selected = -1;
	list_files();
}

void config_page () {
	struct button *b;
	struct b_flags *flags;
	unsigned int i;

	Draw_Rectangle(10,40,tft.width-11,280,1,SQUARE,Light_Grey,Black);
	Draw_Rectangle(10,40,tft.width-11,280,0,SQUARE,Grey,Black);
	Draw_Rectangle(11,41,tft.width-12,279,0,SQUARE,Grey,Black);
	//Draw_Rectangle(12,42,tft.width-13,278,0,SQUARE,Grey,Black);
	for(i = 0; i < 4; i++) {
		b = &tft.pages[actual_page].buttons[i];
		flags = pgm_read_ptr(&b->flags);
		flags->selected = (*(char*)&tft.cfg >> i) & 1;
		if(i == 3)
			flags->selected = 0;
	}
	draw_Buttons();
}

void tape_page () {
	Draw_Rectangle(10,100,tft.width-11,200,1,SQUARE,Light_Grey,Black);
	Draw_Rectangle(10,100,tft.width-11,200,0,SQUARE,Grey,Black);
	Draw_Rectangle(11,101,tft.width-12,199,0,SQUARE,Grey,Black);
	print_str_P(70, 105, 2, Orange, Light_Grey, PSTR("Tape-Emu"));
	Draw_H_Line(12,tft.width-13,122,Orange);
	draw_Buttons();
}

unsigned int debug_page () {

	TFT_fill(atari_bg);

	outx = 10; outy = 10;
	TFT_scroll_init(outy,304,6);
	TFT_scroll(outy);
	scroll = 0;
	set_text_pos(outx, outy);
	outbox_P(ready_str);
	print_char(10,18,1,White,atari_bg,0x80);

	debug = 1;
	actual_page = PAGE_DEBUG;
	return(0);
}

void tft_Setup() {
	unsigned int id;

	TFT_init();
	*(char *)&tft.cfg = eeprom_read_byte(&cfg);
	TFT_set_rotation(tft.cfg.rot);
	id = TFT_getID();
	sprintf_P(atari_sector_buffer, PSTR("TFT-ID: %.04x"), id);
	//outbox(atari_sector_buffer);
	print_str(20, 290, 2, White, Black, atari_sector_buffer);
	//check if touchscreen needs calibration
	if(eeprom_read_word(&MINX) == 0xffff || isTouching() ) {
		TFT_on();
		action_cal();
	}
}

struct button * check_Buttons() {
	struct button *b;
	unsigned char i;
	unsigned int x,y,width,heigth;

	p = getPoint();
	//print_I(30,292,1,White,atari_bg,p.x);
	//print_I(60,292,1,White,atari_bg,p.y);
	for(i = 0; i < tft.pages[actual_page].nbuttons; i++) {
		b = &tft.pages[actual_page].buttons[i];
		x = pgm_read_word(&b->x);
		y = pgm_read_word(&b->y);
		width = pgm_read_word(&b->width);
		heigth = pgm_read_word(&b->heigth);
		if(p.x > x && p.x < x+width &&
		   p.y > y && p.y < y+heigth)
			return(b);
	}
	return(0);
}

