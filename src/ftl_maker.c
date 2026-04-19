#include "lib.h"
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

int is_safe_utf8(unsigned char c) {
  return isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}
char *url_encode_utf8(const char *str) {
  size_t len = strlen(str);
  char *encoded =
      malloc(3 * len + 1); // Max possible size (each byte could become %XX)
  if (!encoded)
    return NULL;

  char *p = encoded;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = str[i]; // Treat each byte as unsigned

    if (is_safe_utf8(c)) {
      // If it's a safe ASCII character, just copy it
      *p++ = c;
    } else if ((c & 0x80) == 0) {
      // If it's an ASCII character (0-127) that's not safe, encode it
      sprintf(p, "%%%02X", c);
      p += 3;
    } else {
      // Handle UTF-8 multi-byte characters:  Encode each byte individually
      sprintf(p, "%%%02X", c);
      p += 3;
    }
  }
  *p = 0; // Null terminate the encoded string
  return encoded;
}

// Function to trim leading and trailing whitespace from a string
void trim(char *str) {
  char *start = str;
  while (isspace((unsigned char)*start))
    start++;

  char *end = str + strlen(str) - 1;
  while (end > start && isspace((unsigned char)*end))
    end--;

  end[1] = '\0';

  memmove(str, start, (end - start) + 2);
}

// Function to parse an FTL file
int parse_ftl_file(const char *filename, FTLMessage *messages,
                   int *num_messages) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    perror("Error opening file");
    return 1;
  }

  char line[MAX_LINE_LENGTH];
  *num_messages = 0;

  while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
    trim(line);

    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == '\0') {
      continue;
    }

    char *equals_pos = strchr(line, '=');
    if (equals_pos != NULL) {
      // Extract message ID
      strncpy(messages[*num_messages].id, line, equals_pos - line);
      messages[*num_messages].id[equals_pos - line] = '\0';
      trim(messages[*num_messages].id);

      // Extract message value
      strncpy(messages[*num_messages].value, equals_pos + 1,
              MAX_MESSAGE_VALUE_LENGTH - 1);
      messages[*num_messages].value[MAX_MESSAGE_VALUE_LENGTH - 1] = '\0';
      trim(messages[*num_messages].value);

      (*num_messages)++;

      if (*num_messages >= 1000) {
        fprintf(stderr, "Warning.\n");
        break;
      }
    }
  }

  fclose(file);
  return 0;
}

// Structure to hold the response data
struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb,
                                    void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if (ptr == NULL) {
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

// Function to translate a single string
void *fill_ftl(char ftl[104][6]) {
  CURL *curl;
  CURLcode res;
  struct MemoryStruct chunk;
  char url[256];

  chunk.memory = malloc(1);
  chunk.size = 0;
  snprintf(url, sizeof(url), "https://mtranslate.myridia.com/ftl");
  curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
      goto cleanup;
    } else {
      json_error_t error;
      json_t *root = json_loads(chunk.memory, 0, &error);

      if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        goto cleanup;
      } else {
        json_t *ftl_array = json_object_get(root, "ftl");
        size_t array_size = json_array_size(ftl_array);
        // printf("Numbers: %zu\n", array_size);
        if (!json_is_array(ftl_array)) {
          printf("...is not array");
        }

        for (size_t i = 0; i < array_size; i++) {
          json_t *string_value = json_array_get(ftl_array, i);

          if (!json_is_string(string_value)) {
            fprintf(stderr, "Error:  %zu is not a string.\n", i);
            continue;
          }

          const char *str = json_string_value(string_value);

          if (str) {
            // strcpy(ftl[i], str);
            strncpy(ftl[i], str, 6 - 1);
            ftl[i][6 - 1] = '\0';
            // printf("String at index %zu: %s\n", i, str);
          } else {
            fprintf(stderr, "Error:  %zu.\n", i);
          }
        }

      cleanup_json:
        json_decref(root);
      }
    }

  cleanup:
    curl_easy_cleanup(curl);
  } else {
    fprintf(stderr, "curl_easy_init() failed\n");
  }

  free(chunk.memory);
}

