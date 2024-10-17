#!/bin/bash

if [ $# -lt 2 ]
then
	echo "Error: Please provide the 2 arguments"
	exit 1
fi

writefile="$1"
writestr="$2"

if ! echo "$writestr" > "$writefile"  
then
	mkdir -p "$(dirname "$writefile")"
fi

if ! echo "$writestr" > "$writefile"
then
	echo "Error: Could not write to file '$writefile'"
	exit 1
fi

echo "Successfully wrote '$writestr' to file '$writefile'"

