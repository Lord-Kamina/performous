#!/bin/bash

. $GITDIR/hooks/functions.bash

changed_files=`getChangedFiles`
issue_found=0

IFS=$'\n' # bash specific
for file in $changed_files
do
	result=`cat "$file" | grep -P -n '^[\t]*(    | {1,3}\t)'`

	if [ -n "$result" ]
	then
		echo "file with indent issue: $file"
		echo "$result"
		issue_found=1
	else
		echo "file without issue: $file"
	fi
done

if [ $issue_found = 1 ]
then
	exit 1
fi

exit 0