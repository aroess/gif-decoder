#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "index_stream.h"
#include "color.h"

#include "die.h"
#include "decoder.h"
#include "write_ppm.h"

typedef enum { 
    false, 
    true 
} bool;

void read_color_table (color *, unsigned char[], short, FILE *, size_t);
void skip_extension_block (FILE *, unsigned char[], size_t);

int main (int argc, char *argv[]) {

    if (argc < 2) die ("USAGE: test <filename>");

    char *filename = argv[1];
    FILE *fp;
    unsigned char buffer[256]; // max IMAGE_DATA block size = 256
    size_t ret;

    fp = fopen(filename, "rb");
    if (fp == NULL) die ("Can't open input file");

    // CHECK GIF HEADER
    ret = fread(buffer, 1, 6, fp);
    if (ret < 6) die ("io read error");
    if (memcmp(buffer, "GIF89a", 6) == 0) printf("This is a GIF89a file\n");
    else die ("not a GIF89a file");
    
    // CHECK LOGICAL FILE DESCRIPTOR
    ret = fread(buffer, 1, 7, fp);
    if (ret < 7) die ("io read error");

    // LFD: WIDTH AND HIGHT -> assumes little endian, short = 2 bytes
    short gif_width = (short)(
        ((unsigned char)buffer[1]) << 8 | 
        ((unsigned char)buffer[0])
    );

    short gif_height = (short)(
        ((unsigned char)buffer[3]) << 8 | 
        ((unsigned char)buffer[2])
    );

    printf("Width:\t %d\n", gif_width);
    printf("Height:\t %d\n", gif_height);

    // LFD: PACKED FIELD
    bool gif_gctflag = (buffer[4] >> 7) & 1; // shift left 7 times, mask with 00000001
    printf("Global color table flag: %d\n", gif_gctflag);

    char gif_cres = (buffer[4] >> 4) & 7; // shift left 4 times, mask with 00000111
    printf("Color resolution in bits: %d\n", gif_cres);

    //bool gif_sflag = (buffer[4] >> 3 ) & 1;
    //printf("Sort flag: %d\n", gif_sflag);

    char gif_sizegct = buffer[4] & 7;
    if (gif_gctflag) printf("Size of global color table: %d\n", 2 << gif_sizegct);

    // CHECK BACKGROUND COLOR INDEX
    //char gif_bgcindex = buffer[5];
    //printf("Background color index: %d\n", gif_bgcindex);

    // CHECK PIXEL ASPECT RATIO
    //char gif_paratio = buffer[6];
    //printf("Pixel aspect ratio: %d\n", gif_paratio);

    // COLOR TABLE
    short ct_length;
    color *GLOBAL_COLOR_TABLE = NULL;
    if (gif_gctflag) {       
        ct_length = 2 << gif_sizegct;
        GLOBAL_COLOR_TABLE = malloc(sizeof(color) * ct_length);
        if (GLOBAL_COLOR_TABLE == NULL) die ("memory error");
        read_color_table(GLOBAL_COLOR_TABLE, buffer, ct_length, fp, ret);
    }

    color *LOCAL_COLOR_TABLE = NULL;
    int frame_count = 0;
    bool gif_lctflag;
    char gif_sizelct;

    // READ NEXT BYTE, could be an extension or image data
    ret = fread(buffer, 1, 1, fp);

    // WHILE NOT EOF
    while (buffer[0] != 0x3b) {

        // CHECK FOR EXTENSIONS
        while (buffer[0] == 0x21) {
            skip_extension_block (fp, buffer, ret);
            ret = fread(buffer, 1, 1, fp);
        }

        // There can be an extension block after the image data!
        if (buffer[0] == 0x3b) break;

        // CHECK IMAGE DESCRIPTOR (OPTIONAL)
        if (buffer[0] == 0x2c) {

            if ((buffer[0] >> 6) & 1) {
                die ("interlace flag found, not implemented!\n");
            }

            printf("image descriptor!\n");
            ret = fread(buffer, 1, 8, fp); // skip this for now
            ret = fread(buffer, 1, 1, fp);

            gif_lctflag = (buffer[0] >> 7) & 1;
            printf("Local color table flag: %d\n", gif_lctflag);    
            gif_sizelct = buffer[0] & 7;
            if (gif_lctflag) printf("Size of local color table: %d\n", 2 << gif_sizelct);

            
            if (gif_lctflag) {       
                LOCAL_COLOR_TABLE = malloc(sizeof(color) * (2 << gif_sizelct));
                if (LOCAL_COLOR_TABLE == NULL) die ("memory error");
                read_color_table(LOCAL_COLOR_TABLE, buffer, (2 << gif_sizelct), fp, ret);
            }

        }

        // READ IMAGE DATA
        ret = fread(buffer, 1, 2, fp);

        unsigned char gif_lzwmcsize = buffer[0];
        printf("LZW minimum code size: %d\n", gif_lzwmcsize);

        unsigned char sub_block_byte_length = buffer[1];
        unsigned char *IMAGE_DATA = NULL;
        int img_byte_length = 0;

        while (sub_block_byte_length) { 
            // allocate memory, size of char is always 1
            IMAGE_DATA = realloc(IMAGE_DATA, img_byte_length + sub_block_byte_length);
            // read bytes as indicated by the sub block file size, max block size = 256
            ret = fread(buffer, 1, sub_block_byte_length, fp);
            //printf("sub block byte length: %d\n", sub_block_byte_length);
            // store read bytes in array
            for (int i = 0; i < ret; i++) IMAGE_DATA[img_byte_length+i] = buffer[i];
            // keep track of entire image data byte length
            img_byte_length = img_byte_length + sub_block_byte_length;
            // read next sub block byte length, loop exists if 00 is read
            ret = fread(buffer, 1, 1, fp);
            sub_block_byte_length = buffer[0];
        }

        // DECODE IMAGE DATA
        struct index_stream istr;
        lzw_decode(&istr, IMAGE_DATA, img_byte_length, gif_lzwmcsize);
        free(IMAGE_DATA);

        // WRITE PPM FILE
        if (!gif_lctflag)
            write_ppm(GLOBAL_COLOR_TABLE, &istr, gif_width, gif_height, frame_count, filename);
        else
            write_ppm(LOCAL_COLOR_TABLE, &istr, gif_width, gif_height, frame_count, filename);

        // FREE VARIABLES
        free(istr.array_pointer);
        if (gif_lctflag) free(LOCAL_COLOR_TABLE);

        // RESET
        istr.len = 0;
        frame_count++;
        gif_lctflag = 0;
        img_byte_length = 0;

        // READ NEXT BYTE, used in condition for while loop 
        ret = fread(buffer, 1, 1, fp);

    }
    printf("EOF rechead\n");


    // CLOSE FILE HANDLER
    fclose(fp);
    
    return 0;

}

