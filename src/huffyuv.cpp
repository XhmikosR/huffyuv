//
// Huffyuv v2.1.1, by Ben Rudiak-Gould.
// http://www.math.berkeley.edu/~benrg/huffyuv.html
//
// Based on MSYUV sample code, which is:
// Copyright (c) 1993 Microsoft Corporation.
// All Rights Reserved.
//
// Changes copyright 2000 Ben Rudiak-Gould, and distributed under
// the terms of the GNU General Public License, v2 or later.  See
// http://www.gnu.org/copyleft/gpl.html.
//
// I edit these files in 10-point Verdana, a proportionally-spaced font.
// You may notice formatting oddities if you use a monospaced font.
//


#include "huffyuv.h"
#include "huffyuv_a.h"
#include "resource.h"

#include <crtdbg.h>

#include <stdio.h>

#include <commctrl.h>

TCHAR szDescription[] = TEXT("Huffyuv v2.1.1 - CCESP Patch v0.2.5"); // Wanton, bastel
TCHAR szName[]        = TEXT("Huffyuv");

#define VERSION         0x00020001      // 2.1


/********************************************************************
********************************************************************/

// these tables are generated at runtime from the data in tables.cpp

unsigned char encode1_shift[256];
unsigned encode1_add_shifted[256];
unsigned char encode2_shift[256];
unsigned encode2_add_shifted[256];
unsigned char encode3_shift[256];
unsigned encode3_add_shifted[256];

unsigned char decode1_shift[256];
unsigned char decode2_shift[256];
unsigned char decode3_shift[256];
DecodeTable decode1, decode2, decode3;

CodecInst *encode_table_owner, *decode_table_owner;

/********************************************************************
********************************************************************/

void Msg(const char fmt[], ...) {
  static int debug = GetPrivateProfileInt("debug", "log", 0, "huffyuv.ini");
  if (!debug) return;

  DWORD written;
  char buf[2000];
  va_list val;
  
  va_start(val, fmt);
  wvsprintf(buf, fmt, val);

  const COORD _80x50 = {80,50};
  static BOOL startup = (AllocConsole(), SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), _80x50));
  WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), buf, lstrlen(buf), &written, 0);
}


int AppFlags() {
  static int flags = -1;
  if (flags < 0) {
    flags = 0;
    TCHAR apppath[MAX_PATH];
    if (GetModuleFileName(NULL, apppath, MAX_PATH)) {
      TCHAR* appname = strrchr(apppath, '\\');
      appname = appname ? appname+1 : apppath;
      Msg("App name is %s; ", appname);
      if (!lstrcmpi(appname, TEXT("premiere.exe")))
        flags = 1;
      if (!lstrcmpi(appname, TEXT("veditor.exe")))
        flags = 1;
      if (!lstrcmpi(appname, TEXT("avi2mpg2_vfw.exe")))
        flags = 1;
      if (!lstrcmpi(appname, TEXT("bink.exe")))
        flags = 1;
      if (!lstrcmpi(appname, TEXT("afterfx.exe")))
        flags = 2;
	  // new flag for ccesp/ccesp trial for buggy YUY2 handling - bastel
      if (!lstrcmpi(appname, TEXT("cctsp.exe")) || !lstrcmpi(appname, TEXT("cctspt.exe")))
		flags = 4;
      Msg("flags=%d\n", flags);
    }
  }
  return flags;
}

bool SuggestRGB() {
  return !!GetPrivateProfileInt("debug", "rgboutput", AppFlags()&1, "huffyuv.ini");
}

bool AllowRGBA() {
  return !!GetPrivateProfileInt("general", "enable_rgba", AppFlags()&2, "huffyuv.ini");
}

// ignore top-down (i.e. negative height) - bastel
bool IgnoreTopDown() {
  return !!GetPrivateProfileInt("debug", "ignore_topdown", AppFlags()&4, "huffyuv.ini");
}


/********************************************************************
********************************************************************/

CodecInst* Open(ICOPEN* icinfo) {
  if (icinfo && icinfo->fccType != ICTYPE_VIDEO)
      return NULL;

  CodecInst* pinst = new CodecInst();

  if (icinfo) icinfo->dwError = pinst ? ICERR_OK : ICERR_MEMORY;

  return pinst;
}

DWORD Close(CodecInst* pinst) {
//    delete pinst;       // this caused problems when deleting at app close time
    return 1;
}

/********************************************************************
********************************************************************/

enum {
  methodLeft=0, methodGrad=1, methodMedian=2,
  methodConvertToYUY2=-1, methodOld=-2,
  flagDecorrelate=64
};

int ConvertOldMethod(int bitcount) {
  switch (bitcount&7) {
    case 1: return methodLeft;
    case 2: return methodLeft|flagDecorrelate;
    case 3: return (bitcount>=24) ? methodGrad+flagDecorrelate : methodGrad;
    case 4: return methodMedian;
    default: return methodOld;
  }
}

static inline int GetMethod(LPBITMAPINFOHEADER lpbi) {
  if (lpbi->biCompression == FOURCC_HFYU) {
    if (lpbi->biBitCount & 7)
      return ConvertOldMethod(lpbi->biBitCount);
    else if (lpbi->biSize > sizeof(BITMAPINFOHEADER))
      return *((unsigned char*)lpbi + sizeof(BITMAPINFOHEADER));
  }
  return methodOld;
}

static inline int GetBitCount(LPBITMAPINFOHEADER lpbi) {
  if (lpbi->biCompression == FOURCC_HFYU && lpbi->biSize > sizeof(BITMAPINFOHEADER)+1) {
    int bpp_override = *((char*)lpbi + sizeof(BITMAPINFOHEADER) + 1);
    if (bpp_override)
      return bpp_override;
  }
  return lpbi->biBitCount;
}

struct MethodName { int method; const char* name; };
MethodName yuv_method_names[] = {
  { methodLeft, "Predict left (fastest)" },
  { methodGrad, "Predict gradient" },
  { methodMedian, "Predict median (best)" }
};
MethodName rgb_method_names[] = {
  { methodLeft, "Predict left/no decorr. (fastest)" },
  { methodLeft+flagDecorrelate, "Predict left" },
  { methodGrad+flagDecorrelate, "Predict gradient (best)" },
  { methodConvertToYUY2, "<-- Convert to YUY2" }
};

MethodName field_thresholds[] = {
	{ 240, "240" },
	{ 288, "288" },
	{ 480, "480" },
	{ 576, "576" }
};

