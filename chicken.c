////////////////////////////////////////////////////////////////////////
// COMP1521 21T3 --- Assignment 2: `chicken', a simple file archiver
// <https://www.cse.unsw.edu.au/~cs1521/21T3/assignments/ass2/index.html>
//
// Written by Jenson Craig Morgan (z5360181) on 12/11/2021
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
uint64_t calculate_content_length(FILE *fp);
void print_filename(FILE *fp, uint16_t pathname_length);
void print_permissions(FILE *fp);

// print the files & directories stored in egg_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes are also printed (subset 0)

void list_egg(char *egg_pathname, int long_listing) {

    FILE *fp = fopen(egg_pathname, "r");
    if (fp == NULL) {
        perror(egg_pathname);
        exit(1);
    }

    int c = 0;
    int next_pathname_length = 0;
    uint16_t pathname_length = 0;
    uint64_t content_length = 0;
    uint8_t format = 0;

    if (long_listing == 0) {
        fseek(fp, EGG_OFFSET_PATHNLEN, SEEK_SET);
        while ((c = fgetc(fp)) != EOF) { //check c != EOF.
            fseek(fp, -1, SEEK_CUR); //reset the fp after checking EOF. move back 1 byte
            pathname_length = (fgetc(fp) | (fgetc(fp) << 8)); //little endian 2 byte int.
            fseek(fp, pathname_length, SEEK_CUR); //move *fp past the length of the pathname.
            content_length = calculate_content_length(fp);      // unsigned 48 bit int to a 64bit int. (little-endian).
            fseek(fp, -(EGG_LENGTH_CONTLEN + (pathname_length)), SEEK_CUR); //moving fp back to the pathname. Minus the content_length + the pathname_length.
            print_filename(fp, pathname_length);        //print out the filename.
            printf("\n");
            next_pathname_length = 6 + content_length + 12; //the number of bytes to the next egglet's pathname_length.
            fseek(fp, next_pathname_length, SEEK_CUR); //moves the fp to the next egglet pathname_length.
        }
    } else if (long_listing) {
        //fp is at the start of the file.
        fseek(fp, 2, SEEK_CUR); //set fp to the permissions.
        while ((c = fgetc(fp)) != EOF) { //check not EOF
            fseek(fp, -1, SEEK_CUR); // move back 1 byte that was checked.
            print_permissions(fp);

            fseek(fp, -(EGG_LENGTH_FORMAT + EGG_LENGTH_MODE), SEEK_CUR); //move to the format.
            format = fgetc(fp);
            printf("%3c", format);

            //print the content_length, i.e. number of bytes of the content.
            //Need to calculate the pathname-length first.
            fseek(fp, EGG_LENGTH_MODE, SEEK_CUR); //move to pathname_length.
            pathname_length = (fgetc(fp) | (fgetc(fp) << 8));

            fseek(fp, pathname_length, SEEK_CUR); //move to content_length.
            content_length = calculate_content_length(fp);
            printf("%7lu  ", content_length);

            fseek(fp, -(EGG_LENGTH_CONTLEN + (pathname_length)), SEEK_CUR); //move to filename.
            print_filename(fp, pathname_length); 
            printf("\n");

            next_pathname_length = EGG_LENGTH_CONTLEN + content_length + EGG_LENGTH_MAGIC + EGG_OFFSET_FORMAT; //move to the start of the next egglet.
            fseek(fp, next_pathname_length, SEEK_CUR);
        }
    }
    fclose(fp);
}


void print_permissions(FILE *fp) {
    char temp[10] = "0";
    for (int i = 0; i < 10; i++) {
        temp[i] = fgetc(fp);
        printf("%c", temp[i]);
    }
}

void print_filename(FILE *fp, uint16_t pathname_length) {
    int i = 0;
    int c = 0;
    while ((c = fgetc(fp)) && i < pathname_length) {
        fputc(c, stdout);
        if (c == '\n') {
            break;
        }
        i++;
    } 
    return;
}

uint64_t calculate_content_length(FILE *fp) {
    uint64_t content_length = 0;
    content_length |= (uint64_t)(fgetc(fp)) << 0;
    content_length |= (uint64_t)(fgetc(fp)) << 8;
    content_length |= (uint64_t)(fgetc(fp)) << 16;
    content_length |= (uint64_t)(fgetc(fp)) << 24;
    content_length |= (uint64_t)(fgetc(fp)) << 32;
    content_length |= (uint64_t)(fgetc(fp)) << 40;

    return content_length;
}





// check the files & directories stored in egg_pathname (subset 1)
//
// prints the files & directories stored in egg_pathname with a message
// either, indicating the hash byte is correct, or
// indicating the hash byte is incorrect, what the incorrect value is and the correct value would be

void check_egg(char *egg_pathname) {


    int c;

    // REPLACE THIS PRINTF WITH YOUR CODE
    FILE *fp = fopen(egg_pathname, "r");
    if (fp == NULL) {
        fprintf(stderr, "%s", egg_pathname);
        exit(1);
    }

while ((c = fgetc(fp)) != EOF) {




    //check the magic number(first byte)  of each egglet.
    if (c != 0x63) {
        fprintf(stderr, "error: incorrect first egglet byte: 0x%x should be 0x63\n", c);
        exit(1);
    }

    fseek(fp, -1, SEEK_CUR);

    fseek(fp, EGG_OFFSET_PATHNLEN, SEEK_CUR);
    uint16_t pathname_length = (fgetc(fp) | (fgetc(fp) << 8));
    fseek(fp, pathname_length, SEEK_CUR);
    uint64_t content_length = calculate_content_length(fp);

    fseek(fp, content_length, SEEK_CUR);
    uint8_t given_hash_value = fgetc(fp);
    
    fseek(fp, -(6 + pathname_length + 14 + content_length + 1), SEEK_CUR); //set to the start of the egglet.
    
    int egglet_size = 14 + pathname_length + 6 + content_length; //doesnt count the hash byte.

    uint8_t calculated_hash_value = 0; //reset the hash value for each egglet.
    int i = 0;
    while ((c = fgetc(fp)) != EOF && i < egglet_size) {
        calculated_hash_value = egglet_hash(calculated_hash_value, c);
        i++;
    }

    fseek(fp, -(content_length + 6 + pathname_length + 1), SEEK_CUR); //set fp to the filename.
    print_filename(fp, pathname_length);

    if (given_hash_value == calculated_hash_value) {
        printf(" - correct hash\n");
    } else {
        printf(" - incorrect hash 0x%x should be 0x%x\n", calculated_hash_value, given_hash_value);
    }

    fseek(fp, 6 + content_length, SEEK_CUR);

}
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
