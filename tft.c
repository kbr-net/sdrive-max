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
#include "touchscreen.h"
#include "tft.h"
#include "fat.h"
#include "tape.h"

extern unsigned char debug;
extern char atari_sector_buffer[];
extern struct FileInfoStruct FileInfo;
extern virtual_disk_t vDisk[];
//extern struct GlobalSystemValues GS;
extern const char system_name[] PROGMEM;
extern const char system_version[] PROGMEM;
extern uint8_t system_fastsio_pokeydiv_default;
extern struct SDriveParameters sdrparams;
#define pokeydiv sdrparams.p2

unsigned char actual_page = PAGE_MAIN;
unsigned char tape_mode = 0;
unsigned int next_file_idx = 0;
unsigned int nfiles = 0;
unsigned int file_selected = -1;
unsigned char scroll_file_len;
char path[13] = "/";
const char ready_str[] PROGMEM = "READY";
const char known_extensions[][3] PROGMEM = { "ATR", "ATX", "CAS", "COM", "BIN", "EXE", "XEX", "XFD", "TAP", "IMG", "BAS" };

void main_page();
void file_page();
void config_page();
void tape_page();
unsigned int debug_page();

struct display tft;
struct TSPoint p;

unsigned char cfg EEMEM = 0xf3;	//config byte on eeprom, initial value is all on except boot_d1 and 1050
struct file_save image_store[DEVICESNUM-1] EEMEM = {[0 ... DEVICESNUM-2] = { 0xffffffff, 0xffff }};
extern u16 MINX EEMEM;
extern u16 MINY EEMEM;
extern u16 MAXX EEMEM;
extern u16 MAXY EEMEM;


unsigned int action_b0 (const struct button *b) {
	struct b_flags *flags = pgm_read_ptr(&b->flags);
	flags->selected = 1;
	return(0);
}

unsigned int action_b1_4 (const struct button *b) {

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

unsigned int action_tape (const struct button *b) {
	actual_page = PAGE_FILE;
	tape_mode = 1;
	sei();
	tft.pages[actual_page].draw();
	return(0);
}

unsigned int action_tape_start (const struct button *b) {
	struct b_flags *flags = pgm_read_ptr(&b->flags);
	const struct button *pb = &tft.pages[actual_page].buttons[1];
	struct b_flags *pause = pgm_read_ptr(&pb->flags);

	if(tape_flags.run || pause->selected) { //Stop
		tape_flags.run = 0;
		flags->selected = 0;
		//clear also the Pause Button
		pause->selected = 0;
		print_str_P(35,132,2,Yellow,window_bg, PSTR("Stopped...   "));
		draw_Buttons();
	}
	else {          //Start
		FileInfo.vDisk->current_cluster=FileInfo.vDisk->start_cluster;
		check_for_FUJI_file();
		tape_flags.run = 1;
		flags->selected = 1;
		print_str_P(35,132,2,Yellow,window_bg, PSTR("Sync Wait...   "));
		draw_Buttons();
		if(!tape_flags.FUJI) {
			//sync wait
			_delay_ms(10000);
		}
	}
	tape_flags.offset = 0;	//reset offset in any case
	return(0);
}

unsigned int action_tape_turbo (const struct button *b) {
	struct b_flags *flags = pgm_read_ptr(&b->flags);
	flags->selected = ~flags->selected;
	tape_flags.turbo = ~tape_flags.turbo;
	draw_Buttons();
	return(0);
}

unsigned int action_tape_pause (const struct button *b) {
	struct b_flags *flags = pgm_read_ptr(&b->flags);
	if(flags->selected) {
		tape_flags.run = 1;
		flags->selected = 0;
		draw_Buttons();
	}
	else
	if(tape_flags.run) {
		tape_flags.run = 0;
		flags->selected = 1;
		draw_Buttons();
	}
	return(0);
}

unsigned int action_cancel () {
	//on file_page reset file index to same page
	if (actual_page == PAGE_FILE)
		next_file_idx -= 10;
	//in case of debug_page deactivate them
	debug = 0;
	//same for tape_page
	tape_mode = 0;
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
				print_ln(2,0xfff0,window_bg,atari_sector_buffer);
			else
				print_ln(2,col,window_bg,atari_sector_buffer);
		}
		else
			print_ln_P(2,Green,window_bg,PSTR("            "));
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
		//save the length for scrolling
		scroll_file_len = strlen((char*)atari_sector_buffer);

		//print long filename
		//atari_sector_buffer[32] = 0;	//chop
		//outbox(atari_sector_buffer);
		outbox_multi(atari_sector_buffer, scroll_file_len);
/*
		if(tft.cfg.scroll) {
			//prepare scrolling filename
			atari_sector_buffer[19] = 0;	//chop
			Draw_Rectangle(5,10,tft.width-1,26,1,SQUARE,Black,Black);
			print_str(5, 10, 2, Yellow, Black, atari_sector_buffer);
		}
*/
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
		if(file_selected != -1)
			actual_page = PAGE_TAPE;
		else	//reset tape mode, if nothing selected
			tape_mode = 0;
		file_selected = 0;	//mark no file action on main page
	}
	tft.pages[actual_page].draw();
	return(file_selected);
}

