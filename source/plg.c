#include "global.h"
#include <ctr/SOC.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_PLUGIN_ENTRY 64
u32 pluginEntry[MAX_PLUGIN_ENTRY][3];
u32 pluginEntryCount = 0;
u32 lastGamePluginMenuSelect = 0;


RT_HOOK	svc_RunHook;
RT_HOOK runAppletHook;
typedef  u32(*svc_RunTypeDef) (u32 hProcess, u32* startInfo);
typedef u32(*runAppletTypeDef) (u32 a1, u32 a2, u32 a3, u32 a4, u32 a5);
u32 lastTid[2] = { 0, 0 };

u32 plgPoolStart = 0x07000000;
u32 plgPoolEnd = 0;
u32 plgMemoryPoolStart = 0x06200000;
u32 plgMemoryPoolEnd = 0;
FS_archive plgSdmcArchive;
PLGLOADER_INFO *g_plgInfo;
u32 plgLoadStart;
u32 plgNextLoadAddr;
u32 arm11BinStart = 0;
u32 arm11BinSize = 0;
u32 arm11BinProcess = 0xffff8001;
extern u32 NTRMenuHotkey;
extern u32 ScreenshotHotkey;
GAME_PLUGIN_MENU plgCurrentGamePluginMenu;

translateTypeDef plgTranslateCallback = 0;
drawStringTypeDef plgDrawStringCallback = 0;


void plgInitScreenOverlay();
void nsInit();
u32 isInDebugMode();

int isVRAMAccessible = 0;
int plgOverlayStatus = 0;

u32 plgRegisterCallback(u32 type, void* callback, u32 param0) {
	if (type == CALLBACK_OVERLAY) {
		plgInitScreenOverlay();
		if (plgOverlayStatus != 1) {
			return -1;
		}
		if (pluginEntryCount >= MAX_PLUGIN_ENTRY) {
			return -1;
		}
		pluginEntry[pluginEntryCount][0] = CALLBACK_OVERLAY;
		pluginEntry[pluginEntryCount][1] = (u32)"overlay";
		pluginEntry[pluginEntryCount][2] = (u32) callback;
		pluginEntryCount++;
		return 0;
	}

	return -1;
}

u32 plgRequestMemory(u32 size) {
	return plgRequestMemorySpecifyRegion(size, 1);
}


u32 plgRequestMemorySpecifyRegion(u32 size, int sysRegion) {
	u32 ret, outAddr, addr;

	if ((size & 0xfff) != 0) {
		return 0;
	}
	if (!plgMemoryPoolEnd) {
		plgMemoryPoolEnd = plgMemoryPoolStart;
	}
	addr = plgMemoryPoolEnd;
	if (sysRegion) {
		ret = controlMemoryInSysRegion(&outAddr, addr, addr, size, NS_DEFAULT_MEM_REGION + 3, 3);
	}
	else {
		ret = svc_controlMemory(&outAddr, addr, addr, size, NS_DEFAULT_MEM_REGION + 3, 3);
	}
	if (ret != 0) {
		return 0;
	}
	plgMemoryPoolEnd += size;
	return addr;
}

u32 plgGetSharedServiceHandle(char* servName, u32* handle) {
	if (strcmp(servName, "fs:USER") == 0) {
		*handle = fsUserHandle;
	}
	return 0;
}

u32 plgGetIoBase(u32 IoBase) {
	if (IoBase == IO_BASE_LCD) {
		return IoBaseLcd;
	}
	if (IoBase == IO_BASE_PAD) {
		return IoBasePad;
	}
	if (IoBase == IO_BASE_PDC) {
		return IoBasePdc;
	}
	if (IoBase == IO_BASE_GSPHEAP) {
		return 0x14000000;
	}
	if (IoBase == IO_BASE_HOME_MENU_PID) {
		return ntrConfig->HomeMenuPid;
	}
	if (IoBase == VALUE_CURRENT_LANGUAGE) {
		return g_plgInfo->currentLanguage;
	}
	return 0;
}

