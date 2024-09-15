#!/bin/sh

writefile=""
writestr=""


check_arg()
{
    if [ "$#" -ne 2 ]; then
        echo "Error: Exactly two arguments are required."
        echo "Usage: $0 <write file path> <string to write>"
        exit 1
    fi

    writefile=$1
    writestr=$2
}

write_string()
{
    mkdir -p "$(dirname "$writefile")"
    if [ $? -ne 0 ]; then
        echo "Could not create the '$writefile' directory."
        exit 1
    fi
    
    echo "$writestr" > "$writefile"

    if [ $? -ne 0 ]; then
        echo "Could not write '$writestr' string into the '$writefile' directory."
        exit 1
    fi

    echo "File '$writefile' created successfully with the specified content."
}

check_arg "$@"
write_string