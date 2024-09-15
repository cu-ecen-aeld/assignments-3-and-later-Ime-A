#!/bin/sh

filesdir=""
searchstr=""
num_of_instances=0
num_of_files=0

check_arg()
{
    if [ "$#" -ne 2 ]; then
        echo "Error: Exactly two arguments are required."
        echo "Usage: $0 <filesdir> <searchstr>"
        exit 1
    fi

    filesdir=$1
    searchstr=$2
}

change_filesdir()
{
    cd "$filesdir"

    if [ $? -ne 0 ]; then
        echo "Directory does not exist!"
        exit 1
    else 
        echo "Searching for '$searchstr' in '$filesdir'  directory."
    fi

}

look_for_string()
{
    num_of_instances=$(grep -rlI "$searchstr" $filesdir | wc -l)
    num_of_files=$(grep -rI "$searchstr" $filesdir | wc -l)

    echo "The number of files are $num_of_files and the number of matching lines are $num_of_instances"
}


check_arg "$@"
change_filesdir
look_for_string