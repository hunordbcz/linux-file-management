#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_PATH_LEN 1000
#define BUF_SIZE 1000

typedef struct arguments {
    char *path;
    char **options;
    int no_of_options;
    int recursive;
} Arguments, *pArguments;

typedef struct section {
    char name[16];
    char type[2];
    int offset;
    short size;
} Section, *pSection;

typedef struct header {
    char magic[4];
    short size;
    short version;
    char no_of_sections;
    pSection *pSections;
} Header, *pHeader;

pArguments parseArgumentsList(int argc, char **argv);

int listFiles(pArguments arguments);

int parseHeader(pArguments arguments);

int main(int argc, char **argv) {
    if (argc >= 2) {
        if (!strcmp(argv[1], "variant")) {
            printf("82417\n");
        } else if (!strcmp(argv[1], "list")) {
            pArguments arguments = parseArgumentsList(argc, argv);
            if (arguments == NULL) {
                printf("ERROR\nInvalid arguments\n");
                exit(1);
            } else {
                printf("SUCCESS\n");
            }
            listFiles(arguments);
        } else if (!strcmp(argv[1], "parse")) {
            pArguments arguments = parseArgumentsList(argc, argv);
            if (arguments == NULL) {
                printf("ERROR\nInvalid arguments\n");
                exit(1);
            }
            parseHeader(arguments);
        } else if (!strcmp(argv[1], "extract")) {

        } else if (!strcmp(argv[1], "findall")) {

        } else if (!strcmp(argv[1], "parse")) {

        }
    }
    return 0;
}

pArguments parseArgumentsList(int argc, char **argv) {
    if (argc < 3) {
        return NULL;
    }

    pArguments data = (pArguments) malloc(sizeof(Arguments));
    for (int i = 2; i < argc; i++) {
        if (!strncmp(argv[i], "path=", 5)) {
            data->path = (char *) malloc(MAX_PATH_LEN);
            snprintf(data->path, MAX_PATH_LEN, "%s", argv[i] + 5);
        } else if (!strcmp(argv[i], "recursive")) {
            data->recursive = 1;
        } else { // Filter option
            if (data->no_of_options == 0) {
                data->options = (char **) malloc(sizeof(char *));
            }
            data->options[data->no_of_options++] = argv[i];
        }
    }

    return data;
}

int listFiles(pArguments arguments) {
    if (arguments->path[strlen(arguments->path) - 1] != '/') {
        strcat(arguments->path, "/");
    }

    DIR *dir;
    struct dirent *dirEntry;
    struct stat inode;
    char path[MAX_PATH_LEN];
    char folder[MAX_PATH_LEN];
    strcpy(folder, arguments->path);

    dir = opendir(folder);
    if (dir == NULL) {
        printf("ERROR\ninvalid directory path\n");
        exit(1);
    }

    while ((dirEntry = readdir(dir)) != 0) {
        char *name = dirEntry->d_name;
        if (!strcmp(name, ".") || !strcmp(name, "..")) {
            continue;
        }

        snprintf(path, MAX_PATH_LEN, "%s%s", folder, name);
        lstat(path, &inode);

        if (S_ISDIR(inode.st_mode) && arguments->recursive) {
            strcpy(arguments->path, path);
            strcat(arguments->path, "/");
            listFiles(arguments);
        }
        if (S_ISREG(inode.st_mode) || !arguments->no_of_options) {
            int good = 1;
            for (int i = 0; i < arguments->no_of_options; i++) {
                char *value = (char *) malloc(sizeof(char));
                strcpy(value, arguments->options[i]);
                char *option = strtok_r(value, "=", &value);
                if (!strcmp(option, "size_smaller")) {
                    if (inode.st_size >= atoi(value)) {
                        good = 0;
                    }
                }
                if (!strcmp(option, "name_starts_with")) {
                    if (strncmp(name, value, strlen(value)) != 0) {
                        good = 0;
                    }
                }
            }
            if (good) {
                printf("%s\n", path);
            }
        }
    }
    closedir(dir);
    return 1;
}

int get_line(int fd, char *line, int line_no, int max_length, int *line_length) {
    char temp;
    while (read(fd, &temp, 1) != -1) {
        printf("%c", temp);
        if (temp == '\n') line_no--;
        if (!line_no) {
            int i;
            for (i = 0; (i < max_length && temp != '\n'); i++) {
                if (read(fd, &temp, 1) == -1) {
                    break;
                }
                line[i] = temp;
            }

            *line_length = i;
            return 0;
        }
    }

    return -1;
}


int parseHeader(pArguments arguments) {
    int fd;
    char *buff = (char *) malloc(BUF_SIZE);
    memset(buff, '\0', BUF_SIZE);

    if (access(arguments->path, R_OK) == -1) {
        chmod(arguments->path, S_IRUSR | S_IRGRP | S_IROTH);
    }

    if ((fd = open(arguments->path, O_RDONLY, 0644)) < 0) {
        printf("ERROR\nUnable to open file\n");
        exit(1);
    }

    int line = 0;

    pHeader header = (pHeader) malloc(sizeof(Header));

    if (read(fd, header->magic, sizeof(header->magic)) < 0) {
        printf("ERROR\nUnable to read\n");
        exit(1);
    }
    printf("%s ", header->magic);

    char tmp;
    read(fd, tmp, 1);
    if (read(fd, buff, 25) < 0) {
        printf("ERROR\nUnable to read\n");
        exit(1);
    }
    header->size = atoi(buff);

    memset(buff, 0, BUF_SIZE);
    if (read(fd, buff, sizeof(header->version)) < 0) {
        printf("ERROR\nUnable to read\n");
        exit(1);
    }
    printf("%s,%d", buff, atoi(buff));
    header->version = atoi(buff);

    memset(buff, 0, BUF_SIZE);
    if (read(fd, buff, sizeof(header->no_of_sections)) < 0) {
        printf("ERROR\nUnable to read\n");
        exit(1);
    }
    header->no_of_sections = atoi(buff);

//    char temp;
//    while (read(fd, &temp, 1) != -1)
//    {
//        printf("%c", temp);
//        if (temp == '\n') line_no --;
//        if(!line_no){
//            int i;
//            for (i = 0; (i < max_length && temp != '\n'); i++)
//            {
//                if (read(fd, &temp, 1) == -1)
//                {
//                    break;
//                }
//                line[i] = temp;
//            }
//
//            *line_length = i;
//            return 0;
//        }
//    }
    return 0;
}