unsigned int action_cfg () {
	actual_page = PAGE_CONFIG;
	tft.pages[actual_page].draw();
	return(0);
}

unsigned int action_change (const struct button *b) {
	struct b_flags *flags = pgm_read_ptr(&b->flags);
	//invert selection
	flags->selected = ~flags->selected;
	draw_Buttons();
	return(0);
}

void print_pokeydiv () {
	char buf[3];
	sprintf_P(buf, PSTR("$%02X"), pokeydiv);
	print_str(150,95,2,Yellow,window_bg,buf);
}

unsigned int action_pokey () {
	pokeydiv++;
	if(pokeydiv > 40)
		pokeydiv = 0;
	if(pokeydiv == 11)
		pokeydiv = 16;
	if(pokeydiv == 17)
		pokeydiv = 40;
	print_pokeydiv();
	return(0);
}

unsigned int action_save_cfg () {
	const struct button *b;
	struct b_flags *flags;
	unsigned char i;
	unsigned char rot = tft.cfg.rot;

	*(char*)&tft.cfg = 0;	//clear first
	for(i = 0; i < tft.pages[actual_page].nbuttons-3; i++) {
		b = &tft.pages[actual_page].buttons[i];
		flags = pgm_read_ptr(&b->flags);
		if(i < tft.pages[actual_page].nbuttons-4)	//not save last(SaveIm) button, only load ptr
			*(char*)&tft.cfg |= flags->selected << i;
	}
	eeprom_update_byte(&cfg, *(char *)&tft.cfg);
	//check for SaveIm Button
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
	if(flags->selected) {
#pragma GCC diagnostic warning "-Wmaybe-uninitialized"
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
	eeprom_update_byte(&system_fastsio_pokeydiv_default, pokeydiv);
	if(rot != tft.cfg.rot) {	//rotation has changed? Then...
		eeprom_update_word(&MINX, 0xffff);	//force new calibration
		tft_Setup();
	}
	action_cancel();
	return(0);
}

unsigned int action_cal () {
	int px1,px2,py1,py2,diff;
	const unsigned int b = 20;	//offset from border
	unsigned int x = b;
	unsigned int y = b;
	unsigned char i;

	TFT_fill(Black);

	for(i = 0; i < 4; i++) {
		Draw_H_Line(x-10,x+10,y,White);
		Draw_V_Line(x,y-10,y+10,White);
		while(isTouching());	//wait for release
		while(!isTouching());
		p = getRawPoint();
		Draw_H_Line(x-10,x+10,y,Green);
		Draw_V_Line(x,y-10,y+10,Green);
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
		switch (i) {
			case 0:
				px1 = p.x;
				py1 = p.y;
				x = tft.width-b;
				break;
			;;
			case 1:
				px2 = p.x;
				py1 = (py1 + p.y) / 2;	//middle
				y = tft.heigth-b;
				break;
			;;
			case 2:
				px2 = (px2 + p.x) / 2;	//middle
				py2 = p.y;
				x = b;
				break;
			;;
			case 3:
				px1 = (px1 + p.x) / 2;	//middle
				py2 = (py2 + p.y) / 2;	//middle
			;;
		}
		//print_I(10,50,1,White,Black,p.x);
		//print_I(40,50,1,White,Black,p.y);
		_delay_ms(500);		//debounce
	}

	while(isTouching());

/*	//print raw values, for debug only
	sprintf_P(atari_sector_buffer, PSTR("X1: %i, X2: %i, Y1: %i, Y2: %i"), px1, px2, py1, py2);
	print_str(20, tft.heigth/2-20, 1, White, Black, atari_sector_buffer);
*/
	//stretch values to whole screen size
        diff = (px2 - px1)/(tft.width-b*2.0)*tft.width;
        diff -= (px2 - px1);
        diff /= 2;
	px1 -= diff;
	px2 += diff;

	//same thing for Y
        diff = (py2 - py1)/(tft.heigth-b*2.0)*tft.heigth;
        diff -= (py2 - py1);
        diff /= 2;
	py1 -= diff;
	py2 += diff;
#pragma GCC diagnostic warning "-Wmaybe-uninitialized"

	//print results
	sprintf_P(atari_sector_buffer, PSTR("X1: %i, X2: %i, Y1: %i, Y2: %i"), px1, px2, py1, py2);
	print_str(20, tft.heigth/2, 1, White, Black, atari_sector_buffer);

	//wait for one more touch to continue
	while(!isTouching());

	//write it to EEPROM
	eeprom_update_word(&MINX, px1);
	eeprom_update_word(&MAXX, px2);
	eeprom_update_word(&MINY, py1);
	eeprom_update_word(&MAXY, py2);

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
	{">>>",10,240,50,30,Grey_Blue,Black,Light_White,&(struct b_flags){ROUND,1,0},press},
	{"Cfg",240-61,240,50,30,Grey,Black,Blue,&(struct b_flags){ROUND,1,0},action_cfg},
	{"",10,280,240-11,320-1,0,0,0,&(struct b_flags){0,0,0},debug_page}
};

const struct button PROGMEM buttons_file[] = {
	{"Top",164,45,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},list_files_top},
	{"Prev",164,85,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},list_files_rev},
	{"OK",164,125,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_ok},
	{"Exit",164,165,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_cancel},
	{"Next",164,205,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},list_files},
	{"Last",164,245,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},list_files_last},
	{"",15,45,150,240,Grey,Black,White,&(struct b_flags){ROUND,0,0},action_select}
};

