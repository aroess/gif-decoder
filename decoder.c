#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "index_stream.h"
#include "die.h"

typedef enum { 
    false, 
    true 
} bool;

typedef struct array { 
    int index;
    int *table[4096];
    int elem_length[4096];
} array;

void  array_push (array *, int[], int);
void  array_clear_to(array *, int);
void  array_print (array *);

void  print_index_stream (unsigned char[], int);
void  lwz_write_index_stream (array *, unsigned char[], int, int);
int   lwz_next_code (unsigned char[], int, unsigned char);


/* input: pointer to a index stream struct, byte stream, lwz minimum code length
   creates an array to hold the code table
   for each array entry a variable length array is allocated
   reads codes with length code_len from byte stream
   builds code table according to the lzw algorythm
   outputs indices to index stream */
void lzw_decode(index_stream *istr, unsigned char bytes[], int bytes_len, unsigned char lzw_min_code_len) {

    array code_table;
    code_table.index = 0;

    int *push_arr = malloc(sizeof(int));
    if (push_arr == NULL) die ("memory error preparing array element\n");
    int lwz_code;
    int lwz_code_prev;
    int K;

    unsigned char code_len = lzw_min_code_len + 1; // log_base_2((1 << lzw_min_code_len) + 2) + 1;
    const unsigned char init_code_len = code_len;
    int bit_index = 0;
  
    int istr_range  = 1000;
    istr->len = istr_range;
    int istr_offset = 0;
    istr->array_pointer = malloc(istr->len);
    if (istr->array_pointer == NULL) die ("memory error extending index stream\n");

    /* translate color table indices to code table, there can be a gap 
       between the last actual color index and the control codes */
    for (int i = 0; i < (1 << lzw_min_code_len); i++) {
        push_arr[0] = i;
        array_push(&code_table, push_arr, 1);
    }
    /* add control codes */
    push_arr[0] = -1;      /* CLEAR CODE */
    array_push(&code_table, push_arr, 1);
    push_arr[0] = -2;      /* END OF INFORMATION CODE */
    array_push(&code_table, push_arr, 1);
    
    /* skip first clear code */
    bit_index = bit_index + code_len;

    /* continue here if CODE table gets resetted */
    RESET:

    /* read first CODE */
    lwz_code = lwz_next_code(bytes, bit_index, code_len);
    bit_index = bit_index + code_len;

    /* output {CODE} to index stream */
    lwz_write_index_stream(&code_table, istr->array_pointer, lwz_code, istr_offset);
    istr_offset = istr_offset + 1;

    int cl; int *sp;

    while (true) {
        
        lwz_code_prev = lwz_code;
        lwz_code = lwz_next_code(bytes, bit_index, code_len); 
        bit_index = bit_index + code_len;


        /* break if {CODE} = EOI */
        if(lwz_code < code_table.index && code_table.table[lwz_code][0] == -2) {
            printf("EOI\n");
            break;
        } 

        /* if {CODE} = CC reset CODE table to initial state, reset code_len */
        if(lwz_code < code_table.index && code_table.table[lwz_code][0] == -1) { 
            //printf("RESETTING COLOR TABLE\n");
            array_clear_to(&code_table, (1 << lzw_min_code_len) + 2);
            code_len = init_code_len;
            goto RESET;
        }           
       
        /* if CODE is already in code table */
        if (lwz_code < code_table.index) {
            sp = code_table.table[lwz_code];
            cl = code_table.elem_length[lwz_code];
            
            /* output {CODE} to index stream, extend istr if necessary */
            if (istr_offset + cl + 1 > istr->len) {
                istr->len += istr_range;
                istr->array_pointer = realloc(istr->array_pointer, istr->len);
                if (istr->array_pointer == NULL) die ("memory error extending index stream\n");
                //printf("extended indexstream to %d\n", istr->len);
            }
            lwz_write_index_stream(&code_table, istr->array_pointer, lwz_code, istr_offset);
            istr_offset = istr_offset + cl;

            /* let K be the first index in {CODE} */
            K = sp[0];

            /* add {CODE-1}+K to the code table */
            if(code_table.index < 4096) {             
                sp = code_table.table[lwz_code_prev];
                cl = code_table.elem_length[lwz_code_prev];
                push_arr = realloc(push_arr, sizeof(int) * (cl+1));
                if (push_arr == NULL) die ("memory error preparing array element\n");
                for(int i = 0; i < cl; i++) push_arr[i] = sp[i];
                push_arr[cl] = K;  
                array_push(&code_table, push_arr, (cl+1));
            }

        /* if CODE is larger than array length it can't be element of the array! */
        } else {
            sp = code_table.table[lwz_code_prev];
            cl = code_table.elem_length[lwz_code_prev];

            /* let K be the first index of {CODE-1} */
            K = sp[0];

            /* add {CODE-1}+K to code table */
            if(code_table.index < 4096) { 
                push_arr = realloc(push_arr, sizeof(int) * (cl+1));
                if (push_arr == NULL) die ("memory error preparing array element\n");
                for(int i = 0; i < cl; i++) push_arr[i] = sp[i];
                push_arr[cl] = K;
                array_push(&code_table, push_arr, (cl+1));
            }

            /* output {CODE} to index stream, extend istr if necessary */
            if (istr_offset + cl + 1 > istr->len) {
                istr->len += istr_range;
                istr->array_pointer = realloc(istr->array_pointer, istr->len);
                if (istr->array_pointer == NULL) die ("memory error extending index stream\n");
                //printf("extended indexstream to %d\n", istr->len);
            }
            lwz_write_index_stream(&code_table, istr->array_pointer, lwz_code, istr_offset);
            istr_offset = istr_offset + (cl+1);

        }

        /* if largest array index can't be represented with code_len
           bits increase code_len, max code_len is 12  according to 
           GIF specification) */
        if (code_len < 12 && ((1<<code_len) == code_table.index)) code_len++;

    }

    //print_index_stream(istr->array_pointer, istr->len);

    /* clear array */
    array_clear_to(&code_table, 0);
    free(push_arr);
    /* set the correct index stream length */
    istr->len = istr_offset;
}

