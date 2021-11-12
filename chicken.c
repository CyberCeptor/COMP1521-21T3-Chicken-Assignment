////////////////////////////////////////////////////////////////////////
// COMP1521 21T3 --- Assignment 2: `chicken', a simple file archiver
// <https://www.cse.unsw.edu.au/~cs1521/21T3/assignments/ass2/index.html>
//
// Written by YOUR-NAME-HERE (z5555555) on INSERT-DATE-HERE.
//
// 2021-11-08   v1.1    Team COMP1521 <cs1521 at cse.unsw.edu.au>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "chicken.h"


// ADD ANY extra #defines HERE


// ADD YOUR FUNCTION PROTOTYPES HERE


// print the files & directories stored in egg_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes are also printed (subset 0)

void list_egg(char *egg_pathname, int long_listing) {

    // REPLACE THIS CODE WITH YOUR CODE
    FILE *fp = fopen(egg_pathname, "r");
    if (fp == NULL) {
        perror(egg_pathname);
        exit(1);
    }


   fseek(fp, EGG_OFFSET_PATHNLEN, SEEK_SET);




    
    uint16_t pathname_length = 0;
    pathname_length = fgetc(fp) | (fgetc(fp) << 8);
    printf("pathname length = %d\n", pathname_length);


    fseek(fp, pathname_length, SEEK_CUR);
    uint64_t content_length = 0;
    content_length |= (uint64_t)(fgetc(fp)) << 0;
    content_length |= (uint64_t)(fgetc(fp)) << 8;
    content_length |= (uint64_t)(fgetc(fp)) << 16;
    content_length |= (uint64_t)(fgetc(fp)) << 24;
    content_length |= (uint64_t)(fgetc(fp)) << 32;
    content_length |= (uint64_t)(fgetc(fp)) << 40;

    printf("content length = %lu\n", content_length);


    fseek(fp, -(EGG_LENGTH_CONTLEN + (pathname_length)), SEEK_CUR);
    int c;
    int i = 0;
    while ((c = fgetc(fp)) && i < pathname_length) {
        fputc(c, stdout);
        if (c == '\n') {
            break;
        }
        i++;
    }

    printf("\n");

    int next_pathname_length = 6 + content_length + 12;

    fseek(fp, next_pathname_length, SEEK_CUR);
    pathname_length = fgetc(fp) | (fgetc(fp) << 8);
    i = 0;
    while ((c = fgetc(fp)) && i < pathname_length) {
        fputc(c, stdout);
        if (c == '\n') {
            break;
        }
        i++;
    }
    printf("\n");
   // printf("list_egg called to list egg: '%s'\n", egg_pathname);

    if (long_listing) {
        printf("long listing with permissions & sizes specified\n");
    }
}


// check the files & directories stored in egg_pathname (subset 1)
//
// prints the files & directories stored in egg_pathname with a message
// either, indicating the hash byte is correct, or
// indicating the hash byte is incorrect, what the incorrect value is and the correct value would be

void check_egg(char *egg_pathname) {

    // REPLACE THIS PRINTF WITH YOUR CODE

    printf("check_egg called to check egg: '%s'\n", egg_pathname);
}


// extract the files/directories stored in egg_pathname (subset 2 & 3)

void extract_egg(char *egg_pathname) {

    // REPLACE THIS PRINTF WITH YOUR CODE

    printf("extract_egg called to extract egg: '%s'\n", egg_pathname);
}


// create egg_pathname containing the files or directories specified in pathnames (subset 3)
//
// if append is zero egg_pathname should be over-written if it exists
// if append is non-zero egglets should be instead appended to egg_pathname if it exists
//
// format specifies the egglet format to use, it must be one EGGLET_FMT_6,EGGLET_FMT_7 or EGGLET_FMT_8

void create_egg(char *egg_pathname, int append, int format,
                int n_pathnames, char *pathnames[n_pathnames]) {

    // REPLACE THIS CODE PRINTFS WITH YOUR CODE

    printf("create_egg called to create egg: '%s'\n", egg_pathname);
    printf("format = %x\n", format);
    printf("append = %d\n", append);
    printf("These %d pathnames specified:\n", n_pathnames);
    for (int p = 0; p < n_pathnames; p++) {
        printf("%s\n", pathnames[p]);
    }
}


// ADD YOUR EXTRA FUNCTIONS HERE