void plgSetHotkeyUi() {
	u8* entries[8];
	u32 r;
	entries[0] = (u8*) "NTR Menu: X+Y";
	entries[1] = (u8*) "NTR Menu: L+START";
	entries[2] = (u8*) "Screenshot: disabled";
	entries[3] = (u8*) "Screenshot: SELECT+START";
	entries[4] = (u8*) "Screenshot: L+R";
	while (1) {
		r = showMenu(((u8*) NTR_CFW_VERSION), 5, entries);
		if (r == -1) {
			break;
		}
		if (r == 0) {
			NTRMenuHotkey = 0xC00;
			break;
		}
		if (r == 1) {
			NTRMenuHotkey = PAD_L | PAD_START;
			break;
		}
		if (r == 2) {
			ScreenshotHotkey = 0;
			break;
		}
		if (r == 3) {
			ScreenshotHotkey = PAD_SELECT | PAD_START;
			break;
		}
		if (r == 4) {
			ScreenshotHotkey = PAD_L | PAD_R;
			break;
		}
	}

}

int plgTryLoadGamePluginMenu() {
	u32 gamePid = g_plgInfo->gamePluginPid;
	u32 gamePluginMenuAddr = g_plgInfo->gamePluginMenuAddr;
	if (gamePid == 0) {
		return 1;
	}
	if (gamePluginMenuAddr == 0) {
		return 1;
	}
	u32 ret = 0;
	u32 hProcess;
	ret = svc_openProcess(&hProcess, gamePid);
	if (ret != 0) {
		return ret;
	}
	ret = copyRemoteMemory(CURRENT_PROCESS_HANDLE, &plgCurrentGamePluginMenu, hProcess, (void*) gamePluginMenuAddr, sizeof(GAME_PLUGIN_MENU));
	if (ret != 0) {
		goto final;
	}
	final:
	svc_closeHandle(hProcess);
	return ret;
}

int plgUpdateGamePluginMenuState() {
	u32 gamePid = g_plgInfo->gamePluginPid;
	u32 gamePluginMenuAddr = g_plgInfo->gamePluginMenuAddr;
	if (gamePid == 0) {
		return 1;
	}
	if (gamePluginMenuAddr == 0) {
		return 1;
	}
	u32 ret = 0;
	u32 hProcess;
	ret = svc_openProcess(&hProcess, gamePid);
	if (ret != 0) {
		return ret;
	}
	ret = copyRemoteMemory(hProcess, (void*) gamePluginMenuAddr, CURRENT_PROCESS_HANDLE, &plgCurrentGamePluginMenu, sizeof(plgCurrentGamePluginMenu.state));
	if (ret != 0) {
		goto final;
	}
	final:
	svc_closeHandle(hProcess);
	return ret;
}

void plgShowGamePluginMenu() {
	u8* entries[70], ret;
	u8* description[70];
	u8* buf;

	int i, j;
	while (1) {
		if (plgCurrentGamePluginMenu.count <= 0) {
			return;
		}
		for (i = 0; i < plgCurrentGamePluginMenu.count; i++) {
			buf = &(plgCurrentGamePluginMenu.buf[plgCurrentGamePluginMenu.offsetInBuffer[i]]);
			description[i] = 0;
			entries[i] = buf;
			for (j = 0; j < strlen((const char*) buf); j++) {
				if (buf[j] == 0xff) {
					buf[j] = 0;
					description[i] = &(buf[j + 1]);
					break;
				}
			}
		}
		int r;
		r = showMenuEx((u8*) plgTranslate("Game Plugin Config"), plgCurrentGamePluginMenu.count, entries, description,
			lastGamePluginMenuSelect);
		if (r == -1) {
			return;
		}
		plgCurrentGamePluginMenu.state[r] = 1;
		lastGamePluginMenuSelect = r;

		releaseVideo();
		plgUpdateGamePluginMenuState();
		svc_sleepThread(500000000);
		ret = plgTryLoadGamePluginMenu();
		acquireVideo();

		if (ret != 0) {
			return;
		}
	}
}

int plgTryUpdateConfig() {
	u32 gamePid = g_plgInfo->gamePluginPid;
	u32 configStart = (u32) &(g_plgInfo->nightShiftLevel);
	u32 configSize = 4;

	if (gamePid == 0) {
		return 1;
	}
	u32 hProcess;
	u32 ret = svc_openProcess(&hProcess, gamePid);
	if (ret != 0) {
		return ret;
	}
	ret = copyRemoteMemory(hProcess, (void*) configStart, CURRENT_PROCESS_HANDLE, (void*) configStart, configSize);
	if (ret != 0) {
		goto final;
	}
	final:
	svc_closeHandle(hProcess);
	return ret;
}

