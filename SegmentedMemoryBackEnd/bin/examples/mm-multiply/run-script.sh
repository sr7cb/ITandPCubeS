#!/bin/bash

matrix_size=$1
echo $matrix_size
block_size=$2
echo $block_size

# first convert two input text files into binary files
../../../tools/text-to-binary /study/IT-Language/Data-Samples/mm-multiply/a-$matrix_size a
../../../tools/text-to-binary /study/IT-Language/Data-Samples/mm-multiply/b-$matrix_size b

# then create an input output configuration file for the program
echo a >  input
echo b >> input
echo $block_size >> input
echo $block_size >> input
echo c >> input
echo $block_size >> input

# then run the mm-multiply program for all segments
for segment in {0..3}
do
	./mmm-on-laptop $segment input
done

# finally, convert the output file from binary to text form to output validation
../../../tools/binary-to-text c c-$matrix_size
