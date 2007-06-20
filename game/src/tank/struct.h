// struct.h

#pragma once

#include "script.h"
#include "constants.h"

//----------------------------------------------------------

typedef struct VEHICLEKEYS
{
	int keyLeft;
	int keyRight;
	int keyForward;
	int keyBack;
	int keyFire;
	int keyLight;
	int keyDrop;
	int keyTowerLeft;
	int keyTowerCenter;
	int keyTowerRight;
} VEHICLEKEYS, *LPVEHICLEKEYS;

//----------------------------------------------------------

enum enumControlType
{
	CT_USER_KB,
	CT_USER_KB2,
	CT_USER_MOUSE,
	CT_USER_MOUSE2,
	CT_USER_HYBRID,
};

//----------------------------------------------------------

struct PLAYER
{
	VEHICLEKEYS     KeyMap;
	enumControlType ControlType;
};

//----------------------------------------------------------

#define MAX_PLRNAME	30
#define MAX_VEHCLSNAME	30

struct PlayerDesc
{
	char nick[MAX_PLRNAME];
	char skin[MAX_PATH];
	char cls[MAX_VEHCLSNAME];
	int  team;
	int  score;
};

struct PlayerDescEx : public PlayerDesc
{
	union
	{
		DWORD dwHasPlayers; // �����, ���������� ����������� ������� � ������
		DWORD dwNetworkId;
	};
};

//----------------------------------------------------------

#define MAX_SRVNAME 16

typedef struct GAMEINFO
{
	DWORD dwVersion;
	DWORD dwMapCRC32;
    char  cMapName[MAX_PATH];
	char  cServerName[MAX_SRVNAME];
	short server_fps;
	short latency;
	short timelimit;
	short fraglimit;
	short seed;
	bool  nightmode;
} GAMEINFO, *LPGAMEINFO;

//----------------------------------------------------------


//----------------------------------------------------------
// ��������� ��������� �������
typedef struct GETFILENAME
{
	DWORD   dwFlags;
	LPCTSTR lpszDirectory;
	LPCTSTR lpszActionName; // ������� �� ������ OK
	LPSTR   fileName;
	LPCTSTR lpszFileExt;    // ���������� (��������, "map")
	BOOL  (*lpValidateFunc)(LPCTSTR);
} GETFILENAME, *LPGETFILENAME;

#define GFNF_OVERWRITEPROMPT	1
#define GFNF_FILEMUSTEXIST		2

//----------------------------------------------------------
// ��������� ��������� �����
typedef struct INPUTSTATE
{
	char	keys[256];
	int		mouse_x, mouse_y;
	int		mouse_wheel;
	bool	bLButtonState;
	bool	bRButtonState;
	bool	bMButtonState;
} INPUTSTATE, *LPINPUTSTATE;


//----------------------------------------------------------
// ������� ����������
/*
typedef struct INTERFACE_RESOURCES
{
	HBITMAP hbmBackground;
	HBITMAP hbmButton1;
	HBITMAP hbmButton2;
	HBITMAP hbmButton3;
	HBITMAP hbmCorner_lt;
	HBITMAP hbmCorner_rt;
	HBITMAP hbmCorner_lb;
	HBITMAP hbmCorner_rb;
	HBITMAP hbmBorder_t;
	HBITMAP hbmBorder_b;
	HBITMAP hbmBorder_l;
	HBITMAP hbmBorder_r;
	HBITMAP hbmItem;
	//-------------------
	HBRUSH hbrBackground;
	HBRUSH hbrBkColor;
	HBRUSH hbrNull;
	HBRUSH hbrItem;
	//-------------------
	HPEN hpSelection;
} INTERFACE_RESOURCES;
*/


//----------------------------------------------------------

typedef float AIPRIORITY;


///////////////////////////////////////////////////////////////////////////////
// end of file
