#ifndef SRV_H
#define SRV_H

Result initSrv();
void exitSrv();
Result srv_RegisterClient(Handle* handleptr);
Result srv_getServiceHandle(Handle* handleptr, Handle* out, char* server);

extern Handle srvHandle;
#endif
