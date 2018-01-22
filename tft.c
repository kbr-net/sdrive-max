#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>
#include "avrlibdefs.h"         // global AVRLIB defines
#include "avrlibtypes.h"        // global AVRLIB types definitions
#include "display.h"
#include "logos.h"
#include "touchscreen.h"
#include "tft.h"
#include "fat.h"

extern unsigned char debug;
extern char atari_sector_buffer[];
extern struct FileInfoStruct FileInfo;
//extern struct GlobalSystemValues GS;

#define	atari_bg 0x257b

unsigned char actual_page = 0;
unsigned int next_file_idx = 0;
unsigned int nfiles = 0;
unsigned int file_selected = -1;
char path[13] = "/";
struct TSPoint p;
struct button *disk_button;

void main_page();
void file_page();
void config_page();
unsigned int debug_page();

struct display tft;

char EEMEM cfg = 0xff;	//config byte on eeprom, initial value is all activated

unsigned int action_b0 (struct button *b) {
	b->selected = 1;
	return(0);
}

unsigned int action_b1_4 (struct button *b) {

	if(p.x > 200) {	//file select page
		actual_page = 1;
		disk_button = b;	//save button for later select
		sei();
		tft.pages[actual_page].draw();
	}
	else if(p.x < 40) {	//deactivate
		return(-1);
	}
	else {	//set actual drive to
		b->selected = 1;
	}
	return(0);
}

