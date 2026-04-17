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

# Copy each source folder and Makefile
cp -r L1/src ../$dirName/L1 ;
cp -r L2/src ../$dirName/L2 ;
cp -r L3/src ../$dirName/L3 ;
cp -r IR/src ../$dirName/IR ;
cp -r LA/src ../$dirName/LA ;
cp -r LB/src ../$dirName/LB ;
cp -r LC/src ../$dirName/LC ;
cp -r util ../$dirName/ ;

cp Makefile ../$dirName/Makefile ;
cp L1/Makefile ../$dirName/L1/Makefile ;
cp L2/Makefile ../$dirName/L2/Makefile ;
cp L3/Makefile ../$dirName/L3/Makefile ;
cp IR/Makefile ../$dirName/IR/Makefile ;
cp LA/Makefile ../$dirName/LA/Makefile ;
cp LB/Makefile ../$dirName/LB/Makefile ;
cp LC/Makefile ../$dirName/LC/Makefile ;

# Copy the lib folder
cp -r lib ../$dirName/ ;

# Go into the new directory to clean stuff up
cd ../$dirName/

# Remove nonsense files
rm -rf .[a-z]* ;
rm -rf .[A-Z]* ;
rm -f */src/*.zip ;
rm -f */prog.* ;
rm -rf `find ./ -name .cache`
rm -rf `find ./ -name .DS_Store`
rm -f `find ./ -iname *.swp`
rm -f `find ./ -iname *.tar.bz2`

# Change permissions in each source folder
chmod 644 */src/*.cpp &> /dev/null ;
chmod 644 -f */src/*.hpp &> /dev/null ;
chmod 644 -f */src/*.h &> /dev/null ;

# Create the package
echo "1b220f1e5dc6f903f59918c6d6e9abae5084990573c6f21b8de6" > signature.txt ;
tar cfj ../${dirName}.tar.bz2 ./ ;
cd ../ ;
rm -r ${dirName} ;
