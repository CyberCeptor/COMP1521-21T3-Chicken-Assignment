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


#define MAX_LINUX_FILENAME_LENGTH 255
#define BYTE 8

void chmod_calculation(FILE *fp, char linux_perm[3]);
void check_hash_value(uint8_t given_hash_value, uint8_t calculated_hash_value);
void print_pathname(FILE *fp, uint16_t pathname_length);
void print_permissions(FILE *fp);
void write_content_length(FILE *fp, uint64_t content_length);
void write_content(FILE *fp, FILE *open_file);
void write_permissions(FILE *fp, struct stat file_stat);
void write_to_file(FILE *fp, FILE *new_file, uint64_t content_length);
long base_eight(char linux_perm[3]);
uint8_t hash_value_calculator(FILE *fp, int byte_count_no_hash);
uint16_t get_pathname_length(FILE *fp);
uint64_t calculate_content_length(FILE *fp);
uint64_t get_content_length(FILE *open_file);

// print the files & directories stored in egg_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes are also printed (subset 0)
void list_egg(char *egg_pathname, int long_listing) {
    int c = 0;
    uint8_t format = 0;
    uint16_t pathname_length = 0;
    uint64_t content_length = 0;
    FILE *fp = fopen(egg_pathname, "r");
    if (fp == NULL) {
        perror(egg_pathname);
        exit(1);
    }
    switch(long_listing) {
        case 0: 
            while ((c = fgetc(fp)) != EOF) { //checks the Magic Number byte != NULL, moves to egglet format.
                fseek(fp, (EGG_OFFSET_PATHNLEN - 1), SEEK_CUR); //moves to pathname_length, minus 1 for the magic number fgetc.
                pathname_length = get_pathname_length(fp);
                char pathname[MAX_LINUX_FILENAME_LENGTH] = ""; //stores the pathname. Need to be reset after each loop as it prints from the start.
                int fread_pathname_length = fread(&pathname, 1, pathname_length, fp);
                if (fread_pathname_length != pathname_length) {
                    fprintf(stderr, "error getting the pathname from %s\n", egg_pathname); //error getting the pathname.
                    exit(1);
                }

                content_length = calculate_content_length(fp);      // unsigned 48 bit int to a 64bit int. (little-endian).
                printf("%s\n", pathname);
                fseek(fp, (content_length + EGG_LENGTH_HASH), SEEK_CUR);
            }

        default: //long_listing != 0, also print the file/directory permissions, format & sizes.
            while ((c = fgetc(fp)) != EOF) { //check not EOF for the Magic number, moves to egglet format byte.
                format = fgetc(fp); //stores the egglet format 
                print_permissions(fp);
                printf("%3c", format);
                pathname_length = get_pathname_length(fp);
                char pathname[MAX_LINUX_FILENAME_LENGTH] = ""; //stores the pathname. Need to be reset after each loop as it prints from the start.
                int fread_pathname_length = fread(&pathname, 1, pathname_length, fp);
                if (fread_pathname_length != pathname_length) {
                    fprintf(stderr, "error getting the pathname from %s\n", egg_pathname); //error getting the pathname.
                    exit(1);
                }

                content_length = calculate_content_length(fp);
                printf("%7lu  ", content_length);
                printf("%s\n", pathname);
                fseek(fp, (content_length + EGG_LENGTH_HASH), SEEK_CUR);
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
    int c = 0;
    int byte_count_no_hash = 0;
    int egglet_length = 0;
    uint8_t calculated_hash_value = 0;
    uint8_t given_hash_value = 0;
    uint16_t pathname_length = 0;
    uint64_t content_length = 0;

    FILE *fp = fopen(egg_pathname, "r");
    if (fp == NULL) {
        fprintf(stderr, "%s\n", egg_pathname);
        exit(1);
    }

    while ((c = fgetc(fp)) != EOF) { //check the magic number(first byte)  of each egglet.
        if (c != EGGLET_MAGIC) { //Validate the magic number == 0x63.
            fprintf(stderr, "error: incorrect first egglet byte: 0x%x should be 0x%x\n", c, EGGLET_MAGIC);
            exit(1);
        }

        fseek(fp, (EGG_OFFSET_PATHNLEN - 1), SEEK_CUR); //set fp to the pathname_length, minus the checked magic number.
        pathname_length = get_pathname_length(fp);
        char pathname[MAX_LINUX_FILENAME_LENGTH] = ""; //stores the pathname. Need to be reset after each loop as it prints from the start.
        int fread_pathname_length = fread(&pathname, 1, pathname_length, fp); //return value = count of chars of fread 
        if (fread_pathname_length != pathname_length) {
            fprintf(stderr, "error getting the pathname from %s\n", egg_pathname); //error getting the pathname.
            exit(1);
        }

        content_length = calculate_content_length(fp);
        fseek(fp, content_length, SEEK_CUR); //set fp to hash.
        given_hash_value = fgetc(fp); //store the hash_value of the egglet.
        egglet_length = EGG_LENGTH_HASH + content_length + EGG_LENGTH_CONTLEN + pathname_length + EGG_OFFSET_PATHNLEN + EGG_LENGTH_MAGIC + EGG_LENGTH_FORMAT;
        fseek(fp, -(egglet_length), SEEK_CUR); //sets fp back to the beginning of the egglet.
        byte_count_no_hash = (egglet_length - EGG_LENGTH_HASH); //counts the number of bytes in the egglet, minus the hash value.
        calculated_hash_value = hash_value_calculator(fp, byte_count_no_hash);
        printf("%s", pathname);
        check_hash_value(given_hash_value, calculated_hash_value);
        fseek(fp, EGG_LENGTH_HASH, SEEK_CUR); //set fp to the next egglet Magic Number. Moves by 1.
    }
}


// extract the files/directories stored in egg_pathname (subset 2 & 3)

void extract_egg(char *egg_pathname) {
    int c;
    int fread_pathname_length = 0;
    uint16_t pathname_length = 0;
    uint64_t content_length = 0;
    FILE *fp = fopen(egg_pathname, "rb"); // opens the egg for reading and writing.
    if (fp == NULL) {
        fprintf(stderr, "error: file does not exist.\n");
        exit(1);
    }

    while ((c = fgetc(fp)) != EOF) { //always check the start of a new egglet isnt EOF. checks the Magic Number.
        char linux_perm[3] = ""; //stores the values of the permissions. reset every loop.
        char pathname[MAX_LINUX_FILENAME_LENGTH] = ""; //stores the pathname. reset every loop.
        fseek(fp, EGG_LENGTH_FORMAT, SEEK_CUR);  //set fp to permissions. Moves past 1 byte.
        chmod_calculation(fp, linux_perm);
        pathname_length = get_pathname_length(fp);
        fread_pathname_length = fread(&pathname, 1, pathname_length, fp); //stores the pathname into pathname[].
        if (fread_pathname_length != pathname_length) {
            fprintf(stderr, "error getting the pathname from %s\n", egg_pathname); //error getting the pathname.
            exit(1);
        }
        printf("Extracting: %s\n", pathname);
        FILE *new_file = fopen(pathname, "w"); //creates a new blank text file.
        long result = base_eight(linux_perm);
        mode_t mode = result; //creates a 'mode' for the result, required for the chmod function.
        if (chmod(pathname, mode) != 0) { //returns zero when successful.
            fprintf(stderr, "chmod error of file: %s\n", pathname);
            exit(1);
        }

        content_length = calculate_content_length(fp);
        write_to_file(fp, new_file, content_length); //writes the contents to the new file.
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
    uint16_t pathname_length = 0;
    uint64_t content_length = 0;
    char *write_or_append = "w+"; //set to create a new file to be written and read.
    if (append != 0) {
        write_or_append = "a+"; //sets to a+ to append an existing file. //Sets the fp pointer to the end of the egglet.
    }

    FILE *fp = fopen(egg_pathname, write_or_append);
    if (fp == NULL) {
        fprintf(stderr, "%s\n", egg_pathname);
        exit(1);
    }

    for (int counter = 0; counter < n_pathnames; counter++) {
        struct stat file_stat;
        if (stat(pathnames[counter], &file_stat) != 0) { //stat() to gather the information about the file.
            fprintf(stderr, "%s\n", pathnames[counter]);
            exit(1);
        }

        FILE *open_file = fopen(pathnames[counter], "r"); //open the file for reading.
        if (open_file == NULL) {
            fprintf(stderr, "%s\n", pathnames[counter]);
            exit(1);
        }

        fputc(EGGLET_MAGIC, fp); //Store the egg magic number in the egglet. 0x63.
        fputc(format, fp);  //store format in egglet.
        write_permissions(fp, file_stat);
        for(pathname_length = 0; pathnames[counter][pathname_length] != '\0'; pathname_length++); //get the length of the pathname.
        fputc((pathname_length & 0xFF), fp);  //writing to egg in little-endian from big-endian. First byte
        fputc((pathname_length >> BYTE), fp); //second byte.
        fwrite(pathnames[counter], 1, pathname_length, fp); //writes the pathname to the egglet.
        printf("Adding: %s\n", pathnames[counter]);
        content_length = get_content_length(open_file);
        write_content_length(fp, content_length);
        fseek(open_file, 0, SEEK_SET); //reset open_file back to the start so the contents can be copied over to fp.
        write_content(fp, open_file);
        byte_count = EGG_LENGTH_MAGIC + EGG_LENGTH_FORMAT + EGG_LENGTH_MODE + EGG_LENGTH_PATHNLEN +
            pathname_length + EGG_LENGTH_CONTLEN + content_length; //doesnt include the hash value.
        fseek(fp, -(byte_count), SEEK_CUR); //set the fp back to the start to calculate the hash value.
        fputc(hash_value_calculator(fp, byte_count), fp); //stores the calculation from the hash_calculator in the egglet.
        fclose(open_file);
    }

    fclose(fp);
}

uint64_t get_content_length(FILE *open_file) {
    int c;
    uint64_t content_length = 0;
    while ((c = fgetc(open_file)) != EOF) { //calculate the content length of the file.
        content_length++;
    }

    return content_length;
}


//writes the content from open_file to fp.
void write_content(FILE *fp, FILE *open_file) {
    int c;
    while ((c = fgetc(open_file)) != EOF) { //writing the content to the egg(content section) byte by byte.
        fputc(c, fp);
    }
}

/*Takes the big-endian uint64_t and stores it in the fp(egglet) as a 
unsigned 48 bit little endian integer.*/
void write_content_length(FILE *fp, uint64_t content_length) {
    for (int i = 0; i < 6; i++) {  //convert big-endian to little endian unsigned 48 bit int.
        fputc((content_length >> (i * BYTE)), fp); //starts with the far right byte, and moves across 6 times to the last.
    }
}

//prints the permissions of the egglet. 10 bytes.
void print_permissions(FILE *fp) {
    char temp[10] = "0";
    for (int i = 0; i < 10; i++) {
        temp[i] = fgetc(fp);
        printf("%c", temp[i]);
    }
}

/*Calculates the content length of the egglet. Converts the 6 bytes to a 
little-endian unsigned 48-bit integer. Uses a uint64_t to store to the int.*/
uint64_t calculate_content_length(FILE *fp) {
    uint64_t content_length = 0;
    for (int i = 0; i < 6; i++) {
        content_length |= (uint64_t)(fgetc(fp)) << (i * BYTE);
    }

    return content_length;
}

/*Calculates the hash value of the egglet, loops according to the byte count given
of that egglet.*/
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

/*Given the character permissions of a file, converts to the Octal value required for chmod.
Adds the values to the argument string.*/
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

/*Given a string which contains the values of the permissions of the egglet. 
Multiplied by 8, to convert to Octal. Returns the value*/
long base_eight(char linux_perm[3]) {
    long result = 0;
    for (int i = 0; i < 3; i++) {
        result *= 8; //*8 as convert to Octal. Base 8, not 10.
        result += linux_perm[i];
    }

    return result;
}

//Loops through the content bytes and fputc's to the new txt file.
void write_to_file(FILE *fp, FILE *new_file, uint64_t content_length) {
    int c;
    int i = 0;
    while (((c = fgetc(fp)) != EOF) && (i < content_length)) {
        fputc(c, new_file);
        i++;
    }
}

//Calculates the pathname_length by converting big endian to little endian 2 byte int.
uint16_t get_pathname_length(FILE *fp) {
    return (fgetc(fp) | (fgetc(fp) << BYTE));
}

// Print the outcome of the caluclated hash value.
void check_hash_value(uint8_t given_hash_value, uint8_t calculated_hash_value) {
    if (given_hash_value == calculated_hash_value) {
        printf(" - correct hash\n");
    } else {
        printf(" - incorrect hash 0x%x should be 0x%x\n", calculated_hash_value, given_hash_value);
    }

    return;
}

/* Function which &'s the st_mode of the stat given and fputc's the 
corresponding linux permission characters.*/
void write_permissions(FILE *fp, struct stat file_stat) {
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
