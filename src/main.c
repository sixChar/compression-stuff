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


typedef uint8_t u8;


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


typedef struct HuffNode{
    int sym;
    float weight;
    struct HuffNode *left;
    struct HuffNode *right;
} HuffNode;




/*
 *
 * NOTE: THIS METHOD ASSUMES YOU HAVE AT LEAST n+1 MEMORY ALLOCATED FOR THE SORTED
 *       NODES AND DON'T CARE WHAT HAPPENS TO THE VALUE STORED AT INDEX n.
 *
 */
void insertIntoHuffNodes(HuffNode** sortedNodes, int n, HuffNode* node) {
    if (n == 0) {
        sortedNodes[0] = node;
    }
    for (int i=n-1; i >= 0; i--) {
        if (sortedNodes[i]->weight >= node->weight) {
            sortedNodes[i+1] = node;
            break;
        } else if (i > 0) {
            sortedNodes[i+1] = sortedNodes[i];
        } else {
            sortedNodes[i+1] = sortedNodes[i];
            sortedNodes[0] = node;
        }
    }
}


void sortHuffNodes(HuffNode** nodes, int n) {
    for (int i=1; i < n; i++) {
        insertIntoHuffNodes(nodes, i, nodes[i]);
    }
}


HuffNode* buildHuffman(int* syms, float* weights, int nSym) {
    // Since every non-leaf node has 2 children there will be 2n-1 total nodes
    // with n being the number of leaves (symbols)
    HuffNode* root = (HuffNode*) malloc(sizeof(HuffNode) * 2 * nSym - 1);
    HuffNode** orphans = (HuffNode**) malloc(sizeof(HuffNode*) * nSym);

    for (int i=0; i < nSym; i++) {
        HuffNode* curr = root+nSym-1+i;
        // Symbols are unordered to start
        *(orphans+i) = curr;
        curr->sym = syms[i];
        curr->weight = weights[i];
        curr->left = 0;
        curr->right = 0;
    }

    int nOrphans = nSym;
    sortHuffNodes(orphans, nOrphans);

    for (; nOrphans > 1; nOrphans--) {
        HuffNode* curr = (root + nOrphans-2);
        curr->left = orphans[nOrphans-2];
        curr->right = orphans[nOrphans-1];
        curr->weight = curr->left->weight + curr->right->weight;
        insertIntoHuffNodes(orphans, nOrphans-2, curr);
    }

    free(orphans);
    return root;
}


typedef struct HuffTable {
    int nSym;
    int entrySize;
    u8* codes;
    int* codeLens;
} HuffTable;


int findMaxHuffLength(HuffNode* tree) {
    if (tree == NULL) {
        return 0;
    }

    int leftL = findMaxHuffLength(tree->left);
    int rightL = findMaxHuffLength(tree->right);
    if (leftL > rightL) {
        return leftL+1;
    } else {
        return rightL+1;
    }
}



void recursiveGenHuffCodes(HuffTable* res, HuffNode* tree, u8* currCode, int depth) {
    if (tree->left == NULL || tree->right == NULL) {
        ASSERT(tree->left == tree->right, "Error: malformed tree. All nodes must be full or leaves.\n");
        int sym = tree->sym;
        for (int i=0; i <= depth/8; i++) {
            res->codes[sym+i] = currCode[i];
        }
        res->codeLens[sym] = depth;
    } else {
        // Set current bit to 1
        currCode[depth/8] |= 0x80 >> (depth % 8);
        recursiveGenHuffCodes(res, tree->right, currCode, depth+1);
        // Set current bit to 0
        currCode[depth/8] &= ~(0x80 >> (depth % 8));
        recursiveGenHuffCodes(res, tree->left, currCode, depth+1);
    }
}

void extractHuffCodes(HuffTable* res, HuffNode* tree) {
    int maxLen = findMaxHuffLength(tree);
    // Allocate enough space for every symbol to fit the longest code
    res->entrySize = (maxLen + 7) / 8 * sizeof(u8);
    res->codes = (u8*) malloc(res->entrySize * res->nSym + sizeof(int) * res->nSym);
    res->codeLens = (int*) (res->codes + res->entrySize*res->nSym);

    // holder for current code
    u8* currCode = (u8*) malloc(res->entrySize);
    for (int i=0; i < res->entrySize; i++) {
        currCode[i] = 0;
    }

    recursiveGenHuffCodes(res, tree, currCode, 0);

    free(currCode);

    
}

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

void printHuffmanTree(HuffNode* root, int tabs) {
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
            printHuffmanTree(root->left, tabs+1);
            for (int i=0; i < tabs; i++) {
                printf("  ");
            }
            printf("Right:\n");
            printHuffmanTree(root->right, tabs+1);
            printf("\n");
        }
    }
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
        //if (c > s) {
        //    c = c - 1;
        //}
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
    TformPtr compress[1] = {imgQuantTransform};
    TformPtr decompress[1] = {invImgQuantTransform};
    int nTforms = 1;

    FILE *infp; 
    FILE *outfp;
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
        //printf("Testing file %s:\n", baseFile);
        //testCompression(baseFile, nTforms, compress, decompress);
        

        int n = 5;
        float weights[5] = {15.0, 7.0, 6.0, 6.0, 5.0};
        int syms[5] = {0, 1, 2, 3, 4};
        
        HuffNode* tree = buildHuffman(syms, weights, n);
        printHuffmanTree(tree, 0);
        HuffTable table;
        table.nSym = n; 
        extractHuffCodes(&table, tree);
        printHuffTable(&table);
    }

    
    return (0);
}