// Function to translate a single string
char *translate(const char *source, const char *target, const char *value) {
  CURL *curl;
  CURLcode res;
  struct MemoryStruct chunk;
  char url[256]; // Adjust size as needed
  char *translation = NULL;

  chunk.memory = malloc(1);
  chunk.size = 0;

  char *encoded = url_encode_utf8(value);
  snprintf(url, sizeof(url), "https://mtranslate.myridia.com?s=%s&t=%s&v=%s",
           source, target, encoded);

  // fprintf(stderr, "%s\n", url);

  curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
      goto cleanup;
    } else {

      json_error_t error;
      json_t *root = json_loads(chunk.memory, 0, &error);

      if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        goto cleanup;
      } else {

        json_t *translated_text = json_object_get(root, "target_value");

        if (translated_text) {
          if (json_is_string(translated_text)) {
            const char *translation_str = json_string_value(translated_text);
            translation = strdup(translation_str);
            if (translation == NULL) {
              fprintf(stderr,
                      "Error: Could not allocate memory for translation.\n");
              goto cleanup_json;
            }
          } else {
            fprintf(stderr, "Error: 'translated_text' is not a string.\n");
            goto cleanup_json;
          }
        } else {
          fprintf(stderr, "Error: 'translated_text' field not found.\n");
          goto cleanup_json;
        }

      cleanup_json:
        json_decref(root);
      }
    }

  cleanup:
    // Clean up curl
    curl_easy_cleanup(curl);
  } else {
    fprintf(stderr, "curl_easy_init() failed\n");
    translation = NULL;
  }

  free(chunk.memory);

  return translation;
}

bool starts_with(const char *str, const char *prefix) {
  if (str == NULL || prefix == NULL) {
    return false;
  }
  bool x = strncmp(str, prefix, strlen(prefix));
  if (x == 0) {
    return true;
  } else {
    return false;
  }
}

char *get_substring(const char *str, int start, int length) {
  if (str == NULL) {
    return NULL;
  }

  size_t str_len = strlen(str);

  if (start < 0 || start >= str_len || length <= 0) {
    return NULL;
  }

  // Adjust length if it exceeds the remaining string length
  if (start + length > str_len) {
    length = str_len - start;
  }

  char *substring = (char *)malloc((length + 1) * sizeof(char));
  if (substring == NULL) {
    return NULL;
  }

  strncpy(substring, str + start, length);
  substring[length] = '\0'; // Null-terminate

  return substring;
}

void log_message(const char *filename, const char *format, ...) {
  FILE *fp = fopen(filename, "a");
  if (fp == NULL) {
    perror("Error opening log file");
    return; // Exit if file cannot be opened
  }

  va_list args;
  va_start(args, format);
  vfprintf(fp, format, args);
  va_end(args);

  fprintf(fp, "\n");

  fclose(fp);
}

// Function to find the first file ending with a specified extension in a
// directory
char *find_first_file_with_extension(const char *directory,
                                     const char *extension) {
  DIR *dir;
  struct dirent *ent;
  struct stat file_stat;
  char filepath[256]; // Buffer for the full file path (adjust as needed)
  char *found_file = NULL;
  size_t ext_len;

  // Check for NULL inputs
  if (directory == NULL || extension == NULL) {
    fprintf(stderr, "Error: NULL directory or extension provided.\n");
    return NULL;
  }

  // Calculate the length of the extension
  ext_len = strlen(extension);

  // Open the directory
  dir = opendir(directory);

  if (dir == NULL) {
    perror("Error opening directory");
    return NULL;
  }

  // Read directory entries
  while ((ent = readdir(dir)) != NULL) {
    // Ignore "." and ".." entries
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }

    // Construct the full file path
    snprintf(filepath, sizeof(filepath), "%s/%s", directory, ent->d_name);

    // Get file information
    if (stat(filepath, &file_stat) == 0) {
      // Check if it's a regular file
      if (S_ISREG(file_stat.st_mode)) {
        // Check if the filename ends with the specified extension
        size_t len = strlen(ent->d_name);
        if (len > ext_len &&
            strcmp(ent->d_name + len - ext_len, extension) == 0) {
          // Found a file ending with the specified extension
          found_file = strdup(ent->d_name);
          if (found_file == NULL) {
            perror("strdup failed");
            closedir(dir);
            return NULL;
          }
          closedir(dir);     // Close directory before returning
          return found_file; // Return the filename
        }
      }
    } else {
      fprintf(stderr, "Error getting file information for: %s\n", filepath);
    }
  }

  // Close the directory
  closedir(dir);
  return NULL; // No file found
}

