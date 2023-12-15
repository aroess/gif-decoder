#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "index_stream.h"
#include "color.h"
#include "die.h"

char *generate_filename(char *, char *);

void write_ppm(color GLOBAL_COLOR_TABLE[], index_stream *istr, int gif_width, int gif_height, int frame_count, char *filename) {

    char write_buffer[20];

    snprintf(write_buffer, log(frame_count)/log(10) + 2, "%d", frame_count);
    filename = generate_filename(filename, "_frame\0");
    filename = generate_filename(filename, write_buffer);
    filename = generate_filename(filename, ".ppm\0");

    FILE *write_fp;
    write_fp = fopen(filename,"wb"); 
    if (write_fp == NULL) die ("Can't open output file");

    /* P6{LF}
       #{LF} -> optional comment*/
    memcpy(write_buffer, "P6", 2);
    write_buffer[2] = 0x0a;
    /* write_buffer[3] = 0x23; */
    /* write_buffer[4] = 0x0a; */
    fwrite(write_buffer, 1, 3, write_fp);    
 
    /* WIDTH {SPACE} HEIGHT */
    snprintf(write_buffer, log(gif_width)/log(10) + 2, "%d", gif_width); // no digits (log/log)+1 plus '\0' 
    fwrite(write_buffer, 1, 3, write_fp);

    write_buffer[0] = 0x20;
    fwrite(write_buffer, 1, 1, write_fp); 

    snprintf(write_buffer, log(gif_height)/log(10) + 2, "%d", gif_height);
    fwrite(write_buffer, 1, 3, write_fp);

    /* {LF}
       255
       {LF} */
    write_buffer[0] = 0x0a;
    write_buffer[1] = 2 + '0';
    write_buffer[2] = 5 + '0';
    write_buffer[3] = 5 + '0';
    write_buffer[4] = 0x0a;  
    fwrite(write_buffer, 1 , 5, write_fp); 
    
    int ct_index;
    for (int i = 0; i < istr->len; i++) {
        ct_index = istr->array_pointer[i];
        for (int j = 0; j < 3; j++) {
            write_buffer[0] = GLOBAL_COLOR_TABLE[ct_index].r;
            write_buffer[1] = GLOBAL_COLOR_TABLE[ct_index].g;
            write_buffer[2] = GLOBAL_COLOR_TABLE[ct_index].b;
        }
        fwrite(write_buffer, 1 , 3, write_fp);
    }
    
    // CLOSE WRITE FILE HANDLE
    fclose(write_fp);
    free(filename);
}

char *generate_filename(char *filename, char *append) {
    char *new_str;
    if((new_str = malloc(strlen(filename) + strlen(append) + 1)) != NULL){
        new_str[0] = '\0';   // ensures the memory is an empty string
        strcat(new_str, filename);
        strcat(new_str, append);
        return new_str;
    } else {
        printf("malloc failed!\n");
        exit(1);
    }
}
