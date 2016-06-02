#pragma once
#include <windows.h>
#include "xr_object.h"



typedef struct _GAME_OBJECT_EVENT {
   DWORD msg;		     // event code
   DWORD id;			 // id of object   
   CObject			*O; // client object pointer
   CSE_Abstract *se_obj; // server object pointer
   DWORD res[16];		 // reserved   

} GAME_OBJECT_EVENT, *PGAME_OBJECT_EVENT;


#define   EVT_OBJECT_ADD              0x0001 // создан объект
#define   EVT_OBJECT_ACTIVATE         0x0002 // обновляемым стал
#define	  EVT_OBJECT_SLEEP            0x0003 // из обновляемых вышел
#define   EVT_OBJECT_REMOVE           0x0004 // вышел из списков
#define   EVT_OBJECT_PARENT           0x0005 // изменился владелец объекта
#define   EVT_OBJECT_REJECT           0x0006 // DROP или что-то в таком духе
#define   EVT_OBJECT_SPAWN            0x0007 // завершен спавн клиентского объекта
#define   EVT_OBJECT_DESTROY          0x0008 // уничтожен объект
#define   EVT_OBJECT_SERVER		      0x1000 // серверный объект
#define   EVT_OBJECT_CLIENT		      0x2000 // клиентский объект





__declspec(dllimport) void FastObjectEvent(PGAME_OBJECT_EVENT evt);

inline void process_object_event(DWORD msg, DWORD id, CObject *O, CSE_Abstract *se_obj = NULL, DWORD r0 = 0, DWORD r1 = 0) // для большинства случаев
{
	GAME_OBJECT_EVENT evt;
	// Memory.mem_fill(&evt, 0, sizeof(evt));
	evt.msg = msg;
	evt.id  = id;
	evt.O	= O;
	evt.se_obj = se_obj;	
	evt.res[0] = r0;
	evt.res[1] = r1;	
	FastObjectEvent(&evt);
}

