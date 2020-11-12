#/bin/bash
curr_dir=$(pwd)
g++ -g ./sdk_demo.cpp -I $curr_dir/../include -L $curr_dir/../lib -std=c++11 -lpthread -ldl -lglog -ldetours_sdk -o sdk_demo