struct { UINT item; UINT tip; } item2tip[] = {
	{ IDC_YUY2METHOD,		IDS_TIP_METHOD_YUY2		},
	{ IDC_RGBMETHOD,		IDS_TIP_METHOD_RGB		},
	{ IDC_FIELDTHRESHOLD,	IDS_TIP_FIELD_THRESHOLD	},
	{ IDC_STATIC_FIELD,		IDS_TIP_FIELD_THRESHOLD	},
	{ IDC_RGBOUTPUT,		IDS_TIP_RGB_ONLY		},
	{ IDC_RGBA,				IDS_TIP_RGBA_COMPR		},
	{ IDC_SWAPFIELDS,		IDS_TIP_SWAPFIELDS		},
	{ IDC_LOG,				IDS_TIP_LOG				},
	{ IDC_FULL_SIZE_BUFFER,	IDS_TIP_FULL_SIZE_BUFFER},
	{ IDC_IGNORE_IFLAG,     IDS_TIP_IGNORE_IFLAG    },
	{ 0,0 }
};


bool IsLegalMethod(int method, bool rgb) {
  if (rgb) {
    return (method == methodOld || method == methodLeft
     || method == methodLeft+flagDecorrelate || method == methodGrad+flagDecorrelate);
  }
  else {
    return (method == methodOld || method == methodLeft
     || method == methodGrad || method == methodMedian);
  }
}

/********************************************************************
********************************************************************/

BOOL CodecInst::QueryAbout() { return TRUE; }

static BOOL CALLBACK AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_COMMAND) {
    switch (LOWORD(wParam)) {
    case IDOK:
      EndDialog(hwndDlg, 0);
      break;
    case IDC_HOMEPAGE:
      ShellExecute(NULL, NULL, "http://www.math.berkeley.edu/~benrg/huffyuv.html", NULL, NULL, SW_SHOW);
      break;
    case IDC_EMAIL:
      ShellExecute(NULL, NULL, "mailto:benrg@math.berkeley.edu", NULL, NULL, SW_SHOW);
      break;
    }
  }
  return FALSE;
}

DWORD CodecInst::About(HWND hwnd) {
  DialogBox(hmoduleHuffyuv, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDialogProc);
  return ICERR_OK;
}


// add a tooltip to a tooltip control, might be buggy, though - bastel
int AddTooltip(HWND tooltip, HWND client, UINT stringid)
{
	HINSTANCE ghThisInstance=(HINSTANCE)GetWindowLong(client, GWL_HINSTANCE);

	TOOLINFO				ti;			// struct specifying info about tool in tooltip control
    static unsigned int		uid	= 0;	// for ti initialization
	RECT					rect;		// for client area coordinates
	TCHAR					buf[2000];	// a static buffer is sufficent, TTM_ADDTOOL seems to copy it

	// load the string manually, passing the id directly to TTM_ADDTOOL truncates the message :(
	if ( !LoadString(ghThisInstance, stringid, buf, 2000) ) return -1;

	// get coordinates of the main client area
	GetClientRect(client, &rect);
	
    // initialize members of the toolinfo structure
	ti.cbSize		= sizeof(TOOLINFO);
	ti.uFlags		= TTF_SUBCLASS;
	ti.hwnd			= client;
	ti.hinst		= ghThisInstance;		// not necessary if lpszText is not a resource id
	ti.uId			= uid;
	ti.lpszText		= buf;

	// Tooltip control will cover the whole window
	ti.rect.left	= rect.left;    
	ti.rect.top		= rect.top;
	ti.rect.right	= rect.right;
	ti.rect.bottom	= rect.bottom;
	
	// send a addtool message to the tooltip control window
	SendMessage(tooltip, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);	
	return uid++;
} 

// create a tooltip control over the entire window area - bastel
HWND CreateTooltip(HWND hwnd)
{
    // initialize common controls
	INITCOMMONCONTROLSEX	iccex;		// struct specifying control classes to register
    iccex.dwICC		= ICC_WIN95_CLASSES;
    iccex.dwSize	= sizeof(INITCOMMONCONTROLSEX);
    InitCommonControlsEx(&iccex);


	HINSTANCE	ghThisInstance=(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE);
    HWND		hwndTT;					// handle to the tooltip control

    // create a tooltip window
	hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
							CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
							hwnd, NULL, ghThisInstance, NULL);
	
	SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	// set some timeouts so tooltips appear fast and stay long (32767 seems to be a limit here)
	SendMessage(hwndTT, TTM_SETDELAYTIME, (WPARAM)(DWORD)TTDT_INITIAL, (LPARAM)10);
	SendMessage(hwndTT, TTM_SETDELAYTIME, (WPARAM)(DWORD)TTDT_AUTOPOP, (LPARAM)30*1000);

	return hwndTT;
}


