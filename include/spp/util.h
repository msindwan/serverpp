/**
 * Serverpp Util
 *
 * Description: Miscellaneous helper functions for spp.
 * Author: Mayank Sindwani
 * Date: 2015-09-10
 */

#ifndef __UTIL_SPP_H__
#define __UTIL_SPP_H__

#include <fstream>
#include <string>
#include <regex>
#include <map>

namespace spp 
{
    // Helper functions.
    void render_template(std::string&, std::map<std::string, std::string>*);
    char* read_file(const char*, size_t*);
    char* get_ext(const char*, size_t);
    int trim(char**, size_t, int*);
}

#endif