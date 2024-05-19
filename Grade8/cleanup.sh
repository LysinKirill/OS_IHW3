#!/bin/bash

for file in *; do
    if [[ ! "$file" == *.* ]]; then
        rm "$file"
        echo "Deleted: $file"
    fi
done
