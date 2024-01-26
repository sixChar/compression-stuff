#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "util.h"

// Crash on failed assert
#define GET_MACRO(_1, _2, NAME,...) NAME

#define ASSERT1(expr) if (!(expr)) {*(int*)0=0;}
#define ASSERT2(expr, msg) if (!(expr)) {fprintf(stderr, msg); *(int*)0=0;}
#define ASSERT(...) GET_MACRO(__VA_ARGS__, ASSERT2, ASSERT1)(__VA_ARGS__)

typedef void (*TformPtr)(FILE*, FILE*);


typedef struct BMPFileHeader {
    int size;
    int imgOffset;
    int headSize;
    int width;
    int height;
    int bitsPerPixel;
} BMPFileHeader;

int readLittleEndian(FILE *fp, int offset, int nBytes) {
    ASSERT(nBytes <= 4, "Error: readLittleEndian only designed to read max 32 bit ints\n");
    int err = fseek(fp, offset, SEEK_SET);
    ASSERT(err==0, "Error reading file in readLittleEndian!");
    int res = 0;
    for (int i=0; i < nBytes; i++) {
        int c = getc(fp);
        ASSERT(c != EOF, "Error in readLittleEndian: End of file reached!\n");
        res |= (c << (8*i));
    }
    return res;
    
}

void readBMPHeader(FILE *fp, BMPFileHeader *h) {
    h->size = readLittleEndian(fp, 2, 4);
    h->imgOffset = readLittleEndian(fp, 10, 4);
    h->headSize = readLittleEndian(fp, 14, 4);
    ASSERT(h->headSize == 124, "Error in readBMPHeader: Header not a BITMAPV5HEADER.\n");
    h->width = readLittleEndian(fp, 18, 4);
    h->height = readLittleEndian(fp, 22, 4);
    h->bitsPerPixel = readLittleEndian(fp, 28, 2);
    fseek(fp, 0, SEEK_SET);
}




void rgbBMPTransform(FILE *infp, FILE *outfp) {
    BMPFileHeader h;
    readBMPHeader(infp, &h);
    ASSERT((h.width * h.bitsPerPixel/8) % 4 == 0, "Error in rgbBMPTransform: Row size not multiple of 4 bytes, handling padding not yet implemented.\n");
    ASSERT(h.bitsPerPixel == 24, "Error in rgbBMPTransform: support for bitsPerPixel other than 24 not implemented.\n")

    char *red = (char*) malloc(3 * h.width * h.height);
    char *blue = red + h.width * h.height;
    char *green = blue + h.width * h.height;

    for (int i=0; i < h.imgOffset; i++) {
        int c = fgetc(infp);
        ASSERT(c != EOF, "Error in rgbBMPTransform: Unexpected end of file in header.\n");
        fputc((char) c, outfp);
    }

    // Split into 3 color channels
    int rOff = 0;
    int gOff = 0;
    int bOff = 0;
    for (int i=0; i < h.height; i++) {
        for (int j=0; j < h.width; j++) {
            int b = fgetc(infp);
            int g = fgetc(infp);
            int r = fgetc(infp);
            ASSERT((b != EOF) && (g != EOF) && (r != EOF), "Error in rgbBMPTransform: Unexpected end of file in image data.\n");
            *(red+rOff) = (char) r;
            *(green+gOff) = (char) g;
            *(blue+bOff) = (char) b;
            rOff++;
            gOff++;
            bOff++;
        }
    }
    
    // Write the separated color channels sequentially (all red, then green, then blue)
    for (int i=0; i < 3 * h.width * h.height; i++) {
        char c = *(red+i);
        fputc(c, outfp);
    }

    // If there's anything else copy it over.
    int c;
    while ((c = fgetc(infp)) != EOF) {
        fputc((char) c, outfp);
    }

    free(red);

}



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
 * Simple run length encoding (that can handle 0 byte):
 *
 * Compression Ratios:
 * ==========================
 * enwik9-sm        1.00148
 * image.bmp        1.00088
 *
 * Note: Previously used 0 as a padding byte but this lead to pretty bad
 * expansion in image file. However the perfrmance was better on enwik9-sm.
 * This is because there are no 0 bytes in that file. This version handles
 * 0s more gracefully.
 */