static BOOL CALLBACK ConfigureDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

  if (uMsg == WM_INITDIALOG) {

    HWND hctlYUY2Method = GetDlgItem(hwndDlg, IDC_YUY2METHOD);
    int yuy2method = GetPrivateProfileInt("general", "yuy2method", methodMedian, "huffyuv.ini");
    for (int i = 0; i < sizeof(yuv_method_names)/sizeof(yuv_method_names[0]); ++i) {
      SendMessage(hctlYUY2Method, CB_ADDSTRING, 0, (LPARAM)yuv_method_names[i].name);
      if (yuv_method_names[i].method == yuy2method)
        SendMessage(hctlYUY2Method, CB_SETCURSEL, i, 0);
    }

    HWND hctlRGBMethod = GetDlgItem(hwndDlg, IDC_RGBMETHOD);
    int rgbmethod = GetPrivateProfileInt("general", "rgbmethod", methodGrad+flagDecorrelate, "huffyuv.ini");
    for (int j = 0; j < sizeof(rgb_method_names)/sizeof(rgb_method_names[0]); ++j) {
      SendMessage(hctlRGBMethod, CB_ADDSTRING, 0, (LPARAM)rgb_method_names[j].name);
      if (rgb_method_names[j].method == rgbmethod)
        SendMessage(hctlRGBMethod, CB_SETCURSEL, j, 0);
    }

	// field threshold entry, using the lame combo box sorting algo - bastel
    HWND	hctlFieldThreshold	= GetDlgItem(hwndDlg, IDC_FIELDTHRESHOLD);
    int		fthreshold			= GetPrivateProfileInt("general", "field_threshold", FIELD_THRESHOLD, "huffyuv.ini");
	char	temp[12]			= {0};
	bool	dupe				= false;
	_snprintf(temp, 11, "%3d", fthreshold);
	SendMessage(hctlFieldThreshold, CB_LIMITTEXT, 11, 0);
	for (int k = 0; k < sizeof(field_thresholds)/sizeof(field_thresholds[0]); k++) {
		SendMessage(hctlFieldThreshold, CB_ADDSTRING, 0, (LPARAM)field_thresholds[k].name);
		if ( field_thresholds[k].method==fthreshold ) dupe=true;
	}
	if ( !dupe) SendMessage(hctlFieldThreshold, CB_ADDSTRING, 0, (LPARAM)temp);
	SendMessage(hctlFieldThreshold, CB_SETCURSEL, SendMessage(hctlFieldThreshold, CB_FINDSTRINGEXACT, -1, (LPARAM)temp), 0);


    CheckDlgButton(hwndDlg, IDC_RGBOUTPUT,
      GetPrivateProfileInt("debug", "rgboutput", false, "huffyuv.ini") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_RGBA,
      GetPrivateProfileInt("general", "enable_rgba", false, "huffyuv.ini") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_SWAPFIELDS,
      GetPrivateProfileInt("debug", "decomp_swap_fields", false, "huffyuv.ini") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_LOG,
      GetPrivateProfileInt("debug", "log", false, "huffyuv.ini") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_FULL_SIZE_BUFFER,
      GetPrivateProfileInt("debug", "full_size_buffer", FULL_SIZE_BUFFER, "huffyuv.ini") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwndDlg, IDC_IGNORE_IFLAG,
      GetPrivateProfileInt("general", "ignore_iflag", false, "huffyuv.ini") ? BST_CHECKED : BST_UNCHECKED);


    // create a tooltip window and add tooltips - bastel
	HWND hwndTip = CreateTooltip(hwndDlg);
	for (int l=0; item2tip[l].item; l++ )
		AddTooltip(hwndTip, GetDlgItem(hwndDlg, item2tip[l].item),	item2tip[l].tip);
	SendMessage(hwndTip, TTM_SETMAXTIPWIDTH, 0, (LPARAM)(INT)350);	// ah well this is totally wrong but works
  }

  else if (uMsg == WM_COMMAND) {

    switch (LOWORD(wParam)) {

    case IDOK:
      {
        char methodstring[4];
        wsprintf(methodstring, "%d",
          yuv_method_names[SendMessage(GetDlgItem(hwndDlg, IDC_YUY2METHOD), CB_GETCURSEL, 0, 0)].method);
        WritePrivateProfileString("general", "yuy2method", methodstring, "huffyuv.ini");
        wsprintf(methodstring, "%d",
          rgb_method_names[SendMessage(GetDlgItem(hwndDlg, IDC_RGBMETHOD), CB_GETCURSEL, 0, 0)].method);
        WritePrivateProfileString("general", "rgbmethod", methodstring, "huffyuv.ini");
      }
      WritePrivateProfileString("debug", "rgboutput",
        (IsDlgButtonChecked(hwndDlg, IDC_RGBOUTPUT) == BST_CHECKED) ? "1" : NULL, "huffyuv.ini");
      WritePrivateProfileString("general", "enable_rgba",
        (IsDlgButtonChecked(hwndDlg, IDC_RGBA) == BST_CHECKED) ? "1" : NULL, "huffyuv.ini");
      WritePrivateProfileString("debug", "decomp_swap_fields",
        (IsDlgButtonChecked(hwndDlg, IDC_SWAPFIELDS) == BST_CHECKED) ? "1" : "0", "huffyuv.ini");
      WritePrivateProfileString("debug", "log",
        (IsDlgButtonChecked(hwndDlg, IDC_LOG) == BST_CHECKED) ? "1" : "0", "huffyuv.ini");
      WritePrivateProfileString("debug", "full_size_buffer",
        (IsDlgButtonChecked(hwndDlg, IDC_FULL_SIZE_BUFFER) == BST_CHECKED) ? "1" : "0", "huffyuv.ini");
      WritePrivateProfileString("general", "ignore_iflag",
        (IsDlgButtonChecked(hwndDlg, IDC_IGNORE_IFLAG) == BST_CHECKED) ? "1" : "0", "huffyuv.ini");

	  // write field threshold setting. accept only if numeric and 0 < ft <= 16384 - bastel
	  {
		char	temp[12]= {0};
		char*	endp	= 0;
		SendMessage(GetDlgItem(hwndDlg, IDC_FIELDTHRESHOLD), WM_GETTEXT, 11, (LPARAM)temp);
		int fthreshold	= strtoul(temp, &endp, 10);
		if ( strchr(" \t\r\n", *endp) && fthreshold>0 && fthreshold<=16384 ) {
			// could pass old temp, but this way whitespace is removed
			_snprintf(temp, 11, "%03d", fthreshold);		
			WritePrivateProfileString("general", "field_threshold", temp, "huffyuv.ini");
		}
	  }

    case IDCANCEL:
      EndDialog(hwndDlg, 0);
      break;

    default:
      return AboutDialogProc(hwndDlg, uMsg, wParam, lParam);    // handle email and home-page buttons
    }
  }
  return FALSE;
}

BOOL CodecInst::QueryConfigure() { return TRUE; }

DWORD CodecInst::Configure(HWND hwnd) {
  DialogBox(hmoduleHuffyuv, MAKEINTRESOURCE(IDD_CONFIGURE), hwnd, ConfigureDialogProc);
  return ICERR_OK;
}


/********************************************************************
********************************************************************/


// we have no state information which needs to be stored

DWORD CodecInst::GetState(LPVOID pv, DWORD dwSize) { return 0; }

DWORD CodecInst::SetState(LPVOID pv, DWORD dwSize) { return 0; }


DWORD CodecInst::GetInfo(ICINFO* icinfo, DWORD dwSize) {
  if (icinfo == NULL)
    return sizeof(ICINFO);

  if (dwSize < sizeof(ICINFO))
    return 0;

  icinfo->dwSize            = sizeof(ICINFO);
  icinfo->fccType           = ICTYPE_VIDEO;
  icinfo->fccHandler        = FOURCC_HFYU;
  icinfo->dwFlags           = 0;

  icinfo->dwVersion         = VERSION;
  icinfo->dwVersionICM      = ICVERSION;
  MultiByteToWideChar(CP_ACP, 0, szDescription, -1, icinfo->szDescription, sizeof(icinfo->szDescription)/sizeof(WCHAR));
  MultiByteToWideChar(CP_ACP, 0, szName, -1, icinfo->szName, sizeof(icinfo->szName)/sizeof(WCHAR));

  return sizeof(ICINFO);
}


/********************************************************************
********************************************************************/


struct PrintBitmapType {
  char s[32];
  PrintBitmapType(LPBITMAPINFOHEADER lpbi) {
    if (!lpbi)
      strcpy(s,  "(null)");
    else {
      *(DWORD*)s = lpbi->biCompression;
      s[4] = 0;
      if (!isalnum(s[0]) || !isalnum(s[1]) || !isalnum(s[2]) || !isalnum(s[3]))
        wsprintfA(s, "%x", lpbi->biCompression);
      wsprintfA(strchr(s, 0), ", %d bits", GetBitCount(lpbi));
      if (lpbi->biCompression == FOURCC_HFYU && !(lpbi->biBitCount&7) && lpbi->biSize > sizeof(BITMAPINFOHEADER))
        wsprintfA(strchr(s, 0), ", method %d", *((unsigned char*)lpbi + sizeof(BITMAPINFOHEADER)));
    }
  }
};