/* allocates new array with (sizeof(int) * len) bytes
   pushes new node into array */
void array_push(array *a, int input[], int len) {
    int *p = malloc(sizeof(int) * len);
    if (p == NULL) die("memory error while appending array\n");
    memcpy(p, input, sizeof(int) * len);
    a->table[a->index] = p;
    a->elem_length[a->index] = len;
    a->index++;
}

/* frees the top most array_pointer
   frees the top most node pointer
   updates the top pointer to the next element */ 
void array_clear_to(array *a, int to) {
    for(int i=a->index; i>to; i--) free(a->table[i-1]);
    a->index = to;
}


void array_print(array *a) {
    for (int i=0; i < a->index; i++) {
        for (int j=0; j<a->elem_length[i]; j++) {
            printf("%d ", a->table[i][j]);
        }
        printf("\n");
    }
}

/* print index stream */
void print_index_stream(unsigned char arr[], int len) {
    for (int i = 0; i < len; i++)
        printf("%d, ", arr[i]);
    printf("\b \b\n");
}

/* takes code table offset
   appends code_table[i] to index stream */
void lwz_write_index_stream(array *a, unsigned char to[], int index, int output_offset) {
    int *np = a->table[index];
    for(int i = 0; i < a->elem_length[index]; i++) {
        to[output_offset+i] = np[i];
    }
}

/* reads next code_len bits from byte stream */
int lwz_next_code (unsigned char bytes[], int bit_index, unsigned char code_len) {
    /* do you believe in magic? */
    unsigned int all;
    char offset = bit_index % 8;
    int bi = bit_index/8;

    *((unsigned char*)&all)   = bytes[bi]; 
    *((unsigned char*)&all+1) = bytes[bi+1]; 
    *((unsigned char*)&all+2) = bytes[bi+2];
    return (all >> offset) & ((1 << code_len) - 1);
}
