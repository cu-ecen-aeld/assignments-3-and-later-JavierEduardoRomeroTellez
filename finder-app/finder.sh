#!/bin/bash

if [ $# -lt 2 ]
then
	echo "Error: Please provide exactly 2 arguments"
	exit 1
fi

filesdir="$1"
searchstr="$2"

if [ -d "$filesdir" ]
then
	num_files=$(find "$filesdir" -type f | wc -l)

	num_matches=0

	for file in $(find "$filesdir" -type f)
	do
		current_matches=$(grep -r "$searchstr" "$file" | wc -l)
		num_matches=$((num_matches + current_matches))
	done	

	echo "The number of files are $num_files and the number of matching lines are $num_matches"
else
	echo "Error: Directory $1 does not exist"
	exit 1
fi