// fast clipping of values to unsigned char range

static unsigned char clip[896];

static void InitClip() {
  memset(clip, 0, 320);
  for (int i=0; i<256; ++i) clip[i+320] = i;
  memset(clip+320+256, 255, 320);
}

static inline unsigned char Clip(int x)
  { return clip[320 + ((x+0x8000) >> 16)]; }


/********************************************************************
********************************************************************/


// 0=unknown, -1=compressed YUY2, -2=compressed RGB, -3=compressed RGBA, 1=YUY2, 2=UYVY, 3=RGB 24-bit, 4=RGB 32-bit
static int GetBitmapType(LPBITMAPINFOHEADER lpbi) {
  if (!lpbi)
    return 0;
  const int fourcc = lpbi->biCompression;
  if (fourcc == FOURCC_VYUY || fourcc == FOURCC_YUY2)
    return 1;
  if (fourcc == FOURCC_UYVY)
    return 2;
  const int bitcount = GetBitCount(lpbi);
  if (fourcc == 0 || fourcc == ' BID')
    return (bitcount == 24) ? 3 : (bitcount == 32) ? 4 : 0;
  if (fourcc == FOURCC_HFYU)
    return ((bitcount&~7) == 16) ? -1 : ((bitcount&~7) == 24) ? -2 : ((bitcount&~7) == 32) ? -3 : 0;
  return 0;
}


static bool CanCompress(LPBITMAPINFOHEADER lpbiIn) {
  int intype = GetBitmapType(lpbiIn);
  return (intype == 1 || intype == 2 || intype == 3 || (intype == 4 && AllowRGBA()));
}


static bool CanCompress(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  if (!lpbiOut) return CanCompress(lpbiIn);

  int intype = GetBitmapType(lpbiIn);
  int outtype = GetBitmapType(lpbiOut);

  if (outtype < 0) {
    if (!IsLegalMethod(GetMethod(lpbiOut), outtype != -1))
      return false;
  }

  switch (intype) {
    case 1: case 2:
      return (outtype == -1);
    case 3:
      return (outtype == 1 || outtype == -1 || outtype == -2);
    case 4:
      return (outtype == -3 && AllowRGBA());
    default:
      return false;
  }
}


/********************************************************************
********************************************************************/


const unsigned char* GetHuffTable(int method, bool rgb) {
  if (rgb) {
    if (method == methodLeft)
      return left_rgb;
    else if (method == methodLeft+flagDecorrelate)
      return left_decorrelate_rgb;
    else
      return grad_decorrelate_rgb;
  } else {
    if (method == methodLeft)
      return left_yuv;
    else if (method == methodGrad)
      return grad_yuv;
    else
      return med_yuv;
  }
}


const unsigned char* DecompressHuffmanTable(const unsigned char* hufftable, unsigned char* dst) {
  int i=0;
  do {
    int val = *hufftable & 31;
    int repeat = *hufftable++ >> 5;
    if (!repeat)
      repeat = *hufftable++;
    while (repeat--)
      dst[i++] = val;
  } while (i<256);
  return hufftable;
}


#define HUFFTABLE_CLASSIC_YUV ((const unsigned char*)-1)
#define HUFFTABLE_CLASSIC_RGB ((const unsigned char*)-2)
#define HUFFTABLE_CLASSIC_YUV_CHROMA ((const unsigned char*)-3)

const unsigned char* InitializeShiftAddTables(const unsigned char* hufftable, unsigned char* shift, unsigned* add_shifted) {
  // special-case the old tables, since they don't fit the new rules
  if (hufftable == HUFFTABLE_CLASSIC_YUV || hufftable == HUFFTABLE_CLASSIC_RGB) {
    DecompressHuffmanTable(classic_shift_luma, shift);
    for (int i=0; i<256; ++i) add_shifted[i] = classic_add_luma[i]<<(32-shift[i]);
    return (hufftable == HUFFTABLE_CLASSIC_YUV) ? HUFFTABLE_CLASSIC_YUV_CHROMA : hufftable;
  }
  else if (hufftable == HUFFTABLE_CLASSIC_YUV_CHROMA) {
    DecompressHuffmanTable(classic_shift_chroma, shift);
    for (int i=0; i<256; ++i) add_shifted[i] = classic_add_chroma[i]<<(32-shift[i]);
    return hufftable;
  }

  hufftable = DecompressHuffmanTable(hufftable, shift);
  // derive the actual bit patterns from the code lengths
  int min_already_processed = 32;
  unsigned __int32 bits = 0;
  do {
    int max_not_processed=0;
    for (int i=0; i<256; ++i) {
      if (shift[i] < min_already_processed && shift[i] > max_not_processed)
        max_not_processed = shift[i];
    }
    int bit = 1<<(32-max_not_processed);
    _ASSERTE(!(bits & (bit-1)));
    for (int j=0; j<256; ++j) {
      if (shift[j] == max_not_processed) {
        add_shifted[j] = bits;
        bits += bit;
      }
    }
    min_already_processed = max_not_processed;
  } while (bits&0xFFFFFFFF);
  return hufftable;
}


const unsigned char* InitializeEncodeTables(const unsigned char* hufftable) {
  hufftable = InitializeShiftAddTables(hufftable, encode1_shift, encode1_add_shifted);
  hufftable = InitializeShiftAddTables(hufftable, encode2_shift, encode2_add_shifted);
  hufftable = InitializeShiftAddTables(hufftable, encode3_shift, encode3_add_shifted);
//  _ASSERTE(!*hufftable);
  return hufftable;
}


DWORD CodecInst::CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  Msg("CompressQuery: input = %s, output = %s\n", &PrintBitmapType(lpbiIn), &PrintBitmapType(lpbiOut));
  return CanCompress(lpbiIn, lpbiOut) ? ICERR_OK : ICERR_BADFORMAT;
}