void plgShowMainMenu() {
	typedef u32(*funcType)();
	u8* entries[70];
	u32 entid[70];
	u32 pos = 0, i;
	u32 mainEntries = 3;
	//Unused
	//u8 requestUpdateState = 0;

	debounceKey();
	entries[0] = (u8*) plgTranslate("Process Manager");
	entries[1] = (u8*) plgTranslate("Enable Debugger");
	entries[2] = (u8*) plgTranslate("Set Hotkey");


	pos = mainEntries;
	for (i = 0; i < pluginEntryCount; i++) {
		if (pluginEntry[i][0] == 1) {
			entries[pos] = (u8*) pluginEntry[i][1];
			entid[pos] = i;
			pos++;
		}
	}
	if (plgTryLoadGamePluginMenu() == 0) {
		entries[pos] = (u8*) plgTranslate("Game Plugin");
		pos++;
	}
	acquireVideo();
	//Unused
	//u8 buf[200];
	while (1) {
		s32 r;
		r = showMenu(((u8*) NTR_CFW_VERSION), pos, entries);
		if (r == -1) {
			break;
		}
		if (r == 0) {
			processManager();
		}
		if (r == 1) {
			if (g_nsConfig->hSOCU) {
				showMsg((u8*) plgTranslate("Debugger has already been enabled."));
				break;
			}
			else {
				nsInit((u32) NULL);
				break;
			}
		}
		if (r == 2) {
			plgSetHotkeyUi();
		}
		if (r >= mainEntries) {
			if (r - mainEntries >= pluginEntryCount) {
				plgShowGamePluginMenu();
				break;
			}
			else {
				u32 ret = ((funcType) ((void*) (pluginEntry[entid[r]][2])))();
				if (ret) {
					break;
				}
			}
		}
	}

	releaseVideo();
	debounceKey();
	delayUi();
}

u32 plgRegisterMenuEntry(u32 catalog, char* title, void* callback) {
	if (pluginEntryCount >= MAX_PLUGIN_ENTRY) {
		return -1;
	}
	pluginEntry[pluginEntryCount][0] = catalog;
	pluginEntry[pluginEntryCount][1] = (u32) title;
	pluginEntry[pluginEntryCount][2] = (u32) callback;
	pluginEntryCount++;
	return 0;
}

u32 plgEnsurePoolEnd(u32 end) {
	if (plgPoolEnd == 0) {
		plgPoolEnd = plgPoolStart;
	}
	u32 ret;
	u32 outAddr;
	u32 addr = plgPoolEnd;
	u32 size = end - plgPoolEnd;

	if (end <= plgPoolEnd) {
		return 0;
	}
	nsDbgPrint((const char*) "expand pool addr: %08x, size: %08x\n", addr, size);
	ret = controlMemoryInSysRegion(&outAddr, addr, addr, size, NS_DEFAULT_MEM_REGION + 3, 3);
	if (ret != 0) {
		if (rtCheckRemoteMemoryRegionSafeForWrite(0xffff8001, addr, size) != 0) {
			nsDbgPrint((const char*) "alloc plg memory failed: %08x\n", ret);
			return ret;
		}
	}
	plgPoolEnd = end;
	return 0;
}


void tryInitFS() {
	u32 ret;
	if (fsUserHandle) {
		return;
	}
	if (!srvHandle) {
		initSrv();
	}
	srv_getServiceHandle(0, &fsUserHandle, "fs:REG");
	ret = FSUSER_Initialize(fsUserHandle);
	if (ret != 0) {
		nsDbgPrint((const char*) "FSUSER_Initialize failed: %08x\n", ret);
	}
	nsDbgPrint((const char*) "fsUserHandle: %08x\n", fsUserHandle);
}

