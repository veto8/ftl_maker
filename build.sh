#!/bin/bash

SOURCE_FILE="src/ftl_maker.c"
SOURCE_FILE2="src/lib.c"
SOURCE_FILE3="src/lib.h"
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )" 


run_build_and_execute() {
  echo "File changed. Rebuilding and running..."
  rm -rf build
  rm ftl_maker
  #gcc -o translate src/translate.c -lcurl -ljansson
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build
 ./run.sh
}

# Check if inotifywait is installed
if ! command -v inotifywait &> /dev/null
then
    echo "inotifywait is not installed. Please install it (e.g., sudo apt-get install inotify-tools)."
    exit 1
fi

# Initial build and execution
run_build_and_execute

# Monitor the source file for changes
while inotifywait -e modify "$SOURCE_FILE" "$SOURCE_FILE2" "$SOURCE_FILE3"; do
    echo "...changed"
    run_build_and_execute
done


