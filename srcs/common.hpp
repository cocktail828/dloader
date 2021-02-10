/*
 * @Author: sinpo828
 * @Date: 2021-02-10 10:16:49
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-10 14:47:57
 * @Description: file content
 */
#ifndef __COMMON__
#define __COMMON__
#include <iostream>
#include <string>

extern "C"
{
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
}

bool is_dir(const std::string &f)
{
    struct stat mstat;
    stat(f.c_str(), &mstat);

    return mstat.st_mode & S_IFDIR;
}

#endif //__COMMON__