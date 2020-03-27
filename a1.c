#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_PATH_LEN 1000
#define BUF_SIZE 110000

typedef struct arguments {
    char *path;
    char **options;
    int sect_nr;
    int line_nr;
    int no_of_options;
    int recursive;
} Arguments, *pArguments;

typedef struct section {
    char name[17];
    short type;
    int offset;
    int size;
} Section, *pSection;

typedef struct header {
    char magic[4];
    short size;
    short version;
    u_int8_t no_of_sections;
    pSection *pSections;
} Header, *pHeader;

pArguments parseArgumentsList(int argc, char **argv);

int listFiles(pArguments arguments);

pHeader parseHeader(char *path, int showError);

void printHeader(pHeader header);

int extractLine(pHeader header, char *path, int section_nr, int line_nr);

int findAll(pArguments arguments);

int main(int argc, char **argv) {
    if (argc >= 2) {
        pArguments arguments = parseArgumentsList(argc, argv);
        if (!strcmp(argv[1], "variant")) {
            printf("82417\n");
        } else if (!strcmp(argv[1], "list")) {

            if (arguments == NULL) {
                printf("ERROR\nInvalid arguments\n");
                exit(1);
            } else {
                printf("SUCCESS\n");
            }
            listFiles(arguments);
        } else if (!strcmp(argv[1], "parse")) {
            if (arguments == NULL) {
                printf("ERROR\nInvalid arguments\n");
                exit(1);
            }
            pHeader header = parseHeader(arguments->path, 1);
            if (header != NULL) {
                printHeader(header);
            }
        } else if (!strcmp(argv[1], "extract")) {
            if (arguments == NULL) {
                printf("ERROR\nInvalid arguments\n");
                exit(1);
            }
            pHeader header = parseHeader(arguments->path, 0);
            extractLine(header, arguments->path, arguments->sect_nr, arguments->line_nr);
//            if (line != NULL) {
//                printf("SUCCESS\n%s\n", line);
//            }
        } else if (!strcmp(argv[1], "findall")) {
            printf("SUCCESS\n");
            findAll(arguments);
        }
        free(arguments);
    }
    return 0;
}

int findAll(pArguments arguments) {
    if (arguments->no_of_options == 0) {
        arguments->options = (char **) malloc(sizeof(char *));
    }
    arguments->options[arguments->no_of_options++] = "sections_smaller=910";
    arguments->recursive = 1;
    listFiles(arguments);
    return 1;
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
        } else if (!strncmp(argv[i], "section=", 8)) {
            data->sect_nr = atoi(argv[i] + 8);
        } else if (!strncmp(argv[i], "line=", 5)) {
            data->line_nr = atoi(argv[i] + 5);
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
    DIR *dir;
    struct dirent *dirEntry;
    struct stat inode;
    char path[MAX_PATH_LEN];
    char folder[MAX_PATH_LEN];


    dir = opendir(arguments->path);
    if (dir == NULL) {
        printf("ERROR\ninvalid directory path\n");
        exit(1);
    }

    if (arguments->path[strlen(arguments->path) - 1] != '/') {
        strcat(arguments->path, "/");
    }
    strcpy(folder, arguments->path);

    while ((dirEntry = readdir(dir)) != 0) {
        char *name = dirEntry->d_name;
        if (!strcmp(name, ".") || !strcmp(name, "..")) {
            continue;
        }

        sprintf(path, "%s%s", folder, name);
        lstat(path, &inode);

        if (dirEntry->d_type == DT_DIR && arguments->recursive) {
            sprintf(arguments->path, "%s/", path);
            listFiles(arguments);
        }
        int good = 1;
        for (int i = 0; i < arguments->no_of_options && good; i++) {
            char valueOriginal[strlen(arguments->options[i]) + 1];
            strcpy(valueOriginal, arguments->options[i]);
            char *option = strtok(valueOriginal, "=");
            char *value = strtok(NULL, "=");
            if (!strcmp(option, "size_smaller")) {
                if (inode.st_size >= atoi(value) || !S_ISREG(inode.st_mode)) {
                    good = 0;
                }
            }
            if (!strcmp(option, "name_starts_with")) {
                if (strncmp(name, value, strlen(value)) != 0) {
                    good = 0;
                }
            }
            if (!strcmp(option, "sections_smaller")) {
                if (!S_ISREG(inode.st_mode)) {
                    good = 0;
                } else {
                    pHeader header = parseHeader(path, 0);
                    if (header == NULL) {
                        good = 0;
                    }
                    for (int nrSection = 0; header != NULL && nrSection < header->no_of_sections && good; nrSection++) {
                        if (header->pSections[nrSection]->size > 910) {
                            good = 0;
                        }
                    }
                }
            }
        }
        if (good) {
            printf("%s\n", path);
        }
    }
    free(dirEntry);
    closedir(dir);
    return 1;
}

