#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "util.h"

/*
 *  Relative encoding
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
    // Is 230 and therefore not worth continuing since it will require the same
    // number of bits to encode.
    printf("Diff: %d\n", max - min);
}
void decomp_relative(FILE *infp, FILE *outfp) {}



/*
    Basic run length encoding.
    Ratio: 99.48754
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
            if (count == 1) {
                fputc((char) last, outfp);
            }
            else if (count == 2) {
                fputc((char) last, outfp);
                fputc((char) last, outfp);
                count = 1;
            }
            else {
                // Put escape char then count then char
                fputc((char) 0x0, outfp);
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
    if (count == 1) {
        fputc((char) last, outfp);
    }
    else if (count == 2) {
        fputc((char) last, outfp);
        fputc((char) last, outfp);
    }
    else {
        // Place length of encoding followed by char
        fputc((char) 0x0, outfp);
        fputc((char) count, outfp);
        fputc((char) last, outfp);
        count = 1; // reset count
    }
    
}


void decomp_rle(FILE *infp, FILE *outfp) {
    int ch;
    unsigned int count, i;
    while ((count = fgetc(infp)) != EOF) {
        // Only one character
        if (count == 0) {
            count = fgetc(infp);
            ch = fgetc(infp); // Char to write
            for (i=0; i < count; i++) {
                fputc(ch, outfp);
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
    void (*compress)(FILE *, FILE *) = comp_relative;
    void (*decompress)(FILE *, FILE *) = decomp_relative;
    if (argc == 1 || *argv[1] == 'c') {
        infp = fopen("enwik9-sm", "rb"); 
        outfp = fopen("enwik9-sm-comp", "wb");

        compress(infp, outfp);
    }
    else if (*argv[1] == 'd') {
        infp = fopen("enwik9-sm-comp", "rb");
        outfp = fopen("enwik9-sm-decomp", "wb");

        decompress(infp, outfp);
    }
    else {
        infp = fopen("enwik9-sm", "rb");
        outfp = fopen("enwik9-sm-decomp", "rb");
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




