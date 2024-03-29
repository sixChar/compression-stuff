#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "util.h"

#define GET_MACRO(_1, _2, NAME,...) NAME

// Crash on failed assert
#define ASSERT1(expr) if (!(expr)) {*(int*)0=0;}
#define ASSERT2(expr, msg) if (!(expr)) {fprintf(stderr, msg); *(int*)0=0;}
#define ASSERT(...) GET_MACRO(__VA_ARGS__, ASSERT2, ASSERT1)(__VA_ARGS__)

#define IMG_QUANT_FAC 16

#define NUM_HUFF_SYMS 256
// Know we need 2n - 1 nodes for a tree with n leaves and no half-filled nodes
#define NUM_HUFF_NODES (NUM_HUFF_SYMS * 2 - 1)

/*
 *  Compression Ratios:
 *  Single character counts Huffman Encoding (enwik-9-sm): 1.5595
 *
 *
 *
 *
 */


typedef uint8_t u8;

typedef struct BitFile {
    FILE* fp;
    char buffer;
    u8 count;
    
} BitFile;


int bfRead(BitFile* bfp) {
    if (bfp->count == 0) {
        int c = fgetc(bfp->fp);
        if (c == EOF) {
            return c;
        }
        bfp->buffer = (char) c;
        bfp->count = 8;
    }
    bfp->count--;
    return (int) ((bfp->buffer >> bfp->count) & 1);
}


void bfWrite(char c, BitFile* bfp) {
    // Set next buffer bit on if c is not 0
    bfp->buffer |= (((c != 0) << (7-bfp->count)));
    bfp->count++;
    if (bfp->count == 8) {
        fputc(bfp->buffer, bfp->fp);
        bfp->buffer = 0x00;
        bfp->count = 0;
    }
}


void bfWriteClose(BitFile* bfp) {
    // Force caller to handle padding
    ASSERT(bfp->count == 0, "Bit files must end on byte alignment.\n");
    fclose(bfp->fp);
}

void bfReadClose(BitFile* bfp) {
    fclose(bfp->fp);
}


void bfOpen(BitFile* bfp, const char* fname, const char* mode) {
    bfp->fp = fopen(fname, mode);
    bfp->buffer = 0x00;
    bfp->count = 0;
}


void bfFromFilePtr(BitFile* bfp, FILE* fp) {
    bfp->fp = fp;
    bfp->buffer = 0x00;
    bfp->count = 0;
}


u8 getIthBit(u8 *bits, int i) {
    return *(bits+i/8) & (0x80 >> (i%8));
}


typedef void (*TformPtr)(FILE*, FILE*);


typedef struct BMPFileHeader {
    int size;
    int imgOffset;
    int headSize;
    int width;
    int height;
    int bitsPerPixel;
} BMPFileHeader;




void rangeArr(int n, int* arr) {
    for (int i=0; i < n; i++) {
        arr[i] = i;
    }
}

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


void writeInt32(FILE *fp, int toWrite) {
    ASSERT(sizeof(int) == 4, "Error: Program assumes 'int' type is a 32 bit integer. The program needs refactoring if this is not the case\n");
    // Note little endian
    fputc((char) toWrite & 0xFF, fp);
    fputc((char) (toWrite >> 8) & 0xFF, fp);
    fputc((char) (toWrite >> 16) & 0xFF, fp);
    fputc((char) (toWrite >> 24) & 0xFF, fp);

}