u32 plgLoadPluginToRemoteProcess(u32 hProcess) {
	static Handle hMenuProcess = 0;
	u32 ret, i;
	u32 base, totalSize;
	PLGLOADER_INFO plgInfo, targetPlgInfo;
	NS_CONFIG cfg;
	u32 procTid[2];




	if (hMenuProcess == 0) {
		ret = svc_openProcess(&hMenuProcess, ntrConfig->HomeMenuPid);
		if (ret != 0) {
			hMenuProcess = 0;
			nsDbgPrint((const char*) "open menu process failed: %08x\n", ret);
			return ret;
		}
	}
	nsDbgPrint((const char*) "hMenuProcess:%08x\n", hMenuProcess);

	memset(&cfg, 0, sizeof(NS_CONFIG));
	ret = copyRemoteMemory(0xffff8001, &plgInfo, hMenuProcess, (void*) plgPoolStart, sizeof(PLGLOADER_INFO));
	if (ret != 0) {
		nsDbgPrint((const char*) "load plginfo failed:%08x\n", ret);
		return ret;
	}

	getProcessTIDByHandle(hProcess, procTid);


	nsDbgPrint((const char*) "procTid: %08x%08x\n", procTid[1], procTid[0]);
	if (!((procTid[0] == plgInfo.tid[0]) && (procTid[1] == plgInfo.tid[1]))) {
		nsDbgPrint((const char*) "tid mismatch\n");
		return -1;
	}
	u32 pid = 0;
	svc_getProcessId(&pid, hProcess);
	plgInfo.gamePluginPid = pid;
	copyRemoteMemory(hMenuProcess, (void*) plgPoolStart, 0xffff8001, &plgInfo, sizeof(PLGLOADER_INFO));

	if (plgInfo.plgCount == 0) {
		if (!isInDebugMode()) {
			if (plgInfo.nightShiftLevel == 0) {
				// no plugin loaded and not debug mode, skipping
				return -1;
			}
		}
	}

	arm11BinStart = plgInfo.arm11BinStart;
	arm11BinSize = plgInfo.arm11BinSize;
	arm11BinProcess = hMenuProcess;
	cfg.startupCommand = NS_STARTCMD_INJECTGAME;

	base = plgPoolStart;
	memcpy(&targetPlgInfo, &plgInfo, sizeof(PLGLOADER_INFO));
	totalSize = rtAlignToPageSize(sizeof(PLGLOADER_INFO));
	base += totalSize;
	for (i = 0; i < plgInfo.plgCount; i++) {
		u32 size = plgInfo.plgSize[i];
		targetPlgInfo.plgBufferPtr[i] = base;
		totalSize += size;
		base += size;
	}
	//Unused
	//u32 newKP = 0;
	//u32 oldKP = 0;
	//u32 hPMProcess = getCurrentProcessHandle();

	ret = mapRemoteMemoryInSysRegion(hProcess, plgPoolStart, totalSize);

	if (ret != 0) {
		nsDbgPrint((const char*) "alloc plugin memory failed: %08x\n", ret);
		return ret;
	}
	ret = rtCheckRemoteMemoryRegionSafeForWrite(hProcess, plgPoolStart, totalSize);
	if (ret != 0) {
		nsDbgPrint((const char*) "rwx failed: %08x\n", ret);
		return ret;
	}
	ret = copyRemoteMemory(hProcess, (void*) plgPoolStart, 0xffff8001, &targetPlgInfo, sizeof(PLGLOADER_INFO));
	if (ret != 0) {
		nsDbgPrint((const char*) "copy plginfo failed: %08x\n", ret);
		return ret;
	}
	for (i = 0; i < targetPlgInfo.plgCount; i++) {
		ret = copyRemoteMemory(hProcess, (void*) targetPlgInfo.plgBufferPtr[i], hMenuProcess, (void*) plgInfo.plgBufferPtr[i],
			targetPlgInfo.plgSize[i]);
		if (ret != 0) {
			nsDbgPrint((const char*) "load plg failed: %08x\n", ret);
			return ret;
		}
	}

	ret = nsAttachProcess(hProcess, 0x00100000, &cfg, 1);
	if (ret != 0) {
		nsDbgPrint((const char*) "attach process failed: %08x\n", ret);
		return ret;
	}
	return 0;
}

