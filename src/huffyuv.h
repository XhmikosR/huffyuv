//
// Huffyuv v2.1.1, by Ben Rudiak-Gould.
// http://www.math.berkeley.edu/~benrg/huffyuv.html
//
// This file is copyright 2000 Ben Rudiak-Gould, and distributed under
// the terms of the GNU General Public License, v2 or later.  See
// http://www.gnu.org/copyleft/gpl.html.
//
// I edit these files in 10-point Verdana, a proportionally-spaced font.
// You may notice formatting oddities if you use a monospaced font.
//


#include <windows.h>
#include <vfw.h>
#pragma hdrstop


static const DWORD FOURCC_HFYU = mmioFOURCC('H','F','Y','U');   // our compressed format
static const DWORD FOURCC_YUY2 = mmioFOURCC('Y','U','Y','2');   // uncompressed YUY2
static const DWORD FOURCC_UYVY = mmioFOURCC('U','Y','V','Y');   // uncompressed UYVY
static const DWORD FOURCC_VYUY = mmioFOURCC('V','Y','U','Y');   // an alias for YUY2 used by ATI cards

static const int	FIELD_THRESHOLD      = 288;					// old field threshold used by huffyuv
static const int	FULL_SIZE_BUFFER     = 0;					// use worst case scenario buffer by default?
static const int    IGNORE_IFLAG         = 0;
static const unsigned char PROGRESSIVE_ENCODING = 0x2;
static const unsigned char INTERLACED_ENCODING  = 0x1;


extern HMODULE hmoduleHuffyuv;


// huffyuv.cpp

struct CodecInst {
  unsigned char* yuy2_buffer;
  unsigned char* median_buffer;
  unsigned char* rgb_buffer;
  unsigned char* decompress_yuy2_buffer;
  bool swapfields;
  bool decompressing;
  bool ignoretopdown;		// ignore top-down output requests and change them into bottom-up ones
  int  field_threshold;		// highest number of lines that will not trigger fieldmode	
  bool ignoreinterlaceflag; // ignore interlace flag on decompression?
  bool isinterlaced;		// indicator during decompression (and compression?)

  // methods
  CodecInst() : yuy2_buffer(0), median_buffer(0), rgb_buffer(0), decompress_yuy2_buffer(0), decompressing(false),
				ignoretopdown(false), field_threshold(FIELD_THRESHOLD), ignoreinterlaceflag(false), isinterlaced(false) {}

  BOOL QueryAbout();
  DWORD About(HWND hwnd);

  BOOL QueryConfigure();
  DWORD Configure(HWND hwnd);

  DWORD GetState(LPVOID pv, DWORD dwSize);
  DWORD SetState(LPVOID pv, DWORD dwSize);

  DWORD GetInfo(ICINFO* icinfo, DWORD dwSize);

  DWORD CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD Compress(ICCOMPRESS* icinfo, DWORD dwSize);
  DWORD CompressEnd();

  void ConvertRGB24toYUY2(const unsigned char* src, unsigned char* dst, int width, int height);

  DWORD DecompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DecompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD Decompress(ICDECOMPRESS* icinfo, DWORD dwSize);
  DWORD DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DecompressEnd();

/*
  DWORD DrawQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DrawBegin(ICDRAWBEGIN* icinfo, DWORD dwSize);
  DWORD Draw(ICDRAW* icinfo, DWORD dwSize);
  DWORD DrawEnd();
  DWORD DrawWindow(PRECT prc);
*/
};

CodecInst* Open(ICOPEN* icinfo);
DWORD Close(CodecInst* pinst);


extern "C" {
  // fixed pregenerated tables (in tables.cpp)
  extern const unsigned char left_yuv[], grad_yuv[], med_yuv[];
  extern const unsigned char left_rgb[], left_decorrelate_rgb[], grad_decorrelate_rgb[];
  extern const unsigned char classic_shift_luma[], classic_shift_chroma[];
  extern const unsigned char classic_add_luma[256], classic_add_chroma[256];

  // tables generated at runtime for compression/decompression
  extern unsigned char encode1_shift[256], encode2_shift[256], encode3_shift[256];
  extern unsigned encode1_add_shifted[256], encode2_add_shifted[256], encode3_add_shifted[256];
  struct DecodeTable {
    unsigned char* table_pointers[32];
    unsigned char table_data[129*25];
  };
  extern DecodeTable decode1, decode2, decode3;
  extern unsigned char decode1_shift[256], decode2_shift[256], decode3_shift[256];
};
