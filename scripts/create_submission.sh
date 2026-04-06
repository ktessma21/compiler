#!/bin/bash

dirName="322_submission";

# Create the empty directory
if test -d ../$dirName ; then
  echo "ERROR: Please delete ../$dirName";
  exit 1;
fi

# Create subdirectories for each language
mkdir ../$dirName ;
mkdir ../$dirName/L1 ;
mkdir ../$dirName/L2 ;
mkdir ../$dirName/L3 ;
mkdir ../$dirName/IR ;
mkdir ../$dirName/LA ;
mkdir ../$dirName/LB ;
mkdir ../$dirName/LC ;

# Copy each source folder and the util folder
cp -r L1/src ../$dirName/L1 ;
cp -r L2/src ../$dirName/L2 ;
cp -r L3/src ../$dirName/L3 ;
cp -r IR/src ../$dirName/IR ;
cp -r LA/src ../$dirName/LA ;
cp -r LB/src ../$dirName/LB ;
cp -r LC/src ../$dirName/LC ;
cp -r util ../$dirName/ ;

# Copy the lib folder and remove common library files
cp -r lib ../$dirName/ ;
rm -r ../$dirName/lib/PEGTL/ ;
rm -r ../$dirName/lib/runtime.c ;

# Change permissions in each source folder
cd ../$dirName/
chmod 644 */src/*.cpp &> /dev/null ;
chmod 644 -f */src/*.hpp &> /dev/null ;
chmod 644 -f */src/*.h &> /dev/null ;

# Create the package
echo "e149bbb20cfdsfdssdf5604b3a7d8c89c2871a802fc2e0d3" > signature.txt ;
tar cfj ../${dirName}.tar.bz2 ./ ;
cd ../ ;
rm -r ${dirName} ;