u32 svc_RunCallback(Handle hProcess, u32* startInfo) {
	//Unused
	//u32 ret;

	plgLoadPluginToRemoteProcess(hProcess);
	/*
	ret = copyRemoteMemory(hProcess, 0x00100000, 0xffff8001, buf,  sizeof(buf));
	nsDbgPrint((const char*) "copyRemoteMemory ret: %08x\n", ret);*/

	//Unused label
	//final:

	return ((svc_RunTypeDef) ((void*) (svc_RunHook.callCode)))(hProcess, startInfo);
}



void initFromInjectPM() {
	//Unused
	//u32 ret;

	rtInitHook(&svc_RunHook, ntrConfig->PMSvcRunAddr, (u32) svc_RunCallback);
	rtEnableHook(&svc_RunHook);
}

u32 plgListPlugins(u32* entries, u8* buf, u8* path) {
	//Unused. This should be #defined constant.
	//u32 maxPlugins = 32;
	u32 off = 0;
	u16 entry[0x228];
	u32 i = 0;
	u32 entryCount = 0;
	u32 dirHandle;
	u32 ret = 0;
	FS_path dirPath = (FS_path) { PATH_CHAR, strlen((const char*) path) + 1, path };

	ret = FSUSER_OpenDirectory(fsUserHandle, &dirHandle, plgSdmcArchive, dirPath);
	if (ret != 0) {
		nsDbgPrint((const char*) "FSUSER_OpenDirectory failed, ret=%08x", ret);
		return 0;
	}
	while (1) {
		u32 nread = 0;
		FSDIR_Read(dirHandle, &nread, 1, (u16*) entry);
		if (!nread) break;
		entries[entryCount] = (u32) &buf[off];
		for (i = 0; i < 0x228; i++) {
			u16 t = entry[i];
			if (t == 0) {
				break;
			}
			buf[off] = (u8) t;
			off += 1;
		}
		buf[off] = 0;
		off += 1;
		entryCount += 1;
	}
	FSDIR_Close(dirHandle);
	return entryCount;
}

u32 plgStartPluginLoad() {
	g_plgInfo->plgCount = 0;
	plgNextLoadAddr = plgLoadStart;
	g_plgInfo->arm11BinStart = arm11BinStart;
	g_plgInfo->arm11BinSize = arm11BinSize;
	return 0;
}


char* plgTranslate(char* origText) {
	if (plgTranslateCallback) {
		char* ret = plgTranslateCallback(origText);
		if (ret) {
			return ret;
		}
	}
	return origText;
}
u32 plgSetValue(u32 index, u32 value) {
	if (index == VALUE_CURRENT_LANGUAGE) {
		g_plgInfo->currentLanguage = value;
	}
	if (index == VALUE_DRAWSTRING_CALLBACK) {
		plgDrawStringCallback = (void*) value;
	}
	if (index == VALUE_TRANSLATE_CALLBACK) {
		plgTranslateCallback = (void*) value;
	}
	return 0;
}

int plgIsValidPluginFile(u8* filename) {
	int len;
	len = strlen((const char*) filename);
	if (len < 5) {
		return 0;
	}
	if (filename[0] == '.') {
		return 0;
	}
	if (strcmp(((const char*) filename) + len - 4, ".plg") != 0) {
		return 0;
	}
	return 1;
}

u32 plgLoadPluginsFromDirectory(u8* dir) {
	u8 path[200], pluginPath[200];
	u32 entries[MAX_PLUGIN_COUNT];
	u8 buf[0x1000];
	u32 cnt, i, ret;
	u32 bufSize;
	u32 validCount = 0;

	xsprintf((char*) path, "/plugin/%s", dir);
	cnt = plgListPlugins(entries, buf, path);

	for (i = 0; i < cnt; i++) {
		if (!plgIsValidPluginFile((u8*) entries[i])) {
			continue;
		}
		xsprintf((char*) pluginPath, "%s/%s", path, entries[i]);

		bufSize = rtAlignToPageSize(rtGetFileSize(pluginPath));
		nsDbgPrint((const char*) "loading plugin: %s, size: %08x, addr: %08x\n", pluginPath, bufSize, plgNextLoadAddr);
		ret = plgEnsurePoolEnd(plgNextLoadAddr + bufSize);
		if (ret != 0) {
			nsDbgPrint((const char*) "alloc plugin memory failed\n");
			continue;
		}
		ret = rtLoadFileToBuffer(pluginPath, (u32*) plgNextLoadAddr, bufSize);
		if (ret == 0) {
			nsDbgPrint((const char*) "rtLoadFileToBuffer failed\n");
			continue;
		}
		g_plgInfo->plgBufferPtr[g_plgInfo->plgCount] = plgNextLoadAddr;
		g_plgInfo->plgSize[g_plgInfo->plgCount] = bufSize;
		(g_plgInfo->plgCount) += 1;
		plgNextLoadAddr += bufSize;
		validCount += 1;
	}
	return validCount;

}