DWORD CodecInst::CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  if (!CanCompress(lpbiIn))
    return ICERR_BADFORMAT;

  int intype = GetBitmapType(lpbiIn);

  bool compress_as_yuy2 = (intype<3);
  int method;
  if (!compress_as_yuy2) {
    method = GetPrivateProfileInt("general", "rgbmethod", methodGrad+flagDecorrelate, "huffyuv.ini");
    if (method==methodConvertToYUY2)
      compress_as_yuy2 = true;
  }
  if (compress_as_yuy2)
    method = GetPrivateProfileInt("general", "yuy2method", methodMedian, "huffyuv.ini");

  const unsigned char* hufftable = GetHuffTable(method, !compress_as_yuy2);

  int hufftable_length = (lstrlen((const char*)hufftable)+4) & -4;

  if (!lpbiOut)
    return sizeof(BITMAPINFOHEADER)+4+hufftable_length;

  int real_bit_count = (intype==4) ? 32 : (compress_as_yuy2) ? 16 : 24;

  //*lpbiOut = *lpbiIn;
  memset(lpbiOut, 0, sizeof(BITMAPINFOHEADER)+4+hufftable_length);
  lpbiOut->biSize           = sizeof(BITMAPINFOHEADER)+4+hufftable_length;
  lpbiOut->biWidth          = lpbiIn->biWidth;
  lpbiOut->biHeight         = lpbiIn->biHeight;
  lpbiOut->biPlanes         = lpbiIn->biPlanes;
  lpbiOut->biBitCount       = max(24, real_bit_count);
  lpbiOut->biCompression    = FOURCC_HFYU;
  lpbiOut->biSizeImage      = lpbiIn->biSizeImage;
  //lpbiOut->biXPelsPerMeter  = 0;
  //lpbiOut->biYPelsPerMeter  = 0;
  //lpbiOut->biClrImportant   = 0;
  //lpbiOut->biClrUsed        = 0;
  unsigned char* extra_data = (unsigned char*)lpbiOut + sizeof(BITMAPINFOHEADER);
  //memset(extra_data, 0, 4+hufftable_length);
  extra_data[0] = method;
  extra_data[1] = real_bit_count;

  // store in file if interlaced or not. this is compatible with huffyuv 2.2.0 (using upper nibble, lower is for reduced)
  int ft = GetPrivateProfileInt("general", "field_threshold", FIELD_THRESHOLD, "huffyuv.ini");
  extra_data[2] = (abs(lpbiIn->biHeight)>ft ? INTERLACED_ENCODING : PROGRESSIVE_ENCODING) << 4;

  //extra_data[3] = 0;

  lstrcpy((char*)extra_data+4, (const char*)hufftable);
  return ICERR_OK;
}


const unsigned char* GetEmbeddedHuffmanTable(LPBITMAPINFOHEADER lpbi) {
  if (lpbi->biSize == sizeof(BITMAPINFOHEADER) && !(lpbi->biBitCount&7))
    return lpbi->biBitCount>=24 ? HUFFTABLE_CLASSIC_RGB : HUFFTABLE_CLASSIC_YUV;
  else
    return (const unsigned char*)lpbi + sizeof(BITMAPINFOHEADER) + ((lpbi->biBitCount&7) ? 0 : 4);
}


DWORD CodecInst::CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  Msg("CompressBegin: input = %s, output = %s\n", &PrintBitmapType(lpbiIn), &PrintBitmapType(lpbiOut));

  if (!CanCompress(lpbiIn, lpbiOut))
    return ICERR_BADFORMAT;

  CompressEnd();  // free resources if necessary

  int intype = GetBitmapType(lpbiIn);
  int outtype = GetBitmapType(lpbiOut);
  int method = GetMethod(lpbiOut);

  if (outtype < 0) {
    InitializeEncodeTables(GetEmbeddedHuffmanTable(lpbiOut));
    encode_table_owner = this;
  }

  // load field threshold - bastel
  field_threshold = GetPrivateProfileInt("general", "field_threshold", FIELD_THRESHOLD, "huffyuv.ini");
  Msg("CompressBegin: field threshold = %d lines, field processing %s\n",
	  field_threshold, abs(lpbiOut->biHeight)>field_threshold ? "enabled" : "disabled");

  // allocate buffer if compressing RGB->YUY2->HFYU
  if (outtype == -1 && (method == methodGrad || intype == 3))
    yuy2_buffer = new unsigned char[lpbiIn->biWidth * lpbiIn->biHeight * 2 + 4];
  if (method == methodMedian)
    median_buffer = new unsigned char[lpbiIn->biWidth * lpbiIn->biHeight * 2 + 8];
  if (outtype == -2 && method == methodGrad+flagDecorrelate)
    rgb_buffer = new unsigned char[lpbiIn->biWidth * lpbiIn->biHeight * 3 + 4];
  if (outtype == -3 && method == methodGrad+flagDecorrelate)
    rgb_buffer = new unsigned char[lpbiIn->biWidth * lpbiIn->biHeight * 4 + 4];

  InitClip();

  return ICERR_OK;
}

DWORD CodecInst::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  // Assume 24 bpp worst-case for YUY2 input, 40 bpp for RGB.
  // The actual worst case is 43/51 bpp currently, but this is exceedingly improbable
  // (probably impossible with real captured video)
  // bastel: never say impossible. try to capture static noise with the original huffyuv and be surprised.
  //         virtualdub has a workaround for it. avi_io not. anyway, i don't think it slows down processing.
  //         if you think it does you can still enable the old behaviour. full buffer costs memory, though :)
  int fullsize = GetPrivateProfileInt("debug", "full_size_buffer", FULL_SIZE_BUFFER, "huffyuv.ini");
  return fullsize	? ((lpbiIn->biWidth * lpbiIn->biHeight * ((GetBitmapType(lpbiIn) <= 2) ? 43 : 51)) + 7) / 8
					:	lpbiIn->biWidth * lpbiIn->biHeight * ((GetBitmapType(lpbiIn) <= 2) ? 3 : 5);
}


template<typename T> static inline T* Align8(T* ptr) { return (T*)((unsigned(ptr)+7)&~7); }


