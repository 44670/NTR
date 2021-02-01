#include "global.h"

void mystrcat(u8* a, u8* b){
	while(*a != 0) {
		a++;
	}
	while(*b != 0) {
		*a = *b;
		a++;
		b++;
	}
	*a = 0;
}

void  myitoa(u32 a, u8* b){
	u8 i = 0;
	u8 t;
	for (i = 0; i < 8; i++) {
		t = ((a & 0xf0000000) >> 28) + '0';
		a = a << 4;
		if (t > '9') {
			t += 'A' - '9' - 1;
		}
		*b = t;
		b++;
	}
	*b = 0;
}

void dbg(u8* key, u32 value) {
	u8 buf[200], buf2[20];
	u32 t = 0;
	buf[0] = 0;
	mystrcat(buf, (u8*) "/dbg");
	mystrcat(buf, key);
	myitoa(value, buf2);
	mystrcat(buf, buf2);
	FS_path testPath = (FS_path){PATH_CHAR, strlen((const char*) buf) + 1, buf};
	FSUSER_OpenFileDirectly(fsUserHandle, &t, sdmcArchive, testPath, 7, 0);
	if (t != 0) {
		FSFILE_Close(t);
	}
}