RT_HOOK	aptPrepareToStartApplicationHook;
typedef u32(*aptPrepareToStartApplicationTypeDef) (u32 a1, u32 a2, u32 a3);

u32 aptPrepareToStartApplicationCallback(u32 a1, u32 a2, u32 a3) {
	volatile uintptr_t temp = a1;
	u32* tid = (u32*) temp;
	nsDbgPrint((const char*) "starting app: %08x%08x\n", tid[1], tid[0]);
	plgStartPluginLoad();
	plgLoadPluginsFromDirectory((u8*) "game");
	u8 buf[32];
	xsprintf((char*) buf, "%08x%08x", tid[1], tid[0]);
	plgLoadPluginsFromDirectory(buf);
	g_plgInfo->tid[0] = tid[0];
	g_plgInfo->tid[1] = tid[1];
	g_plgInfo->gamePluginPid = 0;
	g_plgInfo->gamePluginMenuAddr = 0;
	lastGamePluginMenuSelect = 0;

	return ((aptPrepareToStartApplicationTypeDef) ((void*) (aptPrepareToStartApplicationHook.callCode)))(a1, a2, a3);
}

void injectPM() {
	NS_CONFIG cfg;
	u32 pid = ntrConfig->PMPid, ret;
	Handle hProcess;
	u32 remotePC = ntrConfig->PMSvcRunAddr;

	memset(&cfg, 0, sizeof(NS_CONFIG));
	cfg.startupCommand = NS_STARTCMD_INJECTPM;
	ret = svc_openProcess(&hProcess, pid);
	if (ret != 0) {
		nsDbgPrint((const char*) "openProcess failed: %08x\n", ret, 0);
		hProcess = 0;
		goto final;
	}
	nsAttachProcess(hProcess, remotePC, &cfg, 1);
	final:
	if (hProcess != 0) {
		svc_closeHandle(hProcess);
	}
}

void startHomePlugin() {
	typedef void(*funcType)();
	u32 totalSize, i;
	totalSize = plgNextLoadAddr - plgLoadStart;
	u32 ret = rtCheckRemoteMemoryRegionSafeForWrite(getCurrentProcessHandle(), plgLoadStart, totalSize);
	if (ret != 0) {
		nsDbgPrint((const char*) "rwx failed: %08x\n", ret);
		return;
	}
	for (i = 0; i < g_plgInfo->plgCount; i++) {
		nsDbgPrint((const char*) "plg: %08x\n", g_plgInfo->plgBufferPtr[i]);
		((funcType) (g_plgInfo->plgBufferPtr[i]))();
	}
}