DWORD CodecInst::Compress(ICCOMPRESS* icinfo, DWORD dwSize) {

  if (icinfo->lpckid)
    *icinfo->lpckid = FOURCC_HFYU;
  *icinfo->lpdwFlags = AVIIF_KEYFRAME;

  int intype = GetBitmapType(icinfo->lpbiInput);
  int outtype = GetBitmapType(icinfo->lpbiOutput);

  if (outtype < 0) {    // compressing (as opposed to converting RGB->YUY2)
    const int method = GetMethod(icinfo->lpbiOutput);

    if (encode_table_owner != this) {
      InitializeEncodeTables(GetEmbeddedHuffmanTable(icinfo->lpbiOutput));
      encode_table_owner = this;
    }

    const unsigned char* const in = (unsigned char*)icinfo->lpInput;
    unsigned long* const out = (unsigned long*)icinfo->lpOutput;

    unsigned long* out_end;

    if (outtype == -1) {    // compressing to HFYU16 (4:2:2 YUV)
      int stride = icinfo->lpbiInput->biWidth * 2;
      int size = stride * icinfo->lpbiInput->biHeight;
      if (icinfo->lpbiInput->biHeight > field_threshold) stride *= 2;    // if image is interlaced, double stride so fields are treated separately
      const unsigned char* yuy2in = in;

      if (intype == 3) {    // RGB24->YUY2->HFYU16
        ConvertRGB24toYUY2(in, yuy2_buffer, icinfo->lpbiInput->biWidth, icinfo->lpbiInput->biHeight);
        yuy2in = yuy2_buffer;
        size = icinfo->lpbiInput->biWidth * icinfo->lpbiInput->biHeight * 2;
      }
      if (method == methodGrad) {
        // Attempt to match yuy2_buffer alignment to input alignment.
        // Why, oh why, can't standard libraries just quadword-align
        // big memory blocks and be done with it?
        unsigned char* const aligned_yuy2_buffer = yuy2_buffer + 4*((int(yuy2in)&7) != (int(yuy2_buffer)&7));
        mmx_RowDiff(yuy2in, aligned_yuy2_buffer, yuy2in+size, stride);
        yuy2in = aligned_yuy2_buffer;
      }
      if (intype==2) {    // UYVY->HFYU16
        if (method == methodMedian) {
          unsigned char* aligned_median_buffer = Align8(median_buffer);
          mmx_MedianPredictUYVY(yuy2in, aligned_median_buffer, yuy2in+size, stride);
          out_end = asm_CompressUYVY(aligned_median_buffer, out, aligned_median_buffer+size);
        }
        else {
          out_end = asm_CompressUYVYDelta(yuy2in,out,yuy2in+size);
        }
      } else {    // YUY2->HFYU16
        if (method == methodMedian) {
          unsigned char* aligned_median_buffer = Align8(median_buffer);
          mmx_MedianPredictYUY2(yuy2in, aligned_median_buffer, yuy2in+size, stride);
          out_end = asm_CompressYUY2(aligned_median_buffer, out, aligned_median_buffer+size);
        }
        else {
          out_end = asm_CompressYUY2Delta(yuy2in,out,yuy2in+size);
        }
      }
    }
    else {    // compressing to HFYU24 (RGB) or HFYU32 (RGBA)
      int stride = (icinfo->lpbiInput->biWidth * GetBitCount(icinfo->lpbiInput)) >> 3;
      int size = stride * icinfo->lpbiInput->biHeight;
      if (icinfo->lpbiInput->biHeight > field_threshold) stride *= 2;    // if image is interlaced, double stride so fields are treated separately
      const unsigned char* rgbin = in;

      if ((method&~flagDecorrelate) == methodGrad) {
        unsigned char* const aligned_rgb_buffer = rgb_buffer + 4*((int(in)&7) != (int(rgb_buffer)&7));
        mmx_RowDiff(in, aligned_rgb_buffer, in+size, stride);
        rgbin = aligned_rgb_buffer;
      }
      if (outtype == -2) {    // RGB24->HFYU24
        if (method == methodLeft || method == methodOld)
          out_end = asm_CompressRGBDelta(rgbin,out,rgbin+size);
        else
          out_end = asm_CompressRGBDeltaDecorrelate(rgbin,out,rgbin+size);
      }
      else if (outtype == -3) {     // RGBA->HFYU32
        if (method == methodLeft || method == methodOld)
          out_end = asm_CompressRGBADelta(rgbin,out,rgbin+size);
        else
          out_end = asm_CompressRGBADeltaDecorrelate(rgbin,out,rgbin+size);
      }
      else
        return ICERR_BADFORMAT;
    }
    icinfo->lpbiOutput->biSizeImage = DWORD(out_end) - DWORD(out);
    return ICERR_OK;
  }
  else if (outtype == 1) {    // RGB24->YUY2
    ConvertRGB24toYUY2((unsigned char*)icinfo->lpInput, (unsigned char*)icinfo->lpOutput,
      icinfo->lpbiInput->biWidth, icinfo->lpbiInput->biHeight);
    icinfo->lpbiOutput->biSizeImage = (icinfo->lpbiOutput->biWidth * icinfo->lpbiOutput->biHeight * GetBitCount(icinfo->lpbiOutput)) >> 3;
    return ICERR_OK;
  }
  else
    return ICERR_BADFORMAT;
}


void CodecInst::ConvertRGB24toYUY2(const unsigned char* src, unsigned char* dst, int width, int height) {
  const int cyb = int(0.114*219/255*65536+0.5);
  const int cyg = int(0.587*219/255*65536+0.5);
  const int cyr = int(0.299*219/255*65536+0.5);

  for (int row = 0; row < height; ++row) {
    const unsigned char* rgb = src + width * 3 * (height-1-row);
    unsigned char* yuv = dst + width * 2 * row;
    for (int col = 0; col < width; col += 2) {
      // y1 and y2 can't overflow
      int y1 = (cyb*rgb[0] + cyg*rgb[1] + cyr*rgb[2] + 0x108000) >> 16;
      yuv[0] = y1;
      int y2 = (cyb*rgb[3] + cyg*rgb[4] + cyr*rgb[5] + 0x108000) >> 16;
      yuv[2] = y2;
      int scaled_y = (y1+y2 - 32) * int(255.0/219.0*32768+0.5);
      int b_y = ((rgb[0]+rgb[3]) << 15) - scaled_y;
      yuv[1] = Clip((b_y >> 10) * int(1/2.018*1024+0.5) + 0x800000);  // u
      int r_y = ((rgb[2]+rgb[5]) << 15) - scaled_y;
      yuv[3] = Clip((r_y >> 10) * int(1/1.596*1024+0.5) + 0x800000);  // v
      yuv += 4;
      rgb += 6;
    }
  }
}


DWORD CodecInst::CompressEnd() {
  if (yuy2_buffer) {
    delete[] yuy2_buffer;
    yuy2_buffer = 0;
  }
  if (median_buffer) {
    delete[] median_buffer;
    median_buffer = 0;
  }
  if (rgb_buffer) {
    delete[] rgb_buffer;
    rgb_buffer = 0;
  }
  return ICERR_OK;
}


/********************************************************************
********************************************************************/

static bool CanDecompress(LPBITMAPINFOHEADER lpbiIn) {
  int intype = GetBitmapType(lpbiIn);
  if (intype < 0)
    return IsLegalMethod(GetMethod(lpbiIn), intype != -1);
  else
    return (intype == 1 || intype == 2);
}


