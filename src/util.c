
#include "util.h"


int diff_file(FILE *fp1, FILE *fp2) {
    int c1, c2;
    int i = 1;
    c1 = fgetc(fp1);
    c2 = fgetc(fp2);
    while (c1 != EOF && c2 != EOF) {
        if (c1 != c2)
            return i;
        c1 = fgetc(fp1);
        c2 = fgetc(fp2);
        i+=1;
    }

    if (c1 != c2) {
        return i;
    }
    return 0;
}








