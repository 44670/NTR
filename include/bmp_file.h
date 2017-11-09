#ifndef BMP_FILE_H
#define BMP_FILE_H
struct img_file {
	
	int width;
	int height;
	unsigned char data[];

};

typedef struct img_file bmp_file;
#endif