#!/bin/bash

# Check if a directory has been provided as an argument
if [ $# -eq 0 ]; then
    echo "No directory provided. Usage: ./run_program.sh [directory]"
    exit 1
fi

DIRECTORY=$1

# Check if the provided argument is indeed a directory
if [ ! -d "$DIRECTORY" ]; then
    echo "The provided argument is not a directory. Please provide a valid directory."
    exit 1
fi

# Compile the edge_detector program
gcc -o edge_detector edge_detector.c -lm

# Check if compilation was successful
if [ $? -ne 0 ]; then
    echo "Compilation failed."
    exit 1
fi

# Create an array to hold all .ppm files in the directory
PPM_FILES=()

# Populate the array with .ppm files found in the directory
for FILE in "$DIRECTORY"/*.ppm; do
    if [ -f "$FILE" ]; then
        PPM_FILES+=("$FILE")
    fi
done

# Check if no .ppm files were found
if [ ${#PPM_FILES[@]} -eq 0 ]; then
    echo "No .ppm files found in the directory."
    exit 1
fi

# Run the edge_detector program on all .ppm files found
./edge_detector "${PPM_FILES[@]}"

echo "Edge detection completed for all .ppm files."