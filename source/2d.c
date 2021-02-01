#include "2d.h"
#include "font.h"
#include "memory.h"
#include "string.h"



void paint_square(int x, int y, char r, char g, char b, int w, int h, int screen){
  int x1, y1;

  for (x1 = x; x1 < x+w; x1++){
    for (y1 = y; y1 < y+h; y1++){
        paint_pixel(x1,y1,r,g,b,screen);   
    }   
  }
}

void paint_pixel(u32 x, u32 y, char r, char g, char b, int screen){
	if (x >= BOTTOM_WIDTH) {
		return;
	}
	if (y >= BOTTOM_HEIGHT) {
		return;
	}
  int coord = 720*x+720-(y*3);
  write_color(coord+screen,r,g,b);
}

void blank(int x, int y, int xs, int ys){
  paint_square(x,y,255,255,255,xs,ys,BOTTOM_FRAME1);
  paint_square(x,y,255,255,255,xs,ys,BOTTOM_FRAME2);
}

void paint_letter(u8 letter, int x, int y, char r, char g, char b, int screen){

	int i;
	int k;
	int c;
	unsigned char mask;
	
	//Unused.
	//unsigned char* _letter;

	unsigned char l;
	if ((letter < 32) || (letter > 127)) {
		letter = '?';
	}
	c = (letter - 32) * 12;

	for (i = 0; i < 12; i++){
		mask = 0b10000000;
		l = font[i + c];
		for (k = 0; k < 8; k++){
			if ((mask >> k) & l){
				paint_pixel(k + x, i + y, r, g, b, screen);
			}
			else {
				paint_pixel(k + x, i + y, 255, 255, 255, screen);
			}
		}
	}
}

void paint_word(char* word, int x,int y, char r, char g, char b, int screen){
    int tmp_x =x;
    int i;
    int line = 0;

    for (i = 0; i <strlen(word); i++){
     
      if (tmp_x+8 > BOTTOM_WIDTH) {
        line++;
        tmp_x = x;
      }
      paint_letter(word[i],tmp_x,y+(line*8),r,g,b, screen);

      tmp_x = tmp_x+8;
    }

}