//keep the order analog to struct tft.cfg, otherwise read/write function
//wouldn't work correctly!!!
const struct button PROGMEM buttons_cfg[] = {
	{"Rotate",15,45,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"Scroll",15,85,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"BootD1",15,125,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"1050",15,165,70,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"Blank",15,205,80,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	//!!leave this buttons at the end, then we can loop thru the previous!!
	{"SaveIm",15,245,90,30,Grey,Black,Light_Blue,&(struct b_flags){ROUND,1,0},action_change},
	{"Pokey",130,45,80,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_pokey},
	{"Save",164,125,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_save_cfg},
	{"Exit",164,165,60,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_cancel}
};

const struct button PROGMEM buttons_tape[] = {
	{"Start",15,165,80,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_tape_start},
	{"Pause",15,205,80,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_tape_pause},
	{"Turbo",144,165,80,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_tape_turbo},
	{"Exit",144,205,80,30,Grey,Black,White,&(struct b_flags){ROUND,1,0},action_cancel}
};

const struct button PROGMEM buttons_debug[] = {
	{"",0,0,240,280,Grey,Black,White,&(struct b_flags){ROUND,0,0},action_cancel}
};

struct page pages[] = {
    {main_page, buttons_main, sizeof(buttons_main)/sizeof(struct button)},
    {file_page, buttons_file, sizeof(buttons_file)/sizeof(struct button)},
    {config_page, buttons_cfg, sizeof(buttons_cfg)/sizeof(struct button)},
    {tape_page, buttons_tape, sizeof(buttons_tape)/sizeof(struct button)},
    {(void*)debug_page, buttons_debug, sizeof(buttons_debug)/sizeof(struct button)}
};

struct display tft = {240, 320, {PORTRAIT_2, 0}, pages};

void draw_Buttons () {
	struct button b;
	const struct button *bp;
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
			print_str_P(b.x+b.width-18,b.y+8,2,Dark_Grey,b.fg,PSTR("\x7f"));
			if(b.name[3] == '<')
				Draw_Line(b.x+b.width-18,b.y+8,b.x+b.width-10,b.y+8+16,Red);
		}
	}
}

