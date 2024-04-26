#!/bin/sh

if [ $# -lt 2 ]
then
	echo "please specifiy the directory and string to be searched in files"
	exit 1
fi

filesdir=$1
searchstr=$2

if [ -d ${filesdir}  ]
then
	nb_files=$(find ${filesdir} -type f | wc -l)
	nb_occurence=$(grep -r ${searchstr} ${filesdir} | wc -l)
	#echo "The number of files is" ${nb_files} "and contains" ${nb_occurence} " occurences of" ${searchstr}
	echo "The number of files are "${nb_files}" and the number of matching lines are "${nb_occurence} 
	exit 0

else
	echo ${filesdir} " is not a directory"
	exit 1
fi
