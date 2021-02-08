#/bin/bash
curr_dir=$(pwd)
g++ -g ./sdk_demo.cpp\
    -I $curr_dir/../include\
    -L $curr_dir/../lib/\
    -Wl,-rpath=$curr_dir/../lib\
    -Bstatic\
    -ldetours_sdk\
    -llog4cxx -lapr-1 -laprutil-1 -lexpat\
    -std=c++11 -lpthread -ldl\
    -o sdk_demo
if [ $? != 0 ]; then
    echo "build done"
fi