void skip_extension_block (FILE *fp, unsigned char buffer[], size_t ret) {
    ret = fread(buffer, 1, 1, fp); 
    unsigned char bytes_to_skip;
    switch (buffer[0]) {
        case 0xf9: printf("found graphics control extension!\n"); break;
        case 0x01: printf("found plain text extension!\n"); break;
        case 0xfe: printf("found comment extension!\n"); break;
        case 0xff: printf("found application extension!\n"); break;
        default: die ("unknown extension\n");
    }
    while (true) {
         ret = fread(buffer, 1, 1, fp); 
         bytes_to_skip = buffer[0];
         if (bytes_to_skip) {
             ret = fread(buffer, 1, bytes_to_skip, fp);
             //printf("skipped %d bytes\n", bytes_to_skip);
         } else { break; }
    }
}

void read_color_table (color GLOBAL_COLOR_TABLE[], unsigned char buffer[], short ct_length, FILE *fp, size_t ret) {
    for(int i = 0; i < ct_length; i++) {
        ret = fread(buffer, 1, 3, fp);
        if (ret < 3) die ("io error");
        GLOBAL_COLOR_TABLE[i].r = buffer[0];
        GLOBAL_COLOR_TABLE[i].g = buffer[1];
        GLOBAL_COLOR_TABLE[i].b = buffer[2];
    }
}
