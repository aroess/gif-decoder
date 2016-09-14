#ifndef LZW_DECODE_GUARD
#define LZW_DECODE_GUARD

void lzw_decode(index_stream *, unsigned char[], int, unsigned char);
void die(const char *);

#endif
