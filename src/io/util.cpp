/**
 * Serverpp util implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-09-10
 */

#include <spp\util.h>

using namespace std;

/**
 * render_template
 *
 * @description Replaces escaped sequences with the provided parameters.
 * @param[out] {src}    // The templated string.
 * @param[out] {param}  // The parameters to use.
 */
void spp::render_template(string& src, map<string, string>* params)
{
    // TODO: This a temporary solution that should be optimized.
    map<string, string>::iterator it;

    for (it = params->begin(); it != params->end(); it++)
    {
        regex rgx("<%" + it->first + "%>");
        src = regex_replace(src, rgx, it->second + "$2");
    }
}

/**
 * trim
 *
 * @description Skips to the next word in a string.
 * @param[in]  {buffer} // The string to trim.
 * @param[out] {size}   // The buffer size.
 */
int spp::trim(char** buffer, size_t size, int* pos)
{
    unsigned int cpos, length;

    cpos = 0;
    (*buffer) += *pos;

    // Skip to the next whitespace character.
    while (cpos < size && !isspace((*buffer)[cpos])) cpos++;

    length = cpos;

    // Ignore subsequent whitespace characters.
    while (cpos < size &&  isspace((*buffer)[cpos])) cpos++;

    *pos = cpos;
    return length;
}

/**
 * read_file
 *
 * @description Reads a file into a string.
 * @param[out] {path} // The file path.
 * @param[in] {size}  // The size of the file.
 * @returns // The file read into a buffer.
 */
char* spp::read_file(const char* path, size_t* size)
{
    ifstream file;
    char* buffer;

    file.open(path, ios::in | ios::binary | ios::ate);

    // If the file isn't open, return NULL.
    if (!file.is_open())
        return NULL;

    // Get the size of the file.
    file.seekg(0, ios::end);
    *size = (size_t)file.tellg();
    file.seekg(0, ios::beg);

    // Read the file into the buffer.
    buffer = new char[*size + 1];
    file.read(buffer, *size);
    buffer[*size] = '\0';

    return buffer;
}

/**
 * get_ext
 *
 * @description Returns a pointer to the extension of the provided path.
 * @param[out] {path} // The file path.
 * @param[out] {size} // The size of the path.
 * @returns // A pointer to the extension of the path.
 */
char* spp::get_ext(const char* path, size_t size)
{
    char* p = (char*)(path + size - 1);

    while (size > 0)
    {
        // Return the pointer when the period is encountered.
        if (*p == '.')
            return p;

        // An extension was not found.
        if (*p == '\\' || *p == '/')
            break;

        p--;
        size--;
    }

    return NULL;
}