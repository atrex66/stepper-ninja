#!/bin/bash

# Script to extract ioLibrary_Driver for io-samurai project

# Check if unzip is installed
if ! command -v unzip &> /dev/null; then
    echo "Error: unzip is not installed. Please install it (e.g., sudo apt install unzip)."
    exit 1
fi

# Check if ioLibrary_Driver.zip exists
if [ ! -f "ioLibrary_Driver.zip" ]; then
    echo "Error: ioLibrary_Driver.zip not found in firmware/lib directory."
    exit 1
fi

# Extract ioLibrary_Driver.zip
echo "Extracting ioLibrary_Driver.zip..."
unzip -o ioLibrary_Driver.zip

# Verify extraction
if [ -d "ioLibrary_Driver" ]; then
    echo "ioLibrary_Driver successfully extracted to firmware/lib/ioLibrary_Driver."
else
    echo "Error: Extraction failed."
    exit 1
fi
