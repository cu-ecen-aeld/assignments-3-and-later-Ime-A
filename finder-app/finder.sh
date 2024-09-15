#!/bin/sh

filesdir=""
searchstr=""

function check_arg()
{
    if [ "$#" -ne 2 ]; then
        echo "Error: Exactly two arguments are required."
        echo "Usage: $0 arg1 arg2"
        exit 1
    fi

    filesdir=$1
    searchstr=$2
}

function change_filesdir()
{
    cd $filesdir

    if[$? -ne 0]; then
        echo "Directory does not exist!"
    else 
        echo "Searching for $searchstr in $filesdir  directory."
    fi

}

function look_for_string()
{

}