pHeader parseHeader(char *path, int showError) {
    int fd;
    if (access(path, R_OK) == -1) {
        chmod(path, S_IRUSR | S_IRGRP | S_IROTH);
    }
    if ((fd = open(path, O_RDONLY, 0644)) < 0) {
        printf("ERROR\nUnable to open file\n");
        exit(1);
    }

    pHeader header = (pHeader) malloc(sizeof(Header));
    memset(header, '\0', sizeof(Header));

    if (read(fd, header->magic, sizeof(header->magic)) < 0) {
        printf("ERROR\nUnable to read\n");
        return NULL;
    }
    if (strcmp(header->magic, "79ws") != 0) {
        if (showError) {
            printf("ERROR\nwrong magic\n");
        }
        return NULL;
    }

    if (read(fd, &header->size, sizeof(header->size)) < 0) {
        if (showError) {
            printf("ERROR\nUnable to read\n");
        }

        return NULL;
    }

    if (read(fd, &header->version, sizeof(header->version)) < 0) {
        if (showError) {
            printf("ERROR\nUnable to read\n");
        }
        return NULL;
    }
    if (!(header->version >= 83 && header->version <= 141)) {
        if (showError) {
            printf("ERROR\nwrong version\n");
        }
        return NULL;
    }

    if (read(fd, &header->no_of_sections, sizeof(header->no_of_sections)) < 0) {
        if (showError) {
            printf("ERROR\nUnable to read\n");
        }
        return NULL;
    }
    if (!(header->no_of_sections >= 5 && header->no_of_sections <= 10)) {
        if (showError) {
            printf("ERROR\nwrong sect_nr\n");
        }
        return NULL;
    }

    header->pSections = (pSection *) malloc(sizeof(Section) * header->no_of_sections);
    for (int i = 0; i < header->no_of_sections; i++) {
        pSection section = (pSection) malloc(sizeof(Section));

        if (read(fd, section->name, sizeof(section->name) - 1) < 0) {
            if (showError) {
                printf("ERROR\nUnable to read\n");
            }
            return NULL;
        }
        strcat(section->name, "\0");

        if (read(fd, &section->type, sizeof(section->type)) < 0) {
            if (showError) {
                printf("ERROR\nUnable to read\n");
            }
            return NULL;
        }
        if (!(section->type == 17 || section->type == 18)) {
            if (showError) {
                printf("ERROR\nwrong sect_types\n");
            }
            return NULL;
        }

        if (read(fd, &section->offset, sizeof(section->offset)) < 0) {
            if (showError) {
                printf("ERROR\nUnable to read\n");
            }
            return NULL;
        }

        if (read(fd, &section->size, sizeof(section->size)) < 0) {
            if (showError) {
                printf("ERROR\nUnable to read\n");
            }
            return NULL;
        }

        header->pSections[i] = section;
    }
    close(fd);

    return header;
}

void printHeader(pHeader header) {
    printf("SUCCESS\n");
    printf("version=%d\n", header->version);
    printf("nr_sections=%d\n", header->no_of_sections);
    for (int i = 0; i < header->no_of_sections; i++) {
        printf("section%d: ", i + 1);
        printf("%s ", header->pSections[i]->name);
        printf("%d ", header->pSections[i]->type);
        printf("%d\n", header->pSections[i]->size);
    }
}

int extractLine(pHeader header, char *path, int section_nr, int line_nr) {
    ///Check if File can be opened
    int fd;
    if (access(path, R_OK) == -1) {
        chmod(path, S_IRUSR | S_IRGRP | S_IROTH);
    }
    if ((fd = open(path, O_RDONLY, 0644)) < 0) {
        printf("ERROR\ninvalid file\n");
        exit(1);
    }

    ///Check if section exists
    if (header->no_of_sections < section_nr) {
        printf("ERROR\ninvalid section");
        return 0;
    }
    int offset = header->pSections[section_nr - 1]->offset;
    int size = header->pSections[section_nr - 1]->size;

//    char *line = (char *) malloc(BUF_SIZE);
    int currentLine = 1;

    char buf = '\0';
    char bufPrev = '\r';
    lseek(fd, offset + --size, SEEK_SET);
    read(fd, &buf, 1);
    do {
        lseek(fd, offset + --size, SEEK_SET);

//        if (!(buf[0] == '\n' || buf[0] == '\r')) {
//            strcat(line, buf);
//        }
        if (line_nr == currentLine) {
            if (bufPrev == '\r') {
                printf("SUCCESS\n");
            }
            printf("%c", buf);
        }

        if (bufPrev == '\n' && buf == '\r') {
//            strcat(line, "\0");
            if (line_nr == currentLine++) {
                return 1;
            }
//            memset(line, '\0', BUF_SIZE);
//            continue;
        }
        bufPrev = buf;
        buf = '\0';
    } while (read(fd, &buf, 1) != -1 && size > -1);
    if (line_nr == currentLine) {
        if (buf != '\0') {
            printf("%c", buf);
        }
        return 1;
    }

    printf("ERROR\ninvalid line");
    return 0;
}