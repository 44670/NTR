#ifndef MAIN_H
#define MAIN_H

#include "3dstypes.h"
//#include "..\..\BootNTR\source\ntr_config.h"

#define NTR_MEMMODE_DEFAULT (0)
#define NTR_MEMMODE_BASE (3)

typedef struct NTR_CONFIG {
	//NTR related configuration information.
	u32 bootNTRVersion;
	u32 isNew3DS;
	u32 firmVersion;							//3DS Firmware Version. (o3DS and n3DS firmware, not NTR version!)
	u32 InterProcessDmaFinishState;				//Interprocess Direct Memory Access Finish State.
	u32 fsUserHandle;							//File System Userland Application Process Handle, presumably?
	u8 ntrFilePath[32];							//File path string, up to 32 characters long, null-terminating?

	//ARMv11 payload
	u32 arm11BinStart;							//Start address.
	u32 arm11BinSize;							//Payload size.

	//Input/Output
	u32 IoBasePad;								//Pad, as in the touchpad?
	u32 IoBaseLcd;
	u32 IoBasePdc;								//I/O Base pointer to the device context.

	//Process Manager
	u32 PMSvcRunAddr;							//Process manager Service Run address.
	u32 PMPid;									//Process manager Process ID.
	
	//Home Menu
	u32 HomeMenuPid;
	u32 HomeMenuVersion;

	//NOTE(wedr): CSND:FlushDataCache and DSP:FlushDataCache both calls on this svc function. (CSND: CSound/CTRSound, DSP: Digital Signal Processor)
	//CSND flushes the data within LINEAR virtual memory. DSP flushes within the application GSP (graphics system processor?) heap.
	u32 HomeMenuInjectAddr;						//Points to the svcFlushProcessDataCache function, according to Reisyukaku.
	
	//Home Screen
	u32 HomeFSReadAddr;							//Home File System Read Address.
	u32 HomeFSUHandleAddr;						//Home File System Userland Handle address.  (Based on NS:U and APT:U, this is a guess. APT:A may be Applet:Admin, APT:S may be Applet:Service.)
	u32 HomeCardUpdateInitAddr;
	u32 HomeAptStartAppletAddr;					//Home Applet Start Applet Address.

	//Control Memory Patch
	u32 ControlMemoryPatchAddr1;
	u32 ControlMemoryPatchAddr2;

	//Kernel Process
	u32 KProcessHandleDataOffset;
	u32 KProcessPIDOffset;
	u32 KProcessCodesetOffset;

	//Kernel related
	u32 KernelFreeSpaceAddr_Optional;

	//MMU Hack related
	u32 KMMUHaxAddr;					//Kernal MMU Hack Address
	u32 KMMUHaxSize;					//Kernel MMU Hack Size

	//Debugging purposes
	u32 ShowDbgFunc;
	u32 memMode;
} NTR_CONFIG;

void createpad(void *counter, void *keyY, void *filename, u32 megabytes, u8 padnum);
int main();

extern u32 IoBasePad;
extern u32 IoBaseLcd;
extern u32 IoBasePdc;

extern u32 ShowDbgFunc;

extern u32 KProcessCodesetOffset;
extern u32 KProcessPIDOffset ;
extern u32 KProcessHandleDataOffset;
extern u32 HomeAptStartAppletAddr;


extern NTR_CONFIG* ntrConfig;
 
#endif

