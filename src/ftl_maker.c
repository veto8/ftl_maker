#include "lib.h"
#define TRANS 104

int main(int argc, char *argv[]) {
  char *basefile = NULL;
  char *app = "app.ftl";
  char *folder_path = "i18n/";

  for (int i = 0; i < argc; i++) {
    if (strncmp(argv[i], "-b", 2) == 0) {
      // printf("  %b\n", strncmp(argv[i], base, 2));
      if (argv[i + 1]) {
        if (file_exists(argv[i + 1])) {
          basefile = sanitize_string(argv[i + 1]);
        }
      }
    }

    if (strncmp(argv[i], "-a", 2) == 0) {
      if (argv[i + 1]) {
        char *sanitized_string2 = sanitize_string(argv[i + 1]);
        app = concat_strings(sanitized_string2, ".ftl");
      }
    }

    if (strncmp(argv[i], "-f", 2) == 0) {
      if (argv[i + 1]) {
        char *sanitized_string3 = sanitize_string(argv[i + 1]);
        folder_path = concat_strings(sanitized_string3, "/");
      }
    }
  }
  if (basefile == NULL) {
    basefile = find_first_file_with_extension(".", ".ftl");
  }
  if (basefile != NULL) {
    printf("basefile: %s \n", basefile);
    printf("app: %s \n", app);
    printf("folder: %s \n", folder_path);

    char ftl[TRANS][6];
    fill_ftl(ftl);

    int result = create_directory(folder_path);

    for (int x = 0; x < TRANS; x++) {
      // const char *folder = ftl[x];
      char *code = get_substring(ftl[x], 0, 2);

      if (strlen(ftl[x]) == 5 || strlen(ftl[x]) == 2) {
        printf("%d/%d ...make %s/%s  \n", x, TRANS, ftl[x], app);
        char *folder_path2 = concat_strings(folder_path, ftl[x]);
        int result2 = create_directory(folder_path2);
        char *file_path = concat_strings(folder_path2, app);

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
  } else {
    printf("Missing baseline!. Do you have a *.ftl file in your folder or did "
           "you give a valid base file \n");
    printf("Example: ftl_make -b my_basefile.ftl");
  }
}