void plgInitFromInjectHOME() {
	u32 base = plgPoolStart;
	u32 ret;

	initSharedFunc();
	plgSdmcArchive = (FS_archive) { 9, (FS_path) { PATH_EMPTY, 1, (u8*)"" } };
	ret = FSUSER_OpenArchive(fsUserHandle, &plgSdmcArchive);
	if (ret != 0) {
		nsDbgPrint((const char*) "FSUSER_OpenArchive failed: %08x\n", ret);
		return;
	}
	g_plgInfo = (PLGLOADER_INFO*) base;
	base += rtAlignToPageSize(sizeof(PLGLOADER_INFO));

	u8* arm11BinPath = (u8*) ntrConfig->ntrFilePath;

	arm11BinSize = rtAlignToPageSize(rtGetFileSize(arm11BinPath));
	nsDbgPrint((const char*) "arm11 bin size: %08x\n", arm11BinSize);
	ret = plgEnsurePoolEnd(base + arm11BinSize);
	if (ret != 0) {
		nsDbgPrint((const char*) "alloc memory for arm11bin failed\n");
	}
	ret = rtLoadFileToBuffer(arm11BinPath, (u32*) base, arm11BinSize);
	if (ret == 0) {
		nsDbgPrint((const char*) "load arm11bin failed\n");
		return;
	}
	arm11BinStart = base;
	base += arm11BinSize;
	if (arm11BinSize > 32) {
		volatile uintptr_t temp = (arm11BinStart + 4);
		u32* bootArgs = (u32*) temp;
		bootArgs[0] = 1;
	}

	memset(g_plgInfo, 0, sizeof(PLGLOADER_INFO));
	plgLoadStart = base;
	plgStartPluginLoad();
	plgLoadPluginsFromDirectory((u8*) "home");

	startHomePlugin();

	plgLoadStart = plgNextLoadAddr;
	plgStartPluginLoad();

	rtInitHook(&aptPrepareToStartApplicationHook, ntrConfig->HomeAptStartAppletAddr, (u32) aptPrepareToStartApplicationCallback);
	rtEnableHook(&aptPrepareToStartApplicationHook);

	injectPM();
}


u32 plgSearchReverse(u32 endAddr, u32 startAddr, u32 pat) {
	if (endAddr == 0) {
		return 0;
	}
	while (endAddr >= startAddr) {
		if (*(u32*) (endAddr) == pat) {
			return endAddr;
		}
		endAddr -= 4;

	}
	return 0;
}

u32 plgSearchBytes(u32 startAddr, u32 endAddr, u32* pat, int patlen) {
	u32 lastPage = 0;
	u32 pat0 = pat[0];

	while (1) {
		if (endAddr) {
			if (startAddr >= endAddr) {
				return 0;
			}
		}
		u32 currentPage = rtGetPageOfAddress(startAddr);
		if (currentPage != lastPage) {
			lastPage = currentPage;
			if (rtCheckRemoteMemoryRegionSafeForWrite(getCurrentProcessHandle(), lastPage + 0x1000, 0x1000) != 0) {
				return 0;
			}
		}
		if (*((u32*) (startAddr)) == pat0) {
			if (memcmp((u32*) startAddr, pat, patlen) == 0) {
				return startAddr;
			}
		}
		startAddr += 4;
	}
	return 0;
}

RT_HOOK SetBufferSwapHook;

typedef u32(*SetBufferSwapTypedef) (u32 isDisplay1, u32 a2, u32 addr, u32 addrB, u32 width, u32 a6, u32 a7);


/*
u32 plgNightShiftFramebufferLevel2(u32 addr, u32 stride, u32 height, u32 format) {
	format &= 0x0f;
	if (format == 2) {
		u16* sp = (u16*)addr;
		u16* spEnd = (u16*)(addr + stride * height);
		while (sp < spEnd) {
			u16 pix = *sp;
			u16 b = (pix & 0x1f);
			pix &= 0xffe0;
			pix |= b >> 2;
			*sp = pix;
			sp++;
		}
	}
	else if (format == 1) {
		u8* sp = (u8*)addr;
		u8* spEnd = (u8*)(addr + stride * height);
		while (sp < spEnd) {
			sp[0] >>= 2;
			sp += 3;
		}
	}
	svc_flushProcessDataCache(0xffff8001, (u32)addr, stride * height);
	return 0;
}
*/



u32 plgNightShiftFramebuffer(u32 addr, u32 stride, u32 height, u32 format);

u32 plgOverlayNightShift(u32 isDisplay1, u32 addr, u32 addrB, u32 width, u32 format) {

	if (isDisplay1 == 0) {
		plgNightShiftFramebuffer(addr, width, 400, format);
		if ((addrB) && (addrB != addr)) {
			plgNightShiftFramebuffer(addrB, width, 400, format);
		}
	}
	else {
		plgNightShiftFramebuffer(addr, width, 320, format);
	}


	return 0;
}

typedef u32(*OverlayFnTypedef) (u32 isDisplay1, u32 addr, u32 addrB, u32 width, u32 format);

