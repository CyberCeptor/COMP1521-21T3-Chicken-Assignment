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
#include <ctype.h>

#include "chicken.h"


// ADD ANY extra #defines HERE


// ADD YOUR FUNCTION PROTOTYPES HERE
uint64_t calculate_content_length(FILE *fp);
void print_pathname(FILE *fp, uint16_t pathname_length);
void print_permissions(FILE *fp);
uint8_t hash_value_calculator(FILE *fp, int byte_count_no_hash);


// print the files & directories stored in egg_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes are also printed (subset 0)

void list_egg(char *egg_pathname, int long_listing) {

    int c = 0, next_egglet = 0;
    uint8_t format = 0;
    uint16_t pathname_length = 0;
    uint64_t content_length = 0;

    FILE *fp = fopen(egg_pathname, "r");
    if (fp == NULL) {
        perror(egg_pathname);
        exit(1);
    }


    if (long_listing == 0) {



        while ((c = fgetc(fp)) != EOF) { //check c != EOF.
            fseek(fp, -1, SEEK_CUR); //reset the fp after checking EOF. move back 1 byte
            fseek(fp, EGG_OFFSET_PATHNLEN, SEEK_CUR);

            pathname_length = (fgetc(fp) | (fgetc(fp) << 8)); //little endian 2 byte int.
            fseek(fp, pathname_length, SEEK_CUR); //move *fp past the length of the pathname.
            content_length = calculate_content_length(fp);      // unsigned 48 bit int to a 64bit int. (little-endian).
            fseek(fp, -(EGG_LENGTH_CONTLEN + (pathname_length)), SEEK_CUR); //moving fp back to the pathname. Minus the content_length + the pathname_length.
            print_pathname(fp, pathname_length);        //print out the filename.
            printf("\n");
            next_egglet = EGG_LENGTH_CONTLEN + content_length + EGG_LENGTH_HASH; //the number of bytes to the next egglet's start'.
            fseek(fp, next_egglet, SEEK_CUR); //moves the fp to the next egglet pathname_length.
        }
    } else if (long_listing) {
        //fp is at the start of the file.

        while ((c = fgetc(fp)) != EOF) { //check not EOF
            fseek(fp, -1, SEEK_CUR); // move back 1 byte that was checked.
            fseek(fp, 2, SEEK_CUR); //set fp to the permissions.
            print_permissions(fp);

            fseek(fp, -(EGG_LENGTH_FORMAT + EGG_LENGTH_MODE), SEEK_CUR); //move to the format. Move - 11
            format = fgetc(fp);
            printf("%3c", format);

            //print the content_length, i.e. number of bytes of the content.
            //Need to calculate the pathname-length first.
            fseek(fp, EGG_LENGTH_MODE, SEEK_CUR); //move to pathname_length.
            pathname_length = (fgetc(fp) | (fgetc(fp) << 8));

            fseek(fp, pathname_length, SEEK_CUR); //move to content_length.
            content_length = calculate_content_length(fp);
            printf("%7lu  ", content_length);

            fseek(fp, -(EGG_LENGTH_CONTLEN + (pathname_length)), SEEK_CUR); //move to pathname.
            print_pathname(fp, pathname_length); 
            printf("\n");

            next_egglet = EGG_LENGTH_CONTLEN + content_length + EGG_LENGTH_HASH; //move to the start of the next egglet.
            fseek(fp, next_egglet, SEEK_CUR);
        }
    }
    fclose(fp);
}





// check the files & directories stored in egg_pathname (subset 1)
//
// prints the files & directories stored in egg_pathname with a message
// either, indicating the hash byte is correct, or
// indicating the hash byte is incorrect, what the incorrect value is and the correct value would be

void check_egg(char *egg_pathname) {

    int c;
    FILE *fp = fopen(egg_pathname, "r");
    if (fp == NULL) {
        fprintf(stderr, "%s", egg_pathname);
        exit(1);
    }

    while ((c = fgetc(fp)) != EOF) {
        //check the magic number(first byte)  of each egglet.
        if (c != EGGLET_MAGIC) { //0x63
            fprintf(stderr, "error: incorrect first egglet byte: 0x%x should be 0x%x\n", c, EGGLET_MAGIC);
            exit(1);
        }


        //set fp to the pathname_length, minus the checked magic number.
        fseek(fp, (EGG_OFFSET_PATHNLEN - 1), SEEK_CUR);
        uint16_t pathname_length = (fgetc(fp) | (fgetc(fp) << 8));

        //set fp to content_length.
        fseek(fp, pathname_length, SEEK_CUR);
        uint64_t content_length = calculate_content_length(fp);

        //set fp to hash.
        fseek(fp, content_length, SEEK_CUR);
        uint8_t given_hash_value = fgetc(fp);
        
        //set to the start of the egglet to read all bytes - hash, for the hash calculation.
        fseek(fp, -(EGG_LENGTH_HASH + content_length + EGG_LENGTH_CONTLEN + pathname_length + 14), SEEK_CUR);
        
        int byte_count_no_hash = 14 + pathname_length + 6 + content_length; //doesnt count the hash byte.
        uint8_t calculated_hash_value = hash_value_calculator(fp, byte_count_no_hash);

        //set fp to the pathname.
        fseek(fp, -(content_length + EGG_LENGTH_CONTLEN + pathname_length), SEEK_CUR);
        print_pathname(fp, pathname_length);

        if (given_hash_value == calculated_hash_value) {
            printf(" - correct hash\n");
        } else {
            printf(" - incorrect hash 0x%x should be 0x%x\n", calculated_hash_value, given_hash_value);
        }

        fseek(fp, EGG_LENGTH_CONTLEN + content_length + 1, SEEK_CUR);
    }
}


