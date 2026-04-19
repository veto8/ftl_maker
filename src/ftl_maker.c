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
      /*
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
      */
      fclose(fp);
    }
  }

  return 0;
}