static bool CanDecompress(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  if (!lpbiOut)
    return CanDecompress(lpbiIn);

  // ccesp sets negative output height with YUY2 - a request for a top-down bitmap.
  // but what it really wants is a bottom-up bitmap. this is fine with huffyuv, so we change it. - bastel
  lpbiOut->biHeight = IgnoreTopDown() ? abs(lpbiOut->biHeight) : lpbiOut->biHeight;

  // must be 1:1 (no stretching)
  if (lpbiOut && (lpbiOut->biWidth != lpbiIn->biWidth || lpbiOut->biHeight != lpbiIn->biHeight))
    return false;

  int intype = GetBitmapType(lpbiIn);
  int outtype = GetBitmapType(lpbiOut);

  if (intype < 0) {
    if (!IsLegalMethod(GetMethod(lpbiIn), intype != -1))
      return false;
  }

  switch (intype) {
    case -1:
      // YUY2, RGB-24, RGB-32 output for compressed YUY2
      return (outtype == 1 || outtype == 3 || outtype == 4);
    case -2: case 1: case 2:
      // RGB-24, RGB-32 output only for YUY2/UYVY and compressed RGB
      return (outtype == 3 || outtype == 4);
    case -3:
      // RGB-32 output for compressed RGBA
      return (outtype == 4);
    default:
      return false;
  }
}

/********************************************************************
********************************************************************/


DWORD CodecInst::DecompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  Msg("DecompressQuery: input = %s, output = %s\n", &PrintBitmapType(lpbiIn), &PrintBitmapType(lpbiOut));
  return CanDecompress(lpbiIn, lpbiOut) ? ICERR_OK : ICERR_BADFORMAT;
}


// This function should return "the output format which preserves the most
// information."  However, I now provide the option to return RGB format
// instead, since some programs treat the default format as the ONLY format.

DWORD CodecInst::DecompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {

  // if lpbiOut == NULL, then return the size required to hold an output format struct
  if (lpbiOut == NULL)
    return sizeof(BITMAPINFOHEADER);

  if (!CanDecompress(lpbiIn))
    return ICERR_BADFORMAT;

  *lpbiOut = *lpbiIn;
  lpbiOut->biSize = sizeof(BITMAPINFOHEADER);
  lpbiOut->biPlanes = 1;

  int intype = GetBitmapType(lpbiIn);
  if (intype == -3) {
    lpbiOut->biBitCount = 32;   // RGBA
    lpbiOut->biCompression = 0;
    lpbiOut->biSizeImage = lpbiIn->biWidth * lpbiIn->biHeight * 4;
  } else if (intype == -2 || intype == 1 || intype == 2 || SuggestRGB()) {
    lpbiOut->biBitCount = 24;   // RGB
    lpbiOut->biCompression = 0;
    lpbiOut->biSizeImage = lpbiIn->biWidth * lpbiIn->biHeight * 3;
  } else {
    lpbiOut->biBitCount = 16;       // YUY2
    lpbiOut->biCompression = FOURCC_YUY2;
    lpbiOut->biSizeImage = lpbiIn->biWidth * lpbiIn->biHeight * 2;
  }

  return ICERR_OK;
}


const unsigned char* InitializeDecodeTable(const unsigned char* hufftable, unsigned char* shift, DecodeTable* decode_table) {
  unsigned add_shifted[256];
  hufftable = InitializeShiftAddTables(hufftable, shift, add_shifted);
  char code_lengths[256];
  char code_firstbits[256];
  char table_lengths[32];
  memset(table_lengths,-1,32);
  int all_zero_code=-1;
  for (int i=0; i<256; ++i) {
    if (add_shifted[i]) {
      for (int firstbit=31; firstbit>=0; firstbit--) {
        if (add_shifted[i] & (1<<firstbit)) {
          code_firstbits[i] = firstbit;
          int length = shift[i] - (32-firstbit);
          code_lengths[i] = length;
          table_lengths[firstbit] = max(table_lengths[firstbit], length);
          break;
        }
      }
    } else {
      all_zero_code = i;
    }
  }
  unsigned char* p = decode_table->table_data;
  *p++ = 31;
  *p++ = all_zero_code;
  for (int j=0; j<32; ++j) {
    if (table_lengths[j] == -1) {
      decode_table->table_pointers[j] = decode_table->table_data;
    } else {
      decode_table->table_pointers[j] = p;
      *p++ = j-table_lengths[j];
      p += 1<<table_lengths[j];
    }
  }
  for (int k=0; k<256; ++k) {
    if (add_shifted[k]) {
      int firstbit = code_firstbits[k];
      int val = add_shifted[k] - (1<<firstbit);
      unsigned char* table = decode_table->table_pointers[firstbit];
      memset(&table[1+(val>>table[0])],
        k, 1<<(table_lengths[firstbit]-code_lengths[k]));
    }
  }
  return hufftable;
}


const unsigned char* InitializeDecodeTables(const unsigned char* hufftable) {
  hufftable = InitializeDecodeTable(hufftable, decode1_shift, &decode1);
  hufftable = InitializeDecodeTable(hufftable, decode2_shift, &decode2);
  hufftable = InitializeDecodeTable(hufftable, decode3_shift, &decode3);
  return hufftable;
}


DWORD CodecInst::DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  Msg("DecompressBegin: input = %s, output = %s\n", &PrintBitmapType(lpbiIn), &PrintBitmapType(lpbiOut));

  DecompressEnd();  // free resources if necessary

  if (!CanDecompress(lpbiIn, lpbiOut))
    return ICERR_BADFORMAT;

  // ccesp keeps the in CanDecompress() overwritten output height atm,
  // so we are gonna have postive height in CodecInst::Decompress().
  // but to be sure it is stored in the instance here and accessed in CodecInst::Decompress() - bastel
  ignoretopdown = IgnoreTopDown();
  Msg("DecompressBegin: Top-Down output requests are %s.\n",
	  ignoretopdown ? "converted to bottum-up" : "processed normal");

  decompressing = true;

  int intype = GetBitmapType(lpbiIn);
  int outtype = GetBitmapType(lpbiOut);
  int method = GetMethod(lpbiIn);

  if (intype < 0) {
    InitializeDecodeTables(GetEmbeddedHuffmanTable(lpbiIn));
    decode_table_owner = this;
  }

  // load field threshold and decide if interlaced or progressive processing
  field_threshold     = GetPrivateProfileInt("general", "field_threshold", FIELD_THRESHOLD, "huffyuv.ini");
  ignoreinterlaceflag = !!GetPrivateProfileInt("general", "ignore_iflag", IGNORE_IFLAG, "huffyuv.ini");
  isinterlaced        = abs(lpbiIn->biHeight) > field_threshold;
  if ( !ignoreinterlaceflag ) {
	  unsigned char iflag = (*((unsigned char*)lpbiIn + sizeof(BITMAPINFOHEADER) + 2) >> 4) & 0xf;
	  isinterlaced = iflag==INTERLACED_ENCODING ? true : iflag==PROGRESSIVE_ENCODING ? false : isinterlaced;
  }
  Msg("DecompressBegin: field threshold = %d lines, field processing %s\n", field_threshold,
	  isinterlaced ? "enabled" : "disabled");

  // allocate buffer if decompressing HFYU->YUY2->RGB
  if (intype == -1 && outtype >= 3)
    decompress_yuy2_buffer = new unsigned char[lpbiIn->biWidth * lpbiIn->biHeight * 2];

  InitClip();

  swapfields = !!GetPrivateProfileInt("debug", "decomp_swap_fields", false, "huffyuv.ini");

  return ICERR_OK;
}


