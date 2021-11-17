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
void chmod_calculation(FILE *fp, char linux_perm[3]);
long base_eight(char linux_perm[3]);
void write_to_file(FILE *fp, FILE *new_file, uint64_t content_length);
uint16_t get_pathname_length(FILE *fp);
void get_permissions(FILE *fp, struct stat file_stat);


// print the files & directories stored in egg_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes are also printed (subset 0)


void list_egg(char *egg_pathname, int long_listing) {

    int c = 0, next_egglet_distance = 0;
    uint8_t format = 0;
    uint16_t pathname_length = 0;
    uint64_t content_length = 0;

    //fp will always open at the very start of the egglet. Pointing at the first byte (Magic Number).
    FILE *fp = fopen(egg_pathname, "r");
    if (fp == NULL) {
        perror(egg_pathname);
        exit(1);
    }

    if (long_listing == 0) {
        while ((c = fgetc(fp)) != EOF) { //check c != EOF.
            fseek(fp, -1, SEEK_CUR); //reset the fp after checking EOF. move back 1 byte
            fseek(fp, EGG_OFFSET_PATHNLEN, SEEK_CUR);

            pathname_length = get_pathname_length(fp);
            fseek(fp, pathname_length, SEEK_CUR); //move *fp past the length of the pathname.
            content_length = calculate_content_length(fp);      // unsigned 48 bit int to a 64bit int. (little-endian).
            fseek(fp, -(EGG_LENGTH_CONTLEN + (pathname_length)), SEEK_CUR); //moving fp back to the pathname. Minus the content_length + the pathname_length.
            print_pathname(fp, pathname_length);        //print out the pathname.
            printf("\n");
            next_egglet_distance = EGG_LENGTH_CONTLEN + content_length + EGG_LENGTH_HASH; //the number of bytes to the next egglet's start'.
            fseek(fp, next_egglet_distance, SEEK_CUR); //moves the fp to the next egglet pathname_length.
        }
    } else if (long_listing) {
        while ((c = fgetc(fp)) != EOF) { //check not EOF
            fseek(fp, -1, SEEK_CUR); // move back 1 byte that was checked.
            fseek(fp, 2, SEEK_CUR); //set fp to the permissions.
            print_permissions(fp);

            fseek(fp, -(EGG_LENGTH_FORMAT + EGG_LENGTH_MODE), SEEK_CUR); //move to the format. Move - 11
            format = fgetc(fp);
            printf("%3c", format);

            //Need to calculate the pathname-length first.
            fseek(fp, EGG_LENGTH_MODE, SEEK_CUR); //move to pathname_length.
            pathname_length = get_pathname_length(fp);

            fseek(fp, pathname_length, SEEK_CUR); //move to content_length.
            content_length = calculate_content_length(fp);
            printf("%7lu  ", content_length);

            fseek(fp, -(EGG_LENGTH_CONTLEN + (pathname_length)), SEEK_CUR); //move to pathname.

            print_pathname(fp, pathname_length); 
            printf("\n");

            next_egglet_distance = EGG_LENGTH_CONTLEN + content_length + EGG_LENGTH_HASH; //move to the start of the next egglet.
            fseek(fp, next_egglet_distance, SEEK_CUR);
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
    int c = 0, byte_count_no_hash = 0;
    uint8_t given_hash_value = 0, calculated_hash_value = 0;
    uint16_t pathname_length = 0;
    uint64_t content_length = 0;

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
        pathname_length = get_pathname_length(fp);
        //set fp to content_length.
        fseek(fp, pathname_length, SEEK_CUR);
        content_length = calculate_content_length(fp);
        //set fp to hash.
        fseek(fp, content_length, SEEK_CUR);
        given_hash_value = fgetc(fp);
        //set to the start of the egglet to read all bytes - hash, for the hash calculation.
        fseek(fp, -(EGG_LENGTH_HASH + content_length + EGG_LENGTH_CONTLEN + pathname_length + 14), SEEK_CUR);
        byte_count_no_hash = 14 + pathname_length + 6 + content_length; //doesnt count the hash byte.
        calculated_hash_value = hash_value_calculator(fp, byte_count_no_hash);
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




// extract the files/directories stored in egg_pathname (subset 2 & 3)

void extract_egg(char *egg_pathname) {
    int c;
    uint16_t pathname_length = 0;
    uint64_t content_length = 0;
    // opens the egg file, reads through the content and writes it to a new file. 
    FILE *fp = fopen(egg_pathname, "rb");
    if (fp == NULL) {
        fprintf(stderr, "error: file does not exist.\n");
        exit(1);
    }

    //always check the start of a new egglet isnt EOF.
    while ((c = fgetc(fp)) != EOF) { //checks the magic byte
        fseek(fp, EGG_LENGTH_FORMAT, SEEK_CUR);  //set fp to permissions.

        char linux_perm[3] = "";
        chmod_calculation(fp, linux_perm);

        pathname_length = get_pathname_length(fp);
        //get the pathname to be created and store it in pathname[];
        char pathname[100] = "";
        fread(&pathname, 1, pathname_length, fp); //freads 1 byte of the pathname_length to pathname[].
        printf("Extracting: %s\n", pathname);

        //creating the new txt file.
        FILE *new_file = fopen(pathname, "w");
        if (new_file == NULL) {
            fprintf(stderr, "error: file was not created.\n");
            exit(1);
        }

        long result = base_eight(linux_perm);
        mode_t mode = result;
        if (chmod(pathname, mode) != 0) {
            perror(pathname);
            exit(1);
        }

        content_length = calculate_content_length(fp);
        write_to_file(fp, new_file, content_length);
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

    int byte_count = 0;
    int c;
    uint8_t hash_result = 0;

    char *write_or_append = "w+";
    if (append != 0) {
        write_or_append = "a+";
    }

    FILE *fp = fopen(egg_pathname, write_or_append);
    if (fp == NULL) {
        fprintf(stderr, "%s", egg_pathname);
    }

    
    for (int counter = 0; counter < n_pathnames; counter++) {
        struct stat file_stat;
        if (stat(pathnames[counter], &file_stat) != 0) {
            fprintf(stderr, "%s", pathnames[counter]);
            exit(1);
        }

        fputc(0x63, fp);
        fputc(format, fp);    

        get_permissions(fp, file_stat);

        uint16_t pathname_length = 0;
    
        for(pathname_length = 0; pathnames[counter][pathname_length] != '\0'; pathname_length++); //get the length of the pathname.

        //writing to egg in little-endian format.
        fputc((pathname_length & 0xFF), fp);
        fputc((pathname_length >> 8), fp);

        //copy over the pathname byte by byte.
        printf("Adding: ");
        for (int i = 0; i < pathname_length ; i++) {
            fputc(pathnames[counter][i], fp);
            printf("%c", pathnames[counter][i]);
        }
        printf("\n");

        FILE *open_file = fopen(pathnames[counter], "r");

        uint64_t content_length = 0;
        while ((c = fgetc(open_file)) != EOF) {
            content_length++;
        }

        //convert big-endian to little endian unsigned 48 bit int.
        uint64_t x = content_length;
        for (int i = 0; i < 6; i++) {
            fputc((x >> (i * 8)), fp);
        }

        //reset open_file back to the start to the contents can be copied over.
        fseek(open_file, 0, SEEK_SET);
        //writing the content to the egg(content section) byte by byte.
        while ((c = fgetc(open_file)) != EOF) {
            fputc(c, fp);
        }

        //calculate the hash value from the above bytes. Little-endian 8-bit value.
        byte_count = 14 + pathname_length + 6 + content_length;
        //set the fp back to the start to calculate the hash value.
        fseek(fp, -byte_count, SEEK_CUR);
        hash_result = hash_value_calculator(fp, byte_count);
        fputc(hash_result, fp);
        fclose(open_file);
    }
    fclose(fp);
    //end of file to egglet, move to next file is there is one.
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

//
uint64_t calculate_content_length(FILE *fp) {
    uint64_t content_length = 0;
    for (int i = 0; i < 6; i++) {
        content_length |= (uint64_t)(fgetc(fp)) << (i * 8);
    }

    return content_length;
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

void chmod_calculation(FILE *fp, char linux_perm[3]) {
    int c = fgetc(fp); //remove the first permission at the start as its for the file/directory.
    for (int i = 1, j = 0; i < 10; i++) {
        c = fgetc(fp);
        if (c == 'r') {
            linux_perm[j] += 4;
        } else if (c == 'w') {
            linux_perm[j] += 2;
        } else if (c == 'x') {
            linux_perm[j] += 1;
        } else if (c != '-') { //if the permission given isnt r|w|x|-, then its an error, and should exit.
            fprintf(stderr, "error: invalid permission %c\n", c);
            exit(1);
        }
        //every third permission is the end of that group/user/owner, so move to next array element.
        if (i % 3 == 0) {
            j++;
        }
    }
    return;
}

long base_eight(char linux_perm[3]) {
    long result = 0;
    for (int i = 0; i < 3; i++) {
        result *= 8;
        result += linux_perm[i];
    }

    return result;
}

//Loops through the content bytes and fputc's to the new txt file.
void write_to_file(FILE *fp, FILE *new_file, uint64_t content_length) {
    int i = 0;
    int c;
    while (((c = fgetc(fp)) != EOF) && (i < content_length)) {
        fputc(c, new_file);
        i++;
    }
}

//little endian 2 byte int.
uint16_t get_pathname_length(FILE *fp) {
    return (fgetc(fp) | (fgetc(fp) << 8));
}



void get_permissions(FILE *fp, struct stat file_stat) {

    long long perm;
    perm = file_stat.st_mode;

    if (perm & __S_IFDIR) {
            fputc('d', fp);
        } else {
            fputc('-', fp);
        }

        if (perm & S_IRUSR) {
            fputc('r', fp);
        } else {
            fputc('-', fp);
        }

        if (perm & S_IWUSR) {
            fputc('w', fp);
        } else {
            fputc('-', fp);
        }

        if (perm & S_IXUSR) {
            fputc('x', fp);
        } else {
            fputc('-', fp);
        }

        if (perm & S_IRGRP) {
            fputc('r', fp);
        } else {
            fputc('-', fp);
        }

        if (perm & S_IWGRP) {
            fputc('w', fp);
        } else {
            fputc('-', fp);
        }

        if (perm & S_IXGRP) {
            fputc('x', fp);
        } else {
            fputc('-', fp);
        }

        if (perm & S_IROTH) {
            fputc('r', fp);
        } else {
            fputc('-', fp);
        }

        if (perm & S_IWOTH) {
            fputc('w', fp);
        } else {
            fputc('-', fp);
        }

        if (perm & S_IXOTH) {
            fputc('x', fp);
        } else {
            fputc('-', fp);
        }
}
