
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include "../incs/Server.hpp"

/* Déclaration de l'instance du serveur */
Server* serverInstance = NULL;

/* Gestionnaire de signaux */
void handleSignal(int signal)
{
    const char* signalName;
    if (signal == CTRL_C)
        signalName = "SIGINT (ctrl + c)";
    else if (signal == CTRL_Z)
        signalName = "SIGTSTP (ctrl + z)";
    else
        signalName = "Unknown";

    std::cout << "\nSignal " << signalName << " reçu, fermeture du serveur..." << std::endl;

    if (serverInstance)
    {
        serverInstance->shutdown();
        serverInstance = NULL;
    }
    exit(0);
}

/**
 * Main function
 */
int main(int argc, char **argv)
{
    /* Vérification des arguments */
    if (argc != THREE_ARGMNTS)
    {
        std::cerr << "Usage: ./micro_irc <port> <password>" << std::endl;
        return EXIT_FAILURE;
    }

    /* Conversion du port en entier */
    char *endptr;
    long port = std::strtol(argv[PORT_ARG_INDEX], &endptr, 10);

    /* Vérification de la validité du port */
    if (*endptr != '\0' || port <= 0 || port > MAX_UINT16_BITS)
    {
        std::cerr << "Port invalide. Veuillez spécifier un port entre 1 et 65535." << std::endl;
        return EXIT_FAILURE;
    }

    /* Récupération du mot de passe */
    std::string password(argv[2]);

    /* Configuration des gestionnaires de signaux */
    struct sigaction sa;
    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(CTRL_C, &sa, NULL) == FAILURE || sigaction(CTRL_Z, &sa, NULL) == FAILURE)
    {
        std::cerr << "Erreur lors de la configuration des signaux." << std::endl;
        return EXIT_FAILURE;
    }

    /* Tente de créer et démarrer le serveur IRC avec le port et le mot de passe fournis */
    try {
        std::cout << "\033[1;34m"; // Set text color to bright magenta
        std::cout << "\n";
        std::cout << "====================================================================================\n";
        std::cout << " * --@-- ** --@-- ** --@-- ** --@-- ** --@-- ** --@-- ** --@-- ** --@-- ** --@-- *  \n";
        std::cout << "====================================================================================\n";
        std::cout << "\033[1;36m"; // Set text color to bright cyan 
        std::cout << "\n";                                                                           
        std::cout << "                    :-:::-=-::::::           .::::   .=*%@@@@#+            ..          \n";
        std::cout << "    +%*==++**#%*+: :%@@@@@@@%%%%#*          +@@%*: *@@@@@@@@@@@:       -*%#%#.        \n";
        std::cout << "    +@@@@*+==-:.    =*%@@@@@@#+==-           @@@@=.*@@@@@@*::=%@.     *@@*##@@-       \n";
        std::cout << "    .@@@*              .@@@*                =@@@*  .%++@@@-  -%@#    *@@*%**#@%       \n";
        std::cout << "    .@@%*  :-=*+-      :@@@+                *%@@-     ##@@==%@@@@   =@@%@. :%%*       \n";
        std::cout << "    .@@@@@@@@%*=-      =@@@=                +#@@.     @@@@@@@%##:   #@@%*   .-        \n";
        std::cout << "     @@@*+:.           -@@@-                +%@%      @@@@@@*-.     %@@@:             \n";
        std::cout << "     *@@+-             *=@@:                =@@%.    .%@@@+@%=.      +@@%---=*%#-     \n";
        std::cout << "     =%@*.             *.##.  -+++=+++*+.:  #@@@@=    *@@@  =#@@@+     :=**%@@%*-     \n";
        std::cout << "     .-:.              : #*.  :::::--==-     =%%=     .#@@     :==.                   \n";
        std::cout << "                                                         :=.                        \n";
        std::cout << "\n";
        std::cout << "\033[1;34m"; // Change text color to bright magenta
        std::cout << "====================================================================================\n";
        std::cout << " * --@-- ** --@-- ** --@-- ** --@-- ** --@-- ** --@-- ** --@-- ** --@-- ** --@-- *  \n";
        std::cout << "====================================================================================\n";
        std::cout << "\n";
        std::cout << "\033[1;32m"; // Set text color to bright green
        std::cout << "🚀 Server started successfully!\n";
        std::cout << "\033[1;34m"; // Magenta color for separators
        std::cout << "------------------------------------------------------------------------------------\n";
        std::cout << "\033[1;33m"; // Set text color to bright yellow
        std::cout << "🔌 Port     : \033[1;37m" << port << "\n";      // White color for values
        std::cout << "\033[1;33m" << "🔑 Password : \033[1;37m" << password << "\n";
        std::cout << "\033[1;34m"; // Magenta color for separators
        std::cout << "====================================================================================\n";
        std::cout << "\033[0m"; // Reset text color
        Server server(static_cast<unsigned short>(port), password);
        serverInstance = &server;
        server.run();
    }

    /* Gestion des exceptions */
    catch (const std::exception &e)
    {
        std::cerr << "Erreur du serveur : " << e.what() << std::endl;
        if (serverInstance)
        {
            /* Ensuring memory release on error */
            delete serverInstance;
            serverInstance = NULL;
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
