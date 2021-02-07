/*
 * @Author: sinpo828
 * @Date: 2021-02-07 12:21:12
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-07 19:18:43
 * @Description: file content
 */
#include <iostream>

#include <getopt.h>

#include "firmware.hpp"

using namespace std;

void usage()
{
}

int main(int argc, char **argv)
{
    Firmware fm(argv[1]);
    fm.pacparser(true);
    fm.xmlparser();

    return 0;
}