#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_PATH_LEN 10000

enum type {
    VARIANT,
    LIST,
    PARSE,
    EXTRACT,
    FINDALL
} Type;

typedef struct arguments {
    int type;
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

void freeHeader(pHeader header);

int main(int argc, char **argv) {
    if (argc >= 2) {
        pArguments arguments = parseArgumentsList(argc, argv);
        if (arguments == NULL) {
            printf("ERROR\nInvalid arguments\n");
            return -1;
        }

        switch (arguments->type) {
            case VARIANT: {
                printf("82417\n");
                break;
            }
            case LIST: {
                printf("SUCCESS\n");
                listFiles(arguments);
                break;
            }
            case PARSE: {
                pHeader header = parseHeader(arguments->path, 1);
                if (header != NULL) {
                    printHeader(header);
                }
                freeHeader(header);
                break;
            }
            case EXTRACT: {
                pHeader header = parseHeader(arguments->path, 0);
                if (header != NULL) {
                    extractLine(header, arguments->path, arguments->sect_nr, arguments->line_nr);

                }
                freeHeader(header);
                break;
            }
            case FINDALL: {
                printf("SUCCESS\n");
                findAll(arguments);
            }
            default:
                break;
        }

        free(arguments->options);
        free(arguments->path);
        free(arguments);
    } else {
        printf("Usage: %s [OPTIONS] [PARAMETERS] (arguments in any order)\n", argv[0]);
    }
    return 0;
}

void freeHeader(pHeader header) {
    if (header == NULL) return;
    for (int nrSection = 0; nrSection < header->no_of_sections; nrSection++) {
        if (header->pSections != NULL) {
            free(header->pSections[nrSection]);
        }
    }
    free(header->pSections);
    free(header);
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
    if (argc < 2) {
        return NULL;
    }

    pArguments data = (pArguments) malloc(sizeof(Arguments));
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "path=", 5)) {
            data->path = (char *) malloc(MAX_PATH_LEN);
            snprintf(data->path, MAX_PATH_LEN, "%s", argv[i] + 5);
        } else if (!strcmp(argv[i], "recursive")) {
            data->recursive = 1;
        } else if (!strncmp(argv[i], "section=", 8)) {
            data->sect_nr = atoi(argv[i] + 8);
        } else if (!strncmp(argv[i], "line=", 5)) {
            data->line_nr = atoi(argv[i] + 5);
        } else if (strchr(argv[i], '=')) { // Filter option
            if (data->no_of_options == 0) {
                data->options = (char **) malloc(sizeof(char *));
            }
            data->options[data->no_of_options++] = argv[i];
        } else { // type
            if (!strcmp(argv[i], "variant")) {
                data->type = VARIANT;
            } else if (!strcmp(argv[i], "list")) {
                data->type = LIST;
            } else if (!strcmp(argv[i], "parse")) {
                data->type = PARSE;
            } else if (!strcmp(argv[i], "extract")) {
                data->type = EXTRACT;
            } else if (!strcmp(argv[i], "findall")) {
                data->type = FINDALL;
            }
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
                if (inode.st_size >= atoi(value) || dirEntry->d_type != DT_REG) {
                    good = 0;
                }
            }
            if (!strcmp(option, "name_starts_with")) {
                if (strncmp(name, value, strlen(value)) != 0) {
                    good = 0;
                }
            }
            if (!strcmp(option, "sections_smaller")) {
                if (dirEntry->d_type != DT_REG) {
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
                    freeHeader(header);
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
        freeHeader(header);
        return NULL;
    }
    if (strcmp(header->magic, "79ws") != 0) {
        if (showError) {
            printf("ERROR\nwrong magic\n");
        }
        freeHeader(header);
        return NULL;
    }

    if (read(fd, &header->size, sizeof(header->size)) < 0) {
        if (showError) {
            printf("ERROR\nUnable to read\n");
        }
        freeHeader(header);
        return NULL;
    }

    if (read(fd, &header->version, sizeof(header->version)) < 0) {
        if (showError) {
            printf("ERROR\nUnable to read\n");
        }
        freeHeader(header);
        return NULL;
    }
    if (!(header->version >= 83 && header->version <= 141)) {
        if (showError) {
            printf("ERROR\nwrong version\n");
        }
        freeHeader(header);
        return NULL;
    }

    if (read(fd, &header->no_of_sections, sizeof(header->no_of_sections)) < 0) {
        if (showError) {
            printf("ERROR\nUnable to read\n");
        }
        freeHeader(header);
        return NULL;
    }
    if (!(header->no_of_sections >= 5 && header->no_of_sections <= 10)) {
        if (showError) {
            printf("ERROR\nwrong sect_nr\n");
        }
        freeHeader(header);
        return NULL;
    }

    header->pSections = (pSection *) malloc(sizeof(Section) * header->no_of_sections + 1);
    memset(header->pSections, '\0', sizeof(Section) * header->no_of_sections + 1);
    for (int i = 0; i < header->no_of_sections; i++) {
        pSection section = (pSection) malloc(sizeof(Section));
        memset(section, '\0', sizeof(Section));
        if (read(fd, section->name, sizeof(section->name) - 1) < 0) {
            if (showError) {
                printf("ERROR\nUnable to read\n");
            }
            free(section);
            freeHeader(header);
            return NULL;
        }
        strcat(section->name, "\0");

        if (read(fd, &section->type, sizeof(section->type)) < 0) {
            if (showError) {
                printf("ERROR\nUnable to read\n");
            }
            free(section);
            freeHeader(header);
            return NULL;
        }
        if (!(section->type == 17 || section->type == 18)) {
            if (showError) {
                printf("ERROR\nwrong sect_types\n");
            }
            free(section);
            freeHeader(header);
            return NULL;
        }

        if (read(fd, &section->offset, sizeof(section->offset)) < 0) {
            if (showError) {
                printf("ERROR\nUnable to read\n");
            }
            free(section);
            freeHeader(header);
            return NULL;
        }

        if (read(fd, &section->size, sizeof(section->size)) < 0) {
            if (showError) {
                printf("ERROR\nUnable to read\n");
            }
            free(section);
            freeHeader(header);
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
    int fd;
    if (access(path, R_OK) == -1) {
        chmod(path, S_IRUSR | S_IRGRP | S_IROTH);
    }
    if ((fd = open(path, O_RDONLY, 0644)) < 0) {
        printf("ERROR\ninvalid file\n");
        exit(1);
    }

    if (header->no_of_sections < section_nr) {
        printf("ERROR\ninvalid section");
        exit(1);
    }
    int offset = header->pSections[section_nr - 1]->offset;
    int size = header->pSections[section_nr - 1]->size;
    int currentLine = 1;
    char buf = '\0';
    char bufPrev = '\r';

    lseek(fd, offset + --size, SEEK_SET);
    read(fd, &buf, 1);

    do {
        lseek(fd, offset + --size, SEEK_SET);
        if (line_nr == currentLine) {
            if (bufPrev == '\r') {
                printf("SUCCESS\n");
            }
            printf("%c", buf);
        }

        if (bufPrev == '\n' && buf == '\r') {
            if (line_nr == currentLine++) {
                return 1;
            }
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