int readInt32(FILE *fp) {
    ASSERT(sizeof(int) == 4, "Error: Program assumes 'int' type is a 32 bit integer. The program needs refactoring if this is not the case\n");
    int res = 0;
    int c;
    // Note: little endian
    for (int i=0; i < 4 && (c = fgetc(fp)) != EOF; i++) {
        res |= (c << 8*i);
    }
    ASSERT(c != EOF, "Error: End of file reached while reading int");
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

void copyBMPHeader(FILE* infp, FILE* outfp, BMPFileHeader* h) {
    for (int i=0; i < h->imgOffset; i++) {
        int c = fgetc(infp);
        ASSERT(c != EOF, "Error: Unexpected end of file in header.\n");
        fputc((char) c, outfp);
    }
}

void copyRemaining(FILE* infp, FILE* outfp) {
    int c;
    while ((c = fgetc(infp)) != EOF) {
        fputc((char) c, outfp);
    }
}


/* TODO delete once huffman refactor done



*
 *  
 *
 *
void printHuffTable(HuffTable* table) {
    printf("Huff table:\n");
    printf("Num entries: %d\n", table->nSym);
    printf("Entry size: %d\n", table->entrySize);
    printf("Codes:\n");
    for (int i=0; i < table->nSym; i++) {
        printf("  %i (length %i): ", i, table->codeLens[i]);
        for (int j=0; j < table->codeLens[i]; j++) {
            if ((table->codes[i*table->entrySize + j/8] & (0x80 >> (j%8))) == 0) {
                printf("0");
            } else {
                printf("1");
            }
        }
        printf("\n");
    }
}


void dumpHuffTable(HuffTable* table) {
    // HuffTable: 
    //  int nSym;
    //  int entrySize;
    //  u8* codes;
    //  int* codeLens;
    printf("%d\n", table->nSym);
    printf("%d\n", table->entrySize);
    for (int i=0; i < table->nSym; i++) {
        printf("%d\n", table->codeLens[i]);
        for (int j = 0; j < table->entrySize; j++) {
            printf("%x ", table->codes[table->entrySize * i + j]);
        }
        printf("\n");
    }
}


void printHuffTree(HuffNode* root, int tabs) {
    if (root != 0) {
        if (root->left == 0 && root->right == 0) {
            for (int i=0; i < tabs; i++) {
                printf("  ");
            }
            printf("weight: %.4f  sym: %d\n", root->weight, root->sym);
        } else {
            for (int i=0; i < tabs; i++) {
                printf("  ");
            }
            printf("weight: %.4f\n", root->weight);
            for (int i=0; i < tabs; i++) {
                printf("  ");
            }
            printf("Left:\n");
            printHuffTree(root->left, tabs+1);
            for (int i=0; i < tabs; i++) {
                printf("  ");
            }
            printf("Right:\n");
            printHuffTree(root->right, tabs+1);
            printf("\n");
        }
    }
}



void huffmanEncodeWithTable(FILE* infp, FILE* outfp, HuffTable* table) {

    // Write huffman table
    writeInt32(outfp, table->nSym);
    writeInt32(outfp, table->entrySize);
    for (int i=0; i < table->nSym; i++) {
        writeInt32(outfp, table->codeLens[i]);
    }
    for (int i=0; i < table->nSym * table->entrySize; i++) {
        putc((char) table->codes[i], outfp);
    }


    BitFile outbfp;
    bfFromFilePtr(&outbfp, outfp);
    int c;
    while ((c = getc(infp)) != EOF) {
        int len = table->codeLens[c];    
        u8* code = table->codes + c * table->entrySize;
        for (int i=0; i < len; i++) {
            u8 nextBit = getIthBit(code, i);
            bfWrite((char) nextBit, &outbfp);
        }
    }
    if (outbfp.count != 0) {
        // Bits needed to complete the byte
        int needLen = 8 - outbfp.count;
        // Find code with prefix that would fill byte
        for (int c=0; c < table->nSym; c++) {
            int len = table->codeLens[c];    
            if (len > needLen) {
                u8* code = table->codes + c * table->entrySize;
                int i = 0;
                while (outbfp.count != 0) {
                    bfWrite((char) getIthBit(code, i), &outbfp);
                    i++;
                }
                break;
            }
        }
    }
}


void huffmanDecode(FILE* infp, FILE* outfp, HuffNode *tree) {

    BitFile inbfp; 
    bfFromFilePtr(&inbfp, infp);

    HuffNode* curr = tree;
    int c;
    while ((c = bfRead(&inbfp)) != EOF) {
        if (!c) {
            curr = curr->left;
        } else {
            curr = curr->right;
        }

        if (!curr->left) {
            ASSERT(!curr->right, "Huffman tree malformed in huffmanDecode. All nodes must have 0 or 2 children\n");
            fputc((char) curr->sym, outfp);
            curr = tree;
        }
    }
}
*/

typedef struct HuffNode {
    union {
        int left;
        // Reuse space of left child to hold symbol when node is a leaf
        int sym;
    };
    union {
        int right;
        // Since no node points to the 0 node and all nodes have 2 or 0 children, we
        // can reuse right child space to decide if parent.
        int isParent; 
    };
} HuffNode;


typedef struct HuffTree {
    HuffNode nodes[NUM_HUFF_NODES];
    float weights[NUM_HUFF_NODES];
} HuffTree;


typedef struct HuffTable {
    int codeLens[NUM_HUFF_SYMS];
    // Shouldn't be possible to have a proper tree with more than num syms bits for a
    // particular symbol
    u8 codes[NUM_HUFF_SYMS * NUM_HUFF_SYMS];
} HuffTable;



void buildHuffTree(HuffTree* tree, int *syms, float* symWeights) {
    int nOrphans = NUM_HUFF_SYMS;
    int orphans[NUM_HUFF_SYMS];
    HuffNode* nodes = tree->nodes;
    float* weights = tree->weights;
    // Initialize leaf nodes
    for (int i=0; i < NUM_HUFF_SYMS; i++) {
        orphans[i] = NUM_HUFF_SYMS+i-1;
        nodes[NUM_HUFF_SYMS + i - 1].sym = syms[i];
        nodes[NUM_HUFF_SYMS + i - 1].isParent = 0;
        weights[NUM_HUFF_SYMS + i - 1] = symWeights[i];
    }

    // Build parent nodes
    for (int i=NUM_HUFF_SYMS-2; i >= 0; i--) {
        int small1, small2;
        small1 = orphans[0];
        small2 = orphans[1];
        if (weights[small1] > weights[small2]) {
            small1 = small2;
            small2 = orphans[0];
        }

        // Get lowest 2 nodes in remaining orphans
        for (int j=2; j < nOrphans; j++) {
            int curr = orphans[j];
            if (weights[curr] < weights[small1]) {
                small2 = small1;
                small1 = curr;
            } else if (weights[curr] < weights[small2]) {
                small2 = curr;
            }
        }

        // Make this node point to two lowest
        nodes[i].left = small1;
        nodes[i].right = small2;
        weights[i] = weights[small1] + weights[small2];
        int k=0;
        for (int j=0; j < nOrphans; j++) {
            if (orphans[j] != small1 && orphans[j] != small2) {
                orphans[k++] = orphans[j];
            }
        }
        // Decrement nOrphans and set last orphan to most recent node
        orphans[--nOrphans - 1] = i;
    }
}


void extractHuffCodes(HuffTable* res, HuffTree* tree) {
    int lengths[NUM_HUFF_NODES];
    lengths[0] = 0;
    int maxLen = 0;
    for (int i=0; i < NUM_HUFF_NODES; i++) {
        HuffNode curr = tree->nodes[i];
        if (curr.isParent) {
            lengths[curr.left] = lengths[i] + 1;
            lengths[curr.right] = lengths[i] + 1;
            if (lengths[i] + 1 > maxLen) {
                maxLen = lengths[i] + 1;
            } 
        } else {
            res->codeLens[i-NUM_HUFF_SYMS+1] = lengths[i];
        }
    }
    ASSERT(((maxLen+7)/8) < NUM_HUFF_SYMS, "Error: Code was written expecting maximum code length (in bytes) to be less than the total number of symbols.\n");

    u8 tempCodes[NUM_HUFF_NODES * NUM_HUFF_SYMS];

    for (int i=0; i < NUM_HUFF_NODES; i++) {
        HuffNode curr = tree->nodes[i];
        int byt;
        // Copy this code to it's children
        for (byt=0; byt < (lengths[i]+7) / 8; byt++) {
            tempCodes[curr.left*NUM_HUFF_SYMS + byt] = tempCodes[i*NUM_HUFF_SYMS+byt];
            tempCodes[curr.right*NUM_HUFF_SYMS + byt] = tempCodes[i*NUM_HUFF_SYMS+byt];
        }

        // Current bit to set is 1 after this code's length
        int bitIdx = lengths[i];
        // Left is set off at that bit
        tempCodes[curr.left*NUM_HUFF_SYMS + (bitIdx / 8)] &= ((u8) ~(0x80 >> (bitIdx%8)));
        // Right is set on at that bit
        tempCodes[curr.right*NUM_HUFF_SYMS + (bitIdx / 8)] |= ((u8) (0x80 >> (bitIdx%8)));
    }

    for (int i=0; i < NUM_HUFF_SYMS * NUM_HUFF_SYMS; i++) {
        // Copy over codes for the leaf nodes
        res->codes[i] = tempCodes[i + (NUM_HUFF_SYMS-1) * NUM_HUFF_SYMS];
    }


}


void printHuffTable(HuffTable *table) {
    for (int i=0; i < NUM_HUFF_SYMS; i++) {
        printf("Symbol index: %d\n", i);
        int length = table->codeLens[i];
        printf("  Len: %d\n", length);
        printf("  Code: ");
        for (int j=0; j < length; j++) {
            int bit = table->codes[i*NUM_HUFF_SYMS + (j/8)] & (0x80 >> j % 8);
            printf("%d", bit != 0);
        }
        printf("\n");
        
    }
}


void huffmanEncodeWithTree(FILE* infp, FILE* outfp, HuffTree* tree) {
    HuffTable table;
    extractHuffCodes(&table, tree);

    BitFile outbfp;
    bfFromFilePtr(&outbfp, outfp);

    int c;
    while ((c = getc(infp)) != EOF) {
        int len = table.codeLens[c];    
        u8* code = table.codes + c * NUM_HUFF_SYMS;
        for (int i=0; i < len; i++) {
            u8 nextBit = getIthBit(code, i);
            bfWrite((char) nextBit, &outbfp);
        }
    }
    if (outbfp.count != 0) {
        // Bits needed to complete the byte
        int needLen = 8 - outbfp.count;
        // Find code with prefix that would fill byte
        for (int c=0; c < NUM_HUFF_SYMS; c++) {
            int len = table.codeLens[c];    
            if (len > needLen) {
                u8* code = table.codes + c * NUM_HUFF_SYMS;
                int i = 0;
                while (outbfp.count != 0) {
                    bfWrite((char) getIthBit(code, i), &outbfp);
                    i++;
                }
                break;
            }
        }
    }
    
}


void huffmanDecodeWithTree(FILE* infp, FILE* outfp, HuffTree* tree) {
    BitFile inbfp; 
    bfFromFilePtr(&inbfp, infp);

    HuffNode* curr = tree->nodes;
    int c;
    while ((c = bfRead(&inbfp)) != EOF) {
        if (!c) {
            curr = &tree->nodes[curr->left];
        } else {
            curr = &tree->nodes[curr->right];
        }

        if (!curr->isParent) {
            fputc((char) curr->sym, outfp);
            curr = tree->nodes;
        }
    }
}



void countCharFreqs(FILE* infp, float* weights) {
    int c;
    uint64_t counts[256];
    uint64_t max;
    for (int i=0; i < 256; i++) {
        counts[i] = 0;
    }
    while ((c = fgetc(infp)) != EOF) {
        counts[c] += 1;
    }
    max = counts[0];
    for (int i=1; i < 256; i++) {
        if (counts[i] > max) {
            max = counts[i];
        }
    }
    for (int i=0; i < 256; i++) {
        weights[i] = counts[i] / (float) max;
    }

    // Reset to start
    fseek(infp, 0, SEEK_SET);
}


void moveToFrontTransform(FILE* infp, FILE* outfp) {
    // Position that each character maps to
    int dict[256];
    rangeArr(256, dict);

    int c;
    while ((c = fgetc(infp)) != EOF) {
        int idx = 0;
        int last;
        for (idx=0; dict[idx] != c; idx++) {
            int temp;
            temp = last;
            last = dict[idx];
            dict[idx] = temp;
        }
        dict[idx] = last;
        dict[0] = c;

        fputc((char) idx, outfp);
    }
}


void invMoveToFrontTransform(FILE* infp, FILE* outfp) {
    int dict[256];
    rangeArr(256, dict);

    int idx;
    while ((idx = fgetc(infp)) != EOF) {
        int c = dict[idx];
        fputc((char) c, outfp);

        int last;
        for (int i=0; i < idx; i++) {
            int temp = last;
            last = dict[i];
            dict[i] = temp;
        }
        dict[idx] = last;
        dict[0] = c;
    }
};


void imgQuantTransform(FILE* infp, FILE* outfp) {
    BMPFileHeader h;
    readBMPHeader(infp, &h);
    copyBMPHeader(infp, outfp, &h);

    int fac = IMG_QUANT_FAC;
    
    int c;
    for (int i=0; i < h.height * h.width * (h.bitsPerPixel/8); i++) {
        c = fgetc(infp);
        ASSERT(c != EOF, "Error in imgQuantTransform: Unexpected end of file!\n");
        if ( c > 256-fac || c % fac < fac / 2) {
            c = c / fac;
        } else {
            c = c / fac + 1;
        }
        fputc((char) c, outfp);
    }

    copyRemaining(infp, outfp);
}


void invImgQuantTransform(FILE* infp, FILE* outfp) {
    BMPFileHeader h;
    readBMPHeader(infp, &h);
    copyBMPHeader(infp, outfp, &h);

    int fac = IMG_QUANT_FAC;

    int c;
    for (int i=0; i < h.height * h.width * (h.bitsPerPixel/8); i++) {
        c = fgetc(infp);
        ASSERT(c != EOF, "Error in imgQuantTransform: Unexpected end of file!\n");
        c = c * fac;
        fputc((char) c, outfp);
    }

    copyRemaining(infp, outfp);
}


/*
 *  Split a BMP image into separate RGB channels
 *
 */
void rgbTransform(FILE* infp, FILE* outfp) {
    BMPFileHeader h;
    readBMPHeader(infp, &h);
    ASSERT((h.width * h.bitsPerPixel/8) % 4 == 0, "Error in rgbTransform: Row size not multiple of 4 bytes, handling padding not yet implemented.\n");
    ASSERT(h.bitsPerPixel == 24, "Error in rgbTransform: support for bitsPerPixel other than 24 not implemented.\n");

    char *blue = (char*) malloc(3 * h.width * h.height);
    char *green = blue + h.width * h.height;
    char *red = green + h.width * h.height;

    copyBMPHeader(infp, outfp, &h);

    // Split into 3 color channels
    int rOff = 0;
    int gOff = 0;
    int bOff = 0;
    for (int i=0; i < h.height; i++) {
        for (int j=0; j < h.width; j++) {
            int b = fgetc(infp);
            int g = fgetc(infp);
            int r = fgetc(infp);
            ASSERT((b != EOF) && (g != EOF) && (r != EOF), "Error in rgbTransform: Unexpected end of file in image data.\n");
            *(blue+bOff) = (char) b;
            *(green+gOff) = (char) g;
            *(red+rOff) = (char) r;
            bOff++;
            gOff++;
            rOff++;
        }
    }
    
    // Write the separated color channels sequentially (all red, then green, then blue)
    for (int i=0; i < 3 * h.width * h.height; i++) {
        char c = *(blue+i);
        fputc(c, outfp);
    }

    // If there's anything else copy it over.
    copyRemaining(infp, outfp);

    free(blue);
}


void invRGBTransform(FILE *infp, FILE *outfp) {
    BMPFileHeader h;
    readBMPHeader(infp, &h);
    ASSERT((h.width * h.bitsPerPixel/8) % 4 == 0, "Error in rgbTransform: Row size not multiple of 4 bytes, handling padding not yet implemented.\n");
    ASSERT(h.bitsPerPixel == 24, "Error in rgbTransform: support for bitsPerPixel other than 24 not implemented.\n");

    char *pixels = (char*) malloc(3 * h.width * h.height);

    copyBMPHeader(infp, outfp, &h);

    // Extract color channels into one pixel array with 3 colors per pixel
    for (int color=0; color < 3; color++) {
        for (int i=0; i < h.width * h.height; i++) {
            int c = fgetc(infp);
            ASSERT(c != EOF, "Error in rgbInvTransform: Unexpected end of file in image data\n");
            *(pixels + 3 * i + color) = (char) c;
        }
    }

    // Write combined color array
    for (int i=0; i < 3 * h.width * h.height; i++) {
        fputc(*(pixels+i), outfp);
    }
    free(pixels);

    copyRemaining(infp, outfp);

    
}


/*
 *  Relative encoding
 *  
 *  NOTE: Does not work with stdin
 */
void compRelative(FILE *infp, FILE *outfp) {
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
void decompRelative(FILE *infp, FILE *outfp) {
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
 * enwik9-sm                    1.00148
 * image.bmp                    1.00088
 * image.bmp (w/ rgbTransform)  1.06309
 *
 * Note: Previously used 0 as a padding byte but this lead to pretty bad
 * expansion in image file. However the perfrmance was better on enwik9-sm.
 * This is because there are no 0 bytes in that file. This version handles
 * 0s more gracefully.
 */
void compRLE(FILE *infp, FILE *outfp) {
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


void decompRLE(FILE *infp, FILE *outfp) {
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


void applyTformStack(FILE* infp, FILE* outfp, int nTforms, TformPtr* stack) {
    if (nTforms == 1) {
        (*stack)(infp, outfp);
    } else {

        FILE* tmp1 = tmpfile();
        ASSERT(tmp1, "Error creating temp file 1 in applyTformStack\n");

        FILE* tmp2 = tmpfile();
        ASSERT(tmp2, "Error creating temp file 2 in applyTformStack\n");

        FILE* swapTmp;

        // Apply first transform
        (*stack++)(infp, tmp1);
        rewind(tmp1);
        
        // Apply second to second-to-last transforms
        for (int i=1; i < nTforms-1; i++) {
            (*stack++)(tmp1, tmp2);
            swapTmp = tmp2;
            tmp2 = tmp1;
            tmp1 = swapTmp;
            rewind(tmp1);
            rewind(tmp2);
        }

        // Apply last transform
        (*stack++)(tmp1, outfp);

        fclose(tmp1);
        fclose(tmp2);
        
    }
}


void testCompression(char *baseFile, int nTforms, TformPtr* compress, TformPtr* decompress) {
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

    applyTformStack(infp, outfp, nTforms, compress);

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

    applyTformStack(infp, outfp, nTforms, decompress);

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
        printf("NOTE: Files different at %d\n", i);
        printf("Compression ratio: %.5Lf\n", (long double) baseBytes / (long double) compBytes);
    }
    else {
        printf("Same!\n");
        printf("Compression ratio: %.5Lf\n", (long double) baseBytes / (long double) compBytes);
    }

    fclose(infp);
    fclose(outfp);
}




int main(int argc, char* argv[]) {
    TformPtr compress[1] = {0};
    TformPtr decompress[1] = {0};
    int nTforms = 1;

    FILE *infp; 
    FILE *outfp;
    char *baseFile = "";
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

        applyTformStack(infp, outfp, nTforms, compress);
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

        applyTformStack(infp, outfp, nTforms, decompress);
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
        
        infp = fopen("enwik9-sm", "rb");
        float weights[256];
        int syms[256];
        rangeArr(256, syms);

        countCharFreqs(infp, weights);
        
        HuffTree hTree;
        buildHuffTree(&hTree, syms, weights);
        
        outfp = fopen("enwik9-sm-comp", "wb");

        huffmanEncodeWithTree(infp, outfp, &hTree);

        fclose(outfp);
        fclose(infp);

        infp = fopen("enwik9-sm-comp", "rb");
        outfp = fopen("enwik9-sm-decomp", "wb");

        huffmanDecodeWithTree(infp, outfp, &hTree);

        fclose(outfp);
        fclose(infp);

    }

    
    return (0);
}