unsigned int action_cancel () {
	//on file_page reset file index to same page
	if (actual_page == 1)
		next_file_idx -= 10;
	//on debug_page deactivate them
	else
		debug = 0;

	//and reset to main_page
	actual_page = 0;
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
		if(fatGetDirEntry(i,0)) {
			if(FileInfo.Attr & ATTR_DIRECTORY)	//other color
				col = 0x07ff;
			else
				col = Green;
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
	actual_page = 0;
	next_file_idx -= 10;
	tft.pages[actual_page].draw();
	return(file_selected);
}

unsigned int action_cfg () {
	actual_page = 2;
	tft.pages[actual_page].draw();
	return(0);
}

unsigned int action_change (struct button *b) {
	//invert selection
	b->selected = ~b->selected;
	draw_Buttons();
	return(0);
}

unsigned int action_save () {
	tft.cfg.rot = tft.pages[actual_page].buttons[0].selected;
	tft.cfg.scroll = tft.pages[actual_page].buttons[1].selected;
	eeprom_write_byte(&cfg, *(char *)&tft.cfg);
	TFT_set_rotation(tft.cfg.rot);
	action_cancel();
	return(0);
}

unsigned int press () {
	return(0);
}

struct button buttons_main[] = {
	//name, x, y, width, heigth, fg-col, bg-col, font-col, type, act, sel
	{"D0:",10,200,50,30,Grey,Black,Black,ROUND,1,1,action_b0},
	{"D1:<empty>",10,40,240-21,30,Grey,Black,Black,ROUND,1,0,action_b1_4},
	{"D2:<empty>",10,80,240-21,30,Grey,Black,Black,ROUND,1,0,action_b1_4},
	{"D3:<empty>",10,120,240-21,30,Grey,Black,Black,ROUND,1,0,action_b1_4},
	{"D4:<empty>",10,160,240-21,30,Grey,Black,Black,ROUND,1,0,action_b1_4},
	{"New",240-61,200,50,30,Grey,Black,Green,ROUND,1,0,press},
	{"Cfg",240-61,240,50,30,Grey,Black,Blue,ROUND,1,0,action_cfg},
	{"Outbox",10,280,240-11,320-1,0,0,0,0,0,0,debug_page}
};

struct button buttons_file[] = {
	{"Top",164,45,60,30,Grey,Black,White,ROUND,1,0,list_files_top},
	{"Prev",164,85,60,30,Grey,Black,White,ROUND,1,0,list_files_rev},
	{"OK",164,125,60,30,Grey,Black,White,ROUND,1,0,action_ok},
	{"Exit",164,165,60,30,Grey,Black,White,ROUND,1,0,action_cancel},
	{"Next",164,205,60,30,Grey,Black,White,ROUND,1,0,list_files},
	{"Last",164,245,60,30,Grey,Black,White,ROUND,1,0,list_files_last},
	{"File",15,45,80,240,Grey,Black,White,ROUND,0,0,action_select}
};

struct button buttons_cfg[] = {
	{"Rotate",15,45,90,30,Grey,Black,Light_Blue,ROUND,1,0,action_change},
	{"Scroll",15,85,90,30,Grey,Black,Light_Blue,ROUND,1,0,action_change},
/* all 8 needs too much RAM!!!
	{"empty3",15,125,90,30,Grey,Black,Light_Blue,ROUND,1,0,action_change},
	{"empty4",15,165,90,30,Grey,Black,Light_Blue,ROUND,1,0,action_change},
	{"empty5",15,205,90,30,Grey,Black,Light_Blue,ROUND,1,0,action_change},
	{"empty6",15,245,90,30,Grey,Black,Light_Blue,ROUND,1,0,action_change},
	{"empty7",115,125,90,30,Grey,Black,Light_Blue,ROUND,1,0,action_change},
	{"empty8",115,165,90,30,Grey,Black,Light_Blue,ROUND,1,0,action_change},
*/
	{"Save",164,125,60,30,Grey,Black,White,ROUND,1,0,action_save},
	{"Exit",164,165,60,30,Grey,Black,White,ROUND,1,0,action_cancel}
};

struct button buttons_debug[] = {
	{"Back",0,0,240,280,Grey,Black,White,ROUND,0,0,action_cancel}
};

struct page pages[] = {
    {main_page, buttons_main, sizeof(buttons_main)/sizeof(struct button)},
    {file_page, buttons_file, sizeof(buttons_file)/sizeof(struct button)},
    {config_page, buttons_cfg, sizeof(buttons_cfg)/sizeof(struct button)},
    {debug_page, buttons_debug, sizeof(buttons_debug)/sizeof(struct button)}
};

struct display tft = {240, 320, {PORTRAIT_2, 0}, pages};

void draw_Buttons () {
	struct button b;
	unsigned char i;

	for(i = 0; i < tft.pages[actual_page].nbuttons; i++) {
		b = tft.pages[actual_page].buttons[i];
		if(!b.active)
			continue;
		if(b.selected) {
			b.fg = Blue;
			b.fc = Yellow;
		}
		Draw_Rectangle(b.x,b.y,b.x+b.width,b.y+b.heigth,1,b.type,b.fg,b.bg);
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
			if (actual_page == 3)
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

	TFT_off();
	TFT_scroll_init(outy,32,4);
	TFT_scroll(outy);
	TFT_on();
	TFT_fill(Black);

	//Header
	print_str_P(20, 10, 2, Orange, Black, PSTR("SDrive-MAX"));
	print_str_P(160, 18, 1, Orange, Black, PSTR("by KBr V0.4"));
	Draw_H_Line(0,tft.width,30,Orange);

	draw_Buttons();

	//Output Box
	Draw_Rectangle(10,280,tft.width-11,tft.heigth-1,1,SQUARE,atari_bg,Black);
	scroll = 0;
	set_text_pos(outx, outy);
	outbox_P(PSTR("READY"));
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

	Draw_Rectangle(10,40,tft.width-11,280,1,SQUARE,Light_Grey,Black);
	Draw_Rectangle(10,40,tft.width-11,280,0,SQUARE,Grey,Black);
	Draw_Rectangle(11,41,tft.width-12,279,0,SQUARE,Grey,Black);
	//Draw_Rectangle(12,42,tft.width-13,278,0,SQUARE,Grey,Black);
	tft.pages[actual_page].buttons[0].selected = tft.cfg.rot;
	tft.pages[actual_page].buttons[1].selected = tft.cfg.scroll;
	draw_Buttons();
}

unsigned int debug_page () {

	TFT_fill(atari_bg);

	outx = 10; outy = 10;
	TFT_scroll_init(outy,304,6);
	TFT_scroll(outy);
	scroll = 0;
	set_text_pos(outx, outy);
	outbox_P(PSTR("READY"));
	print_char(10,18,1,White,atari_bg,0x80);

	debug = 1;
	actual_page = 3;
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
}

struct button * check_Buttons() {
	struct button *b;
	unsigned char i;

	p = getPoint();
	//print_I(30,292,1,White,atari_bg,p.x);
	//print_I(60,292,1,White,atari_bg,p.y);
	for(i = 0; i < tft.pages[actual_page].nbuttons; i++) {
		b = &tft.pages[actual_page].buttons[i];
		if(p.x > b->x && p.x < b->x+b->width &&
		   p.y > b->y && p.y < b->y+b->heigth)
			return(b);
	}
	return(0);
}

/* Testcode
int main () {
	atari_bg = RGB565_converter(0x20,0xaf,0xde);

	TFT_init();
	TFT_set_rotation(tft.cfg.rot);
	//file_page();
	while(1) {
		tft.pages[actual_page].draw();
		while(!isTouching());
		p = getPoint();
		print_I(30,292,1,White,atari_bg,p.x);
		print_I(60,292,1,White,atari_bg,p.y);
		_delay_ms(50);
		struct button *b;
		unsigned char i;
		//check buttons pressed
		for(i = 0; i < nbuttons; i++) {
			b = &tft.pages[actual_page].buttons[i];
			if(p.x > b->x && p.x < b->x+b->width && p.y > b->y && p.y < b->y+b->heigth) {
				//clear all selections
				for(i = 0; i < nbuttons; i++)
					tft.pages[actual_page].buttons[i].selected = 0;
				//select the new button
				b->selected = 1;
				draw_Buttons();
				b->pressed();	//do action
				break;
			}
			else
				actual_page = 0;
		}
	}
	return(0);
}
*/