uint8_t hash_value_calculator(FILE *fp, int byte_count_no_hash) {
    uint8_t calculated_hash_value = 0;
    for (int i = 0, c = 0; i < byte_count_no_hash; i++) {
        if ((c = fgetc(fp)) == EOF) {
            break;
        }
        calculated_hash_value = egglet_hash(calculated_hash_value, c);
    }
    return calculated_hash_value;
}



// extract the files/directories stored in egg_pathname (subset 2 & 3)

void extract_egg(char *egg_pathname) {
    int i = 0;
    // opens the egg file, reads through the content and writes it to a new file. 
    FILE *fp = fopen(egg_pathname, "rb");
    if (fp == NULL) {
        fprintf(stderr, "error: file does not exist.\n");
        exit(1);
    }

    int c;
    while ((c = fgetc(fp)) != EOF) {
        fseek(fp, -1, SEEK_CUR);
        //set fp to permissions.
        fseek(fp, 2, SEEK_CUR);
        int permissions[100];
        char linux_perm[3] = "";
       
        c = fgetc(fp); //remove the file permissions at the start of permissions.
        i = 1; //set to 1 as it ignores the file/directory permissions.
        int j = 0;
        while (i < 10) {
            permissions[i] = fgetc(fp);
            if (permissions[i] == 'r') {
                linux_perm[j] += 4;
            } else if (permissions[i] == 'w') {
                linux_perm[j] += 2;
            } else if (permissions[i] == 'x') {
                linux_perm[j] += 1;
            } else if (permissions[i] != '-') { //if the permission given isnt r|w|x|-, then its an error, and should exit so.
                fprintf(stderr, "error: incorrect permission %c\n", permissions[i]);
                exit(1);
            }
            //increment every third loop. for the 3 permissions of group/user/owner.
            if (i % 3 == 0) {
                j++;
            }
            i++;
        }

        //fp is now at the pathname length.
        uint16_t pathname_length = (fgetc(fp) | (fgetc(fp) << 8));
        
        //get the filename(pathname) to be created.
        char filename[100] = "";

        //fp is at pathname.
        i = 0;
        while (i < pathname_length) {
            fread(&filename[i], 1, 1, fp);
            i++;
        }   

        printf("Extracting: %s\n", filename);

        FILE *new_file = fopen(filename, "w");
        if (new_file == NULL) {
            fprintf(stderr, "error: file was not created.\n");
            exit(1);
        }

        long result = 0;
        for (int tmp = 0; tmp < 3; tmp++) {
            result *= 8; //multiply by 8 to set to base 8, rather than base 10.
            result += linux_perm[tmp];
        }

        mode_t mode = result;

        if (chmod(filename, mode) != 0) {
            perror(filename);
            exit(1);
        }

        // at the content length pointer.
        uint64_t content_length = calculate_content_length(fp);
        //fp is at 'content', loops through to add the content to the new file.
        i = 0;
        while (((c = fgetc(fp)) != EOF) && (i < content_length)) {
            fputc(c, new_file);
            i++;
        }

        fclose(new_file);
    }

    fclose(fp);
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



void print_permissions(FILE *fp) {
    char temp[10] = "0";
    for (int i = 0; i < 10; i++) {
        temp[i] = fgetc(fp);
        printf("%c", temp[i]);
    }
}



void print_pathname(FILE *fp, uint16_t pathname_length) {
    int i = 0, c = 0;

    for (i = 0; i < pathname_length; i++) {
        c = fgetc(fp);
        fputc(c, stdout);
        if (c == '\n') {
            break;
        }
    }

    return;
}



uint64_t calculate_content_length(FILE *fp) {
    uint64_t content_length = 0;
    for (int i = 0; i < 6; i++) {
        content_length |= (uint64_t)(fgetc(fp)) << (i * 8);
    }

    return content_length;
}

