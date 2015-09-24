/**
 * Serverpp.cpp
 *
 * Description: Entry point for the windows Serverpp service.
 * Author: Mayank Sindwani
 * Date: 2015-06-03
 */

#include <stdio.h>

using namespace std;

// Commands.
enum COMMANDS
{
    HELP,
    INSTALL,
    REMOVE
};

/**
 * Print Usage
 *
 * @description: prints the usage for the serverpp windows service.
 */
 static __inline void print_usage()
 {
    // printf("Serverpp Service \n");
    // printf("USAGE for Windows: [PATH] {command (optional)} \n\n");
    // printf("COMMANDS:\n");
    // printf("\t-i or install : Installs the service.\n");
    // printf("\t-r or remove  : Removes the service.\n");
    // printf("\t-h or help    : Prints service usage.\n");
    // printf("\t<no command>  : Starts the service if not already running.\n");
 }

/**
 * Entry point
 *
 * @param[in] {argc} // The number of arguments
 * @param[in] {argv} // Commands
 */
int main(int argc, char* argv[])
{

}