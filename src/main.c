#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "util.h"

// Crash on failed assert
#define ASSERT(expr) if (!(expr)) {*(int*)0=0;}

/*
 *  Relative encoding
 *  
 *  NOTE: Does not work with stdin
 */
void comp_relative(FILE *infp, FILE *outfp) {
    int curr, last;
    int min = 255;
    int max = -256;
    last = fgetc(infp);
    // File is empty
    if (last == EOF) {
        return;
    }

    while ((curr = fgetc(infp)) != EOF) {
        int diff = curr - last;
        if (diff < min) {
            min = diff;
        }
        if (diff > max) {
            max = diff;
        }
    }

    if ((max - min) > 128) {
        rewind(infp);
        fputc(0x00, outfp);
        while ((curr = fgetc(infp)) != EOF) {
            fputc((char) curr, outfp);
        }
        return;
    } else {
        ASSERT(0);
    }
    
}
void decomp_relative(FILE *infp, FILE *outfp) {
    int curr = fgetc(infp);
    ASSERT(curr == 0x00);
    while ((curr = fgetc(infp)) != EOF) {
        fputc(curr, outfp);
    }
}



/*
    Basic run length encoding.
    Enwik9-sm Ratio: 99.48754 %
    Image Ratio: 106.88208 % (Using 0 as escape is no good)
*/
void comp_rle(FILE *infp, FILE *outfp) {
    int curr, last;
    int count = 1;
    last = fgetc(infp);
    // File is empty
    if (last == EOF)
        return;
    while ((curr = fgetc(infp)) != EOF) {
        // If char changes or count will overflow
        if (curr != last || count == 0xFF) {
            if (last != 0x00 && count == 1) {
                fputc((char) last, outfp);
            }
            else if (count == 1) {
                // Put two copies to escape a single escape
                fputc((char) 0x00, outfp);
                fputc((char) 0x00, outfp);
            }
            else if (last != 0x00 && count == 2) {
                // Don't run length encode 2 bytes unless it's 2 escape chars
                fputc((char) last, outfp);
                fputc((char) last, outfp);
                count = 1;
            }
            else {
                // Put escape char, then count, then char
                fputc((char) 0x00, outfp);
                fputc((char) count, outfp);
                fputc((char) last, outfp);
                count = 1; // reset count
            }
        }
        else {
            count++;
        }
        last = curr;
    }

    // Print last character
    if (last != 0x00 && count == 1) {
        fputc((char) last, outfp);
    } else if (count == 1) {
        fputc((char) 0x00, outfp);
        fputc((char) 0x00, outfp);
    }
    else if (last != 0x00 && count == 2) {
        fputc((char) last, outfp);
        fputc((char) last, outfp);
    }
    else {
        // Place length of encoding followed by char
        fputc((char) 0x00, outfp);
        fputc((char) count, outfp);
        fputc((char) last, outfp);
    }
    
}


void decomp_rle(FILE *infp, FILE *outfp) {
    int ch;
    unsigned int count, i;
    while ((count = fgetc(infp)) != EOF) {
        // Only one character
        if (count == 0x00) {
            count = fgetc(infp);
            if (count == 0x00) {
                fputc(0x00, outfp);
            }
            else {
                ch = fgetc(infp); // Char to write
                for (i=0; i < count; i++) {
                    fputc(ch, outfp);
                }
            }
        }
        else {
            fputc(count, outfp);
        }
    }
}


int main(int argc, char* argv[]) {
    FILE *infp; 
    FILE *outfp;
    void (*compress)(FILE *, FILE *) = comp_rle;
    void (*decompress)(FILE *, FILE *) = decomp_rle;
    char *baseFile = "image.bmp";
    char fname[1024];
    if (argc == 2 && *argv[1] == 'c') {
        printf("Compressing...\n");

        strcpy(fname, baseFile);
        printf("In file: %s\n", fname);
        infp = fopen(fname, "rb"); 

        strcat(fname, "-comp");
        printf("Out file: %s\n", fname);
        outfp = fopen(fname, "wb");

        ASSERT(infp != NULL && outfp != NULL);

        compress(infp, outfp);
        printf("Done.\n");
    }
    else if (argc == 2 && *argv[1] == 'd') {
        printf("Decompressing...\n");

        strcpy(fname, baseFile);
        strcat(fname, "-comp");
        printf("In file: %s\n", fname);
        infp = fopen(fname, "rb");

        strcpy(fname, baseFile);
        strcat(fname, "-decomp");
        printf("Out file: %s\n", fname);
        outfp = fopen(fname, "wb");

        ASSERT(infp != NULL && outfp != NULL);

        decompress(infp, outfp);
        printf("Done.\n");
    }
    else {
        printf("Comparing...\n");

        strcpy(fname, baseFile);
        infp = fopen(fname, "rb");
        printf("f1: %s\n", fname);

        strcat(fname, "-decomp");
        outfp = fopen(fname, "rb");
        printf("f2: %s\n", fname);

        ASSERT(infp != NULL && outfp != NULL);

        int i;
        if ((i = diff_file(infp, outfp))) {
            printf("Different at %d\n", i);
        }
        else {
            printf("Same!\n");
        }
    }
    fclose(infp);
    fclose(outfp);
    
    return (0);
}




