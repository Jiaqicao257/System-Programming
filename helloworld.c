// student netid: jiaqic7
//A program to encrypt a file using xor protocal and decrypt the encryted file 
//when error occurs print "Hello World"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>


int main(int argc, char **argv) {
    if(argc != 4) {
        //printf("Hello World1");
        printf("Hello World");
        exit(1);
    }
    if (!strcmp(argv[0], "./decrypt")) { 
        int fd1 = open(argv[2], O_RDWR);
        int fd2 = open(argv[3], O_RDWR);
        struct stat stat1, stat2;
        if(fstat(fd1, &stat1) !=0) {
            //printf("Hello World3");
            printf("Hello World");
            exit(1);
        } else if (fstat(fd2, &stat2) !=0) {
            //printf("Hello World4");
            printf("Hello World");
            exit(1);
        }
        if (stat1.st_size != stat2.st_size) {
            printf("Hello World");
            exit(1);
        }
        char* result1 = mmap(NULL, stat1.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);
        char* result2 = mmap(NULL, stat2.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
        if (result1 == (void *)-1 || result2 == (void*)-1) {
            //printf("Hello World5");
            printf("Hello World");
            exit(1);
        } 
        char* output_file = argv[1];
        FILE* output = fopen(output_file, "w");
        if (!output) {
            //printf("Hello World6");
            printf("Hello World");
            exit(1);
        }
        size_t size = stat1.st_size;
        for (size_t j = 0; j < size; j++) {
            fputc(result1[j] ^ result2[j], output);
        }
        close(fd1);
        close(fd2);
        fclose(output);
        munmap(result1, stat1.st_size);
        munmap(result2, stat2.st_size);
    } else if (!strcmp(argv[0], "./encrypt")) {
        char *output = argv[1];
        int file_d = open(output, O_RDWR);
        if (file_d < 0) {
            //printf("Hello World7");
            printf("Hello World");
            exit(0);
        }
        struct stat stat1, stat2;
        stat(argv[2], &stat1);
        stat(argv[3], &stat2);
        struct stat status;
        if(fstat(file_d, &status) !=0) {
            //printf("Hello World8");
            printf("Hello World");
            exit(1);
        }
        size_t size = status.st_size;
        char* address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, file_d, 0);
        if (address == (void *)-1) {
            //printf("Hello World9");
            printf("Hello World");
            exit(1);
        }
        FILE* fd1 = fopen(argv[2], "w");
        FILE* fd2 = fopen(argv[3], "w");
        if (!fd1 || !fd2) {
            printf("Hello World");
            exit(1);
        } 
        unsigned char buffer[1];
        int fd_num = open("/dev/urandom", O_RDONLY);
        for (size_t i = 0; i < size; i++) {
            read(fd_num, buffer, 1);
            fputc(buffer[0], fd1);
            fputc(address[i] ^ buffer[0], fd2); 
            address[i] = 0;
        }
        close(file_d);
        fclose(fd1);
        fclose(fd2);
        munmap(address, size);   
    } else {
        //neither encrypt nor decrypt
        //printf("Hello World2");
        printf("Hello World");
        exit(1);
    }
    return 0;
}