static DWORD ConvertYUY2toRGB(LPBITMAPINFOHEADER lpbiOutput, void* _src, void* _dst) {
  const unsigned char* src = (const unsigned char*)_src;
  unsigned char* dst = (unsigned char*)_dst;
  int stride = lpbiOutput->biWidth * 2;
  const unsigned char* src_end = src + stride*lpbiOutput->biHeight;
  if (lpbiOutput->biBitCount == 32)
    mmx_YUY2toRGB32(src, dst, src_end, stride);
  else
    mmx_YUY2toRGB24(src, dst, src_end, stride);
  return ICERR_OK;
}

static DWORD ConvertUYVYtoRGB(LPBITMAPINFOHEADER lpbiOutput, void* _src, void* _dst) {
  const unsigned char* src = (const unsigned char*)_src;
  unsigned char* dst = (unsigned char*)_dst;
  int stride = lpbiOutput->biWidth * 2;
  const unsigned char* src_end = src + stride*lpbiOutput->biHeight;
  if (lpbiOutput->biBitCount == 32)
    mmx_UYVYtoRGB32(src, dst, src_end, stride);
  else
    mmx_UYVYtoRGB24(src, dst, src_end, stride);
  return ICERR_OK;
}


DWORD CodecInst::Decompress(ICDECOMPRESS* icinfo, DWORD dwSize) {

  // If you insert a Huffyuv-compressed AVI to a Premiere project and then
  // drag it on to the timeline, the following dialogue occurs:
  //
  // 1. Premiere calls ICDecompressBegin, asking Huffyuv to decompress
  //    to a bitmap with different dimensions than the compressed frame.
  //
  // 2. Huffyuv can't resize, so it returns ICERR_BADFORMAT.
  //
  // 3. Premiere calls ICDecompress without making another call to
  //    ICDecompressBegin.
  //
  // Therefore I now check for this case and compensate for Premiere's
  // negligence by making the DecompressBegin call myself.

  if (!decompressing) {
    DWORD retval = DecompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput);
    if (retval != ICERR_OK)
      return retval;
  }

  // we change a top-down output request to a bottom-up one here. - bastel
  // (imho it should be possible to always do this, this code should never be reached with a negative height)
  if ( ignoretopdown ) icinfo->lpbiOutput->biHeight = abs(icinfo->lpbiOutput->biHeight);

  icinfo->lpbiOutput->biSizeImage = (icinfo->lpbiOutput->biWidth * icinfo->lpbiOutput->biHeight * icinfo->lpbiOutput->biBitCount) >> 3;

  int intype = GetBitmapType(icinfo->lpbiInput);
  if (intype < 0) {
    int method = GetMethod(icinfo->lpbiInput);
    int outtype = GetBitmapType(icinfo->lpbiOutput);

    if (decode_table_owner != this) {
      InitializeDecodeTables(GetEmbeddedHuffmanTable(icinfo->lpbiInput));
      decode_table_owner = this;
    }

    const unsigned long* const in = (unsigned long*)icinfo->lpInput;
    unsigned char* const out = (unsigned char*)icinfo->lpOutput;

    if (intype == -1) {   // decompressing HFYU16
      int stride = icinfo->lpbiOutput->biWidth * 2;
      const int size = stride * icinfo->lpbiOutput->biHeight;
      // if image is interlaced, double stride so fields are treated separately
      if ( isinterlaced ) stride *= 2;

      unsigned char* const yuy2 = outtype>1 ? decompress_yuy2_buffer : out;

      if (method == methodMedian) {
        asm_DecompressHFYU16(in, yuy2, yuy2+size);
        asm_MedianRestore(yuy2, yuy2+size, stride);
      }
      else {
        asm_DecompressHFYU16Delta(in, yuy2, yuy2+size);
        if (method == methodGrad)
          mmx_RowAccum(yuy2, yuy2+size, stride);
      }

      if (outtype>1)    // HFYU16->RGB
        ConvertYUY2toRGB(icinfo->lpbiOutput, yuy2, out);
    }
    else {   // decompressing HFYU24/HFYU32
      int stride = (icinfo->lpbiOutput->biWidth * icinfo->lpbiOutput->biBitCount) >> 3;
      const int size = stride * icinfo->lpbiOutput->biHeight;
      // if image is interlaced, double stride so fields are treated separately
	  if ( isinterlaced ) stride *= 2;
      if (intype == -2) {   // HFYU24->RGB
        if (outtype == 4) {
          if (method == methodLeft || method == methodOld)
            asm_DecompressHFYU24To32Delta(in, out, out+size);
          else
            asm_DecompressHFYU24To32DeltaDecorrelate(in, out, out+size);
        } else {
          if (method == methodLeft || method == methodOld)
            asm_DecompressHFYU24To24Delta(in, out, out+size);
          else
            asm_DecompressHFYU24To24DeltaDecorrelate(in, out, out+size);
        }
      }
      else if (intype == -3) {    // HFYU32->RGBA
        if (method == methodLeft || method == methodOld)
          asm_DecompressHFYU32To32Delta(in, out, out+size);
        else
          asm_DecompressHFYU32To32DeltaDecorrelate(in, out, out+size);
      }
      else
        return ICERR_BADFORMAT;

      if ((method&~flagDecorrelate) == methodGrad)
        mmx_RowAccum(out, out+size, stride);
    }

    if (swapfields && isinterlaced )
      asm_SwapFields(out, out+icinfo->lpbiOutput->biSizeImage,
       (icinfo->lpbiOutput->biWidth * icinfo->lpbiOutput->biBitCount) >> 3);

    return ICERR_OK;
  }
  else if (intype == 1) {   // YUY2->RGB
    return ConvertYUY2toRGB(icinfo->lpbiOutput, icinfo->lpInput, icinfo->lpOutput);
  }
  else if (intype == 2) {   // UYVY->RGB
    return ConvertUYVYtoRGB(icinfo->lpbiOutput, icinfo->lpInput, icinfo->lpOutput);
  }
  else
    return ICERR_BADFORMAT;
}


// palette-mapped output only
DWORD CodecInst::DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  return ICERR_BADFORMAT;
}


DWORD CodecInst::DecompressEnd() {
  if (decompress_yuy2_buffer) {
    delete[] decompress_yuy2_buffer;
    decompress_yuy2_buffer = 0;
  }
  decompressing = false;
  return ICERR_OK;
}
