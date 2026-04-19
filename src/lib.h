#ifndef LIB_H // Include guard to prevent multiple inclusions!
#define LIB_H

#include <ctype.h>
#include <curl/curl.h>
#include <dirent.h>
#include <errno.h>
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define access _access
#define F_OK 0
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

#define MAX_LINE_LENGTH 256
#define MAX_MESSAGE_ID_LENGTH 64
#define MAX_MESSAGE_VALUE_LENGTH 256

// Structure to represent a message
typedef struct {
  char id[MAX_MESSAGE_ID_LENGTH];
  char value[MAX_MESSAGE_VALUE_LENGTH];
} FTLMessage;

// Structure to hold the response data
struct MemoryStruct {
  char *memory;
  size_t size;
};

int add(int a, int b);
// int parse_ftl_file(const char *filename, FTLMessage *messages,
//
// int *num_messages);
// void *fill_ftl(char ftl[104][6]);
bool starts_with(const char *str, const char *prefix);
char *get_substring(const char *str, int start, int length);
void log_message(const char *filename, const char *format, ...);
char *find_first_file_with_extension(const char *directory,
                                     const char *extension);
int file_exists(const char *filename);
int create_directory(const char *directory_name);
char *concat_strings(const char *str1, const char *str2);
void trim(char *str);

int is_safe_utf8(unsigned char c);
char *url_encode_utf8(const char *str);
#endif