void comp_rle(FILE *infp, FILE *outfp) {
    int curr, last;
    int count = 1;
    last = fgetc(infp);
    if (last == EOF) {
        return;
    }
    while ((curr = fgetc(infp)) != EOF) {
        if (curr != last || count == 0xff + 3) {
            if (count < 3) {
                fputc((char) last, outfp);
                if (count == 2) {
                    fputc((char) last, outfp);
                }
            } else {
                fputc((char) last, outfp);
                fputc((char) last, outfp);
                fputc((char) last, outfp);
                // n more repeats
                fputc((char) count - 3, outfp);
            }
            count = 1;
        } else {
            count++;
        }
        last = curr;
    }

    // Handle last char
    if (count < 3) {
        fputc((char) last, outfp);
        if (count == 2) {
            fputc((char) last, outfp);
        }
    } else {
        fputc((char) last, outfp);
        fputc((char) last, outfp);
        fputc((char) last, outfp);
        // n more repeats
        fputc((char) count - 3, outfp);
    }
}


void decomp_rle(FILE *infp, FILE *outfp) {
    int curr, last;
    int count=1;
    last = fgetc(infp);
    // empty file
    if (last == EOF) {
        return;
    }
    while ((curr = fgetc(infp)) != EOF) {
        if (count == 3) {
            fputc((char) last, outfp);
            fputc((char) last, outfp);
            fputc((char) last, outfp);
            for (int i=0; i < curr; i++) {
                fputc((char) last, outfp);
            }

            last = fgetc(infp);
            count = 1;
            if (last == EOF) {
                break;
            }
        } else if (last != curr) {
            fputc((char) last, outfp);
            if (count >= 2) {
                fputc((char) last, outfp);
            }
            count = 1;
            last = curr;
                
        } else {
            count++;
            last = curr;
        }
        
    } 

    // Check last symbol
    if (last != EOF) {
        fputc((char) last, outfp);
        if (count >= 2) {
            fputc((char) last, outfp);
        }
        // For a properly formatted compressed file, we should never get to the end
        // of the character stream with a count of 3. This would need to be followed
        // by the number of extra characters even if that number is 0. The loop would
        // have caught this.
        ASSERT(count < 3);
    }
}



void testCompression(char *baseFile, TformPtr compress, TformPtr decompress) {
    FILE *infp;
    FILE *outfp;

    char fname[1024];

    long int baseBytes;
    long int compBytes;
    
    // Compress file
    printf("Compressing file...\n");
    // set fname to "base.file"
    strcpy(fname, baseFile);
    infp = fopen(fname, "rb");

    // set fname to "base.file-comp"
    strcat(fname, "-comp");
    outfp = fopen(fname, "wb");

    compress(infp, outfp);

    // Make sure at end of files and count bytes
    fseek(infp, 0, SEEK_END);
    fseek(outfp, 0, SEEK_END);
    baseBytes = ftell(infp);
    compBytes = ftell(outfp);

    fclose(outfp);
    fclose(infp);

    // Decompress file
    printf("Decompressing file...\n");
    // fname is still "base.file-comp"
    infp = fopen(fname, "rb");
    
    // set fname to "base.file-decomp"
    strcpy(fname, baseFile);
    strcat(fname, "-decomp");
    outfp = fopen(fname, "wb");

    decompress(infp, outfp);

    fclose(infp);
    fclose(outfp);

    // Test correctness
    printf("Checking for differences between base and decompressed...\n");
    // fname is still "base.file-decomp"
    outfp = fopen(fname, "rb");
    
    // set fname to "base.file"
    strcpy(fname, baseFile);
    infp = fopen(fname, "rb");

    int i;
    if ((i = diff_file(infp, outfp))) {
        printf("ERROR: Files different at %d\n", i);
    }
    else {
        printf("Same!\n");
        printf("Compression ratio: %.5Lf\n", (long double) baseBytes / (long double) compBytes);
    }

    fclose(infp);
    fclose(outfp);
}


int main(int argc, char* argv[]) {
    FILE *infp; 
    FILE *outfp;
    TformPtr compress = comp_rle;
    TformPtr decompress = decomp_rle;
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

        fclose(infp);
        fclose(outfp);
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

        fclose(infp);
        fclose(outfp);
    }
    else if (argc == 2 && *argv[1] == 't') {
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

        fclose(infp);
        fclose(outfp);
    }
    else {
        //printf("Testing file %s:\n", baseFile);
        //testCompression(baseFile, compress, decompress);
        BMPFileHeader head;
        infp = fopen("image.bmp", "rb");
        outfp = fopen("image.bmp-split", "wb");
        //printf("Size: %d\n", head.size);
        //printf("Offset: %d\n", head.imgOffset);
        //printf("Image width: %d\n", head.width);
        //printf("Image height: %d\n", head.height);
        //printf("Bits per Pixel: %d\n", head.bitsPerPixel);
        
        rgbBMPTransform(infp, outfp);
    }

    
    return (0);
}




