mkdir build
pushd build
gcc ../src/main.c ../src/util.c -o main -g -Wall
popd
