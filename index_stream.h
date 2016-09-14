#ifndef INDEX_STREAM_GUARD
#define INDEX_STREAM_GUARD

/* this struct is used in the main source file AND the decoder source file */
typedef struct index_stream {
    unsigned char *array_pointer;
    int len;
} index_stream;

#endif