unsigned int outx, outy;
unsigned char scroll;

void _outbox(const char *txt, char P) {

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

#define LINE_LENGTH 34

void outbox_multi (char *txt, unsigned char len) {
	char *lineptr = txt;
	char linebuf[LINE_LENGTH];

	while(len > LINE_LENGTH) {	//each line
		strncpy(linebuf, lineptr, LINE_LENGTH);
		_outbox(linebuf,0);
		len -= LINE_LENGTH;
		lineptr += LINE_LENGTH;
	}
	_outbox(lineptr,0);	//print the rest
}

void main_page () {

	outx = 20; outy = 284;

	TFT_off();	//turn off if returned to main to avoid scrollbak ef.
	TFT_scroll_init(outy,32,4);
	TFT_scroll(outy);
	TFT_on();
	TFT_fill(Black);

	//Header
	print_str_P(20, 10, 2, Orange, Black, system_name); //"SDrive-MAX"
	print_str_P(160, 18, 1, Orange, Black, system_version); //"by KBr V1.0"
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

	Draw_Rectangle(10,40,tft.width-11,280,1,SQUARE,window_bg,Black);
	Draw_Rectangle(10,40,tft.width-11,280,0,SQUARE,Grey,Black);
	Draw_Rectangle(11,41,tft.width-12,279,0,SQUARE,Grey,Black);
	//Draw_Rectangle(12,42,tft.width-13,278,0,SQUARE,Grey,Black);
	draw_Buttons();
	file_selected = -1;
	list_files();
}

void config_page () {
	const struct button *b;
	struct b_flags *flags;
	unsigned int i;

	Draw_Rectangle(10,40,tft.width-11,280,1,SQUARE,window_bg,Black);
	Draw_Rectangle(10,40,tft.width-11,280,0,SQUARE,Grey,Black);
	Draw_Rectangle(11,41,tft.width-12,279,0,SQUARE,Grey,Black);
	//Draw_Rectangle(12,42,tft.width-13,278,0,SQUARE,Grey,Black);
	print_pokeydiv();
	for(i = 0; i < tft.pages[actual_page].nbuttons-3; i++) {
		b = &tft.pages[actual_page].buttons[i];
		flags = pgm_read_ptr(&b->flags);
		flags->selected = (*(char*)&tft.cfg >> i) & 1;
		if(i == tft.pages[actual_page].nbuttons-4)
			flags->selected = 0;
	}
	draw_Buttons();
}

void tape_page () {
	Draw_Rectangle(5,100,tft.width-6,245,1,SQUARE,window_bg,Black);
	Draw_Rectangle(5,100,tft.width-6,245,0,SQUARE,Grey,Black);
	Draw_Rectangle(6,101,tft.width-7,244,0,SQUARE,Grey,Black);
	print_str_P(70, 105, 2, Orange, window_bg, PSTR("Tape-Emu"));
	Draw_H_Line(7,tft.width-8,122,Orange);
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
	unsigned char ts_found;

	ts_found = TS_init();
	TFT_init();
	*(char *)&tft.cfg = eeprom_read_byte(&cfg);
	TFT_set_rotation(tft.cfg.rot);
	id = TFT_getID();
	sprintf_P(atari_sector_buffer, PSTR("TFT-ID: %.04x"), id);
	print_str(20, 290, 2, White, Black, atari_sector_buffer);
	if(ts_found) {
		//check if touchscreen needs calibration
		if( (eeprom_read_word(&MINX) == 0xffff) || isTouching() ) {
			TFT_on();
			action_cal();
		}
	}
}

const struct button * check_Buttons() {
	const struct button *b;
	unsigned char i;
	unsigned int x,y,width,heigth;

	p = getPoint();
	//sprintf_P(atari_sector_buffer, PSTR("%i %i"), p.x, p.y);
	//print_str(20, 265, 1, White, Black, atari_sector_buffer);
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