int file_exists(const char *filename) {
  // Check if the filename is NULL
  if (filename == NULL) {
    fprintf(stderr, "Error: Filename is NULL\n");
    return 0; // Indicate that the file does not exist
  }

#ifdef _WIN32
  // Windows implementation using _access
  if (access(filename, F_OK) == 0) {
    // File exists
    return 1;
  } else {
    // File does not exist or there was an error
    return 0;
  }
#else
  // POSIX (Linux, macOS, etc.) implementation using stat
  struct stat buffer;
  if (stat(filename, &buffer) == 0) {
    // File exists
    return 1;
  } else {
    // File does not exist or there was an error
    return 0;
  }
#endif
}

// Function to create a directory if it doesn't already exist
int create_directory(const char *directory_name) {
  if (mkdir(directory_name, 0777) == 0) {
    // printf("Directory created successfully: %s\n", directory_name);
    return 0; // Success
  } else {
    // Check if the directory already exists
    if (errno == EEXIST) {
      // printf("Directory already exists: %s\n", directory_name);
      return 0; // Success (directory already exists)
    } else {
      // perror("Error creating directory");
      return 1; // Indicate an error
    }
  }
}

char *concat_strings(const char *str1, const char *str2) {
  if (str1 == NULL && str2 == NULL) {
    return NULL; // Or return an empty string, depending on desired behavior
  }

  if (str1 == NULL) {
    return strdup(str2); // Duplicate str2
  }

  if (str2 == NULL) {
    return strdup(str1); // Duplicate str1
  }

  size_t len1 = strlen(str1);
  size_t len2 = strlen(str2);
  size_t total_len = len1 + len2 + 1; // +1 for the null terminator

  char *result = (char *)malloc(total_len * sizeof(char));
  if (result == NULL) {
    perror("malloc failed");
    return NULL; // Indicate memory allocation failure
  }

  strcpy(result, str1); // Copy the first string
  strcat(result, str2); // Append the second string

  return result;
}

int main(int argc, char *argv[]) {

  int x = 5;
  int y = 3;
  int sum = add(x, y);
  printf("The sum is: %d\n", sum);

  char *basefile = NULL;

  // printf("filename: %s \n", filename);
  // printf("Arguments: %d \n", argc);
  // char *base = "-b";
  for (int i = 0; i < argc; i++) {
    if (strncmp(argv[i], "-b", 2) == 0) {
      // printf("  %b\n", strncmp(argv[i], base, 2));
      if (argv[i + 1]) {
        if (file_exists(argv[i + 1])) {
          basefile = argv[i + 1];
          // printf("  argv[%d] = %s\n", i, argv[i + 1]);
        }
      }
    }
  }
  if (basefile == NULL) {
    basefile = find_first_file_with_extension(".", ".ftl");
  }
  printf("basefile: %s \n", basefile);
  // return 0;

  // const char *directory_name2 = "i18n/xx";
  // int result2 = create_directory(directory_name2);

  // fill the ftl possible translations
  char ftl[104][6];
  fill_ftl(ftl);
  char folder_path[] = "i18n/";
  int result = create_directory(folder_path);

  for (int x = 0; x < 104; x++) {
    // const char *folder = ftl[x];
    char *code = get_substring(ftl[x], 0, 2);

    if (strlen(ftl[x]) == 5) {
      printf("size: %d code: %s folder: %s  \n", strlen(ftl[x]), code, ftl[x]);

      char *folder_path2 = concat_strings(folder_path, ftl[x]);
      int result2 = create_directory(folder_path2);
      char *file_path = concat_strings(folder_path2, "/app.ftl");

      FILE *fp = fopen(file_path, "w");
      FTLMessage messages[104];
      int num_messages = 0;

      if (parse_ftl_file(basefile, messages, &num_messages) == 0) {
        for (int i = 0; i < num_messages; i++) {
          char *translation = NULL;
          // if (same_code == 1) {
          if (!starts_with(messages[i].id, "!")) {
            // printf("code: %s ID: %s, Value: %s\n", code,
            messages[i].id,
                //        messages[i].value);
                translation = translate("en", code, messages[i].value);
            fprintf(fp, "%s = %s \n", messages[i].id, translation);
          } else {
            fprintf(fp, "%s = %s \n", messages[i].id, messages[i].value);
          }
          if (translation != NULL) {
            free(translation);
          }
          //}
        }
      }

      fclose(fp);
    }
  }

  return 0;
}
