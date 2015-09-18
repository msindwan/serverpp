/**
 * Serverpp.cpp
 *
 * Description: Entry point for the windows Serverpp service.
 * Author: Mayank Sindwani
 * Date: 2015-06-03
 */

#include "controller.h"

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
    printf("Serverpp Service \n");
    printf("USAGE for Windows: [PATH] {command (optional)} \n\n");
    printf("COMMANDS:\n");
    printf("\t-i or install : Installs the service.\n");
    printf("\t-r or remove  : Removes the service.\n");
    printf("\t-h or help    : Prints service usage.\n");
    printf("\t<no command>  : Starts the service if not already running.\n");
 }

/**
 * Entry point
 *
 * @param[in] {argc} // The number of arguments
 * @param[in] {argv} // Commands
 */
int main(int argc, char* argv[])
{
    map < string, int >::iterator it;
    map < string, int > cmds;

    // Map supported commands.
    cmds["-i"]      = INSTALL;
    cmds["-r"]      = REMOVE;
    cmds["-h"]      = HELP;

    if (argc > 1)
    {
        if ((it = cmds.find(string(argv[1]))) == cmds.end())
        {
            printf("\"%s\" is not a valid command.\n", argv[1]);
            print_usage();
        }
        else
        {
            switch (it->second)
            {
                // Install serverpp
                case INSTALL:
                    install_service();
                    break;

                // Uninstall serverpp
                case REMOVE:
                    uninstall_service();
                    break;

                // Serverpp usage
                case HELP:
                    print_usage();
                    break;
            };
        }

        return 0;
    }

    // Initialize and run the service.
    return spp_svc_init();
}