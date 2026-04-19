#include <curl/curl.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
// Structure to hold the response data
struct MemoryStruct {
  char *memory;
  size_t size;
};

// Callback function to write the curl response to memory
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
char *translate(const char *source, const char *target, const char *value) {
  CURL *curl;
  CURLcode res;
  struct MemoryStruct chunk;
  char url[256]; // Adjust size as needed
  char *translation = NULL;

  // Initialize the memory chunk
  chunk.memory = malloc(1); // will be grown as needed by the realloc above
  chunk.size = 0;           // no data at this point

  // Construct the URL
  snprintf(url, sizeof(url), "https://mtranslate.myridia.com?s=%s&t=%s&v=%s",
           source, target, value);

  // Initialize curl
  curl = curl_easy_init();

  if (curl) {
    // Set the URL
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Set the write function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);

    // Pass the chunk structure to the callback function
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    // Perform the request
    res = curl_easy_perform(curl);

    // Check for errors
    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
      goto cleanup; // Use goto for error handling within the function
    } else {
      // Parse the JSON
      json_error_t error;
      json_t *root = json_loads(chunk.memory, 0, &error);

      if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        goto cleanup;
      } else {
        // Extract the 'translated_text' field
        json_t *translated_text = json_object_get(root, "target_value");

        if (translated_text) {
          if (json_is_string(translated_text)) {
            const char *translation_str = json_string_value(translated_text);
            translation =
                strdup(translation_str); // Allocate memory for the translation
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
        json_decref(root); // Release the JSON object
      }
    }

  cleanup:
    // Clean up curl
    curl_easy_cleanup(curl);
  } else {
    fprintf(stderr, "curl_easy_init() failed\n");
    translation = NULL; // Indicate failure
  }

  // Free the memory allocated for the response
  free(chunk.memory);

  return translation;
}

// Function to write a log message to a file
void log_message(const char *filename, const char *format, ...) {
  FILE *fp = fopen(filename, "a"); // Open file in append mode
  if (fp == NULL) {
    perror("Error opening log file");
    return; // Exit if file cannot be opened
  }

  // Get current time
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);

  // Write timestamp to the file
  fprintf(fp, "%s: ", timestamp);

  // Write the log message using variable arguments
  va_list args;
  va_start(args, format);
  vfprintf(fp, format, args);
  va_end(args);

  fprintf(fp, "\n"); // Add a newline character

  fclose(fp); // Close the file
}
// Function to load words from a file and translate them
void translate_from_file(const char *filename, const char *source,
                         const char *target, const int start) {
  FILE *fp;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error: Could not open file %s\n", filename);
    return;
  }

  printf("Translations from file %s:\n", filename);

  int counter = 0;
  while ((read = getline(&line, &len, fp)) != -1) {
    counter++;
    if (counter < start) {
      continue;
    }
    // Remove trailing newline character
    if (line[read - 1] == '\n') {
      line[read - 1] = '\0';
      read--;
    }
    srand(time(NULL));

    int min = 1;
    int max = 1;
    int random_number = (rand() % (max - min + 1)) + min;

    printf("Counter %d - Random number: %d  \n", counter, random_number);
    log_message("log.log", "Counter %d", counter);

    sleep(random_number);

    //    printf("Original: %s\n", line);

    // Translate the line
    char *translation = translate(source, target, line);

    if (translation != NULL) {
      printf("Original: %s\n", line);
      printf("Translation: %s\n", translation);
      free(translation); // Free the allocated memory
    } else {
      fprintf(stderr, "Translation failed for: %s\n", line);
    }
  }

  fclose(fp);
  if (line)
    free(line); // Free the line buffer
}

int main(int argc, char *argv[]) {
  printf("Arguments: %d \n", argc);
  /*
  CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(res));
    return 1;
  }
  printf("Arguments: %d \n", argc);
  for (int i = 0; i < argc; i++) {
    printf("  argv[%d] = %s\n", i, argv[i]);
  }

  // Translate words from a file
  char *filename = argv[1];
  char *source = argv[2];
  char *target = argv[3];
  char *start = argv[4];
  int s;
  s = 0;
  if (start) {
    s = atoi(start);
    printf("%d\n", s);
  }

  translate_from_file(filename, source, target, s);
  */
  return 0;
}