u32 plgSetBufferSwapCallback(u32 isDisplay1, u32 a2, u32 addr, u32 addrB, u32 width, u32 format, u32 a7) {
	u32 height = isDisplay1 ? 320 : 400;
	u32 ret = 0;
	int isDirty = 0;

	if ((addr >= 0x1f000000) && (addr < 0x1f600000)) {
		if (!isVRAMAccessible) {
			goto final;
		}
	}

	svc_invalidateProcessDataCache(CURRENT_PROCESS_HANDLE, (u32) addr, width * height);
	if ((isDisplay1 == 0) && (addrB) && (addrB != addr)) {
		svc_invalidateProcessDataCache(CURRENT_PROCESS_HANDLE, (u32) addrB, width * height);
	}
	int i;
	for (i = 0; i < pluginEntryCount; i++) {
		if (pluginEntry[i][0] == CALLBACK_OVERLAY) {
			ret = ((OverlayFnTypedef) ((void*) pluginEntry[i][2]))(isDisplay1, addr, addrB, width, format);
		}
		if (ret == 0) {
			isDirty = 1;
		}
	}
	if (g_plgInfo->nightShiftLevel) {
		plgOverlayNightShift(isDisplay1, addr, addrB, width, format);
	}
	else if (isDirty) {
		svc_flushProcessDataCache(CURRENT_PROCESS_HANDLE, (u32) addr, width * height);
		if ((isDisplay1 == 0) && (addrB) && (addrB != addr)) {
			svc_flushProcessDataCache(CURRENT_PROCESS_HANDLE, (u32) addrB, width * height);
		}
	}

	final:
	ret = ((SetBufferSwapTypedef) ((void*) SetBufferSwapHook.callCode))(isDisplay1, a2, addr, addrB, width, format, a7);
	return ret;
}

void plgInitScreenOverlay() {
	if (plgOverlayStatus) {
		return;
	}
	plgOverlayStatus = 2;

	if (rtCheckRemoteMemoryRegionSafeForWrite(getCurrentProcessHandle(), 0x1F000000, 0x00600000) == 0) {
		isVRAMAccessible = 1;
	}
	nsDbgPrint((const char*) "vram accessible: %d\n", isVRAMAccessible);

	static u32 pat[] = { 0xe1833000, 0xe2044cff, 0xe3c33cff, 0xe1833004, 0xe1824f93 };
	static u32 pat2[] = { 0xe8830e60, 0xee078f9a, 0xe3a03001, 0xe7902104 };
	static u32 pat3[] = { 0xee076f9a, 0xe3a02001, 0xe7901104, 0xe1911f9f, 0xe3c110ff };

	u32 addr = plgSearchBytes(0x00100000, 0, pat, sizeof(pat));
	if (!addr) {
		addr = plgSearchBytes(0x00100000, 0, pat2, sizeof(pat2));
	}
	u32 fp = plgSearchReverse(addr, addr - 0x400, 0xe92d5ff0);
	if (!fp) {
		addr = plgSearchBytes(0x00100000, 0, pat3, sizeof(pat3));
		fp = plgSearchReverse(addr, addr - 0x400, 0xe92d47f0);
	}
	nsDbgPrint((const char*) "overlay addr: %08x, %08x\n", addr, fp);

	if (!fp) {

		return;
	}
	rtInitHook(&SetBufferSwapHook, fp, (u32) plgSetBufferSwapCallback);
	rtEnableHook(&SetBufferSwapHook);
	plgOverlayStatus = 1;

}

void initFromInjectGame() {
	typedef void(*funcType)();
	u32 i;
	//Unused
	//u32 ret;

	initSharedFunc();

	g_plgInfo = (PLGLOADER_INFO*) plgPoolStart;
	if (g_plgInfo->nightShiftLevel) {
		plgInitScreenOverlay();
	}
	for (i = 0; i < g_plgInfo->plgCount; i++) {
		nsDbgPrint((const char*) "plg: %08x\n", g_plgInfo->plgBufferPtr[i]);
		((funcType) (g_plgInfo->plgBufferPtr[i]))();
	}
}



u32 plgRequestTempBuffer(u32 size) {

	plgStartPluginLoad();
	if (plgEnsurePoolEnd(plgNextLoadAddr + size) != 0) {
		return 0;
	}
	return plgNextLoadAddr;

}
