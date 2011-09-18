extern "C" {

unsigned long* __cdecl asm_CompressYUY2(const unsigned char* src, unsigned long* dst, const unsigned char* src_end);
unsigned long* __cdecl asm_CompressYUY2Delta(const unsigned char* src, unsigned long* dst, const unsigned char* src_end);
unsigned long* __cdecl asm_CompressUYVY(const unsigned char* src, unsigned long* dst, const unsigned char* src_end);
unsigned long* __cdecl asm_CompressUYVYDelta(const unsigned char* src, unsigned long* dst, const unsigned char* src_end);

unsigned long* __cdecl asm_CompressRGBDelta(const unsigned char* src, unsigned long* dst, const unsigned char* src_end);
unsigned long* __cdecl asm_CompressRGBDeltaDecorrelate(const unsigned char* src, unsigned long* dst, const unsigned char* src_end);
unsigned long* __cdecl asm_CompressRGBADelta(const unsigned char* src, unsigned long* dst, const unsigned char* src_end);
unsigned long* __cdecl asm_CompressRGBADeltaDecorrelate(const unsigned char* src, unsigned long* dst, const unsigned char* src_end);

void __cdecl mmx_RowDiff(const unsigned char* src, unsigned char* dst, const unsigned char* src_end, int stride);

void __cdecl mmx_RowAccum(unsigned char* buf, unsigned char* buf_end, int stride);

void __cdecl mmx_MedianPredictYUY2(const unsigned char* src, unsigned char* dst, const unsigned char* src_end, int stride);
void __cdecl mmx_MedianPredictUYVY(const unsigned char* src, unsigned char* dst, const unsigned char* src_end, int stride);

void __cdecl asm_MedianRestore(unsigned char* buf, unsigned char* buf_end, int stride);

void __cdecl asm_DecompressHFYU16(const unsigned long* src, unsigned char* dst, unsigned char* dst_end);
void __cdecl asm_DecompressHFYU16Delta(const unsigned long* src, unsigned char* dst, unsigned char* dst_end);

void __cdecl asm_DecompressHFYU24To24Delta(const unsigned long* src, unsigned char* dst, unsigned char* dst_end);
void __cdecl asm_DecompressHFYU24To24DeltaDecorrelate(const unsigned long* src, unsigned char* dst, unsigned char* dst_end);
void __cdecl asm_DecompressHFYU24To32Delta(const unsigned long* src, unsigned char* dst, unsigned char* dst_end);
void __cdecl asm_DecompressHFYU24To32DeltaDecorrelate(const unsigned long* src, unsigned char* dst, unsigned char* dst_end);
void __cdecl asm_DecompressHFYU32To32Delta(const unsigned long* src, unsigned char* dst, unsigned char* dst_end);
void __cdecl asm_DecompressHFYU32To32DeltaDecorrelate(const unsigned long* src, unsigned char* dst, unsigned char* dst_end);

void __cdecl asm_SwapFields(unsigned char* buf, unsigned char* buf_end, int stride);

void __cdecl mmx_YUY2toRGB24(const unsigned char* src, unsigned char* dst, const unsigned char* src_end, int stride);
void __cdecl mmx_YUY2toRGB32(const unsigned char* src, unsigned char* dst, const unsigned char* src_end, int stride);
void __cdecl mmx_UYVYtoRGB24(const unsigned char* src, unsigned char* dst, const unsigned char* src_end, int stride);
void __cdecl mmx_UYVYtoRGB32(const unsigned char* src, unsigned char* dst, const unsigned char* src_end, int stride);

}
