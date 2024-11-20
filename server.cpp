#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdio.h>
#include <string.h>
#include <ldap.h>
#include "getpass.h"
#include <ctime>
#include <bits/stdc++.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;

const int BUFFER_SIZE = 2048;

class MailServer
{
public:
    // Constructor
    MailServer(int port, const std::string &spoolDirectory) : port(port), spoolDirectory(spoolDirectory)
    {
        if (!std::filesystem::exists(spoolDirectory))
            std::filesystem::create_directory(spoolDirectory);
    }
    // Start the server
    void start()
    {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1)
        {
            std::cerr << "Socket creation failed!" << std::endl;
            return;
        }
        sockaddr_in server_address;
        server_address.sin_family = AF_INET;         // IPv4
        server_address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
        server_address.sin_port = htons(port);       // Convert port to network byte order

        if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        { // Bind the socket to the address and port
            std::cerr << "Binding failed!" << std::endl;
            return;
        }
        int rc;
        if((rc = listen(server_fd, 3)) == -1)
            std::cerr << "Error occured while listening\n";
        
        signal(SIGCHLD, SIG_IGN);
        std::cout << "Server listening on port " << port << std::endl; // Print the port number

        while (true)
        {
            sockaddr_in client_address;
            socklen_t addr_len = sizeof(client_address);
            int client_fd = accept(server_fd, (struct sockaddr *)&client_address, &addr_len);
            if (client_fd < 0)
            {
                std::cerr << "Failed to accept client connection!" << std::endl;
                continue;
            }

            // Fork to handle the client
            pid_t pid = fork();

            if (pid < 0)
            {
                std::cerr << "Fork failed!" << std::endl;
                close(client_fd);
            }
            else if (pid == 0)
            {
                // Child process: handle the client
                close(server_fd); // Child doesn't need the listening socket
                handleClient(client_fd);
                close(client_fd);
                exit(0); // Terminate the child process when done
            }
            else
            {
                // Parent process: close the client socket
                close(client_fd);
            }
        }

        close(server_fd); // Close the server socket
    }

private:
    int port;
    std::string spoolDirectory;

    std::string getClientIp(int client_fd)
    {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        char ip_str[INET_ADDRSTRLEN];

        if (getpeername(client_fd, (struct sockaddr *)&client_addr, &addr_len) == -1)
        {
            perror("getpeername failed\n");
            return "";
        }
        if (inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, size(ip_str)) == nullptr)
        {
            perror("inet_ntop failed\n");
            return "";
        }
        return std::string(ip_str);
    }

    void handleClient(int client_fd)
    {                             // Handle the client connection
        char buffer[BUFFER_SIZE]; // Buffer to store received data

        bool isLoggedIn = false;

        int loginAttempts = 3;

        time_t currentTime;

        std::string message;
        std::string clientIp = getClientIp(client_fd);
        std::string timestampStr = to_string(time(&currentTime));

        if (!isInBlackList(timestampStr, clientIp))
        {
            while (true)
            {
                ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0); // Receive data from the client
                if (bytes_received <= 0)
                { // Check if the connection was closed or an error occurred
                    std::cerr << "Client disconnected or error occurred!" << std::endl;
                    break;
                }

                buffer[bytes_received] = '\0'; // Null-terminate the received data
                message = buffer;

                if (loginAttempts <= 0)
                {
                    time_t timestamp;
                    std::string loginErrorMessage = "Login has failed too many times!\n";
                    send(client_fd, loginErrorMessage.c_str(), loginErrorMessage.size(), 0);
                    addToBlacklist(time(&timestamp), clientIp);
                    close(client_fd);
                }
                if (isLoggedIn)
                {
                    if (message.substr(0, 4) == "SEND")
                        processSend(client_fd, message);
                    else if (message.substr(0, 4) == "LIST")
                        processList(client_fd, message);
                    else if (message.substr(0, 4) == "READ")
                        processRead(client_fd, message);
                    else if (message.substr(0, 3) == "DEL")
                        processDelete(client_fd, message);
                    else if (message.substr(0, 4) == "QUIT")
                        break;
                }
                else if (!isLoggedIn && loginAttempts > 0)
                {
                    if (message.substr(0, 5) == "LOGIN")
                    {
                        if (processLogin(client_fd, message, loginAttempts) == true)
                            isLoggedIn = true;

                        else
                        {
                            isLoggedIn = false;
                            loginAttempts--;
                        }
                    }
                    else if (message.substr(0, 4) == "QUIT")
                        break;
                }
            }
        }
        if (isInBlackList(timestampStr, clientIp))
            close(client_fd);
    }

    void processSend(int client_fd, const std::string &message)
    { // Process the SEND command
        std::istringstream iss(message);
        std::string command, username, receiver, subject, content, line;
        std::getline(iss, command, '\n');
        std::getline(iss, username, '\n');
        std::getline(iss, receiver, '\n');
        std::getline(iss, subject, '\n');

        std::string okMessage = "Message sent successfully\n";
        std::string errMessage = "Error saving message\n";

        while (std::getline(iss, line))
        {
            if (line == ".")
                break;
            content += line + "\n";
        }

        std::string message_filename = spoolDirectory + "/" + username + "_messages.txt"; // Create the message filename
        std::ofstream outfile(message_filename, std::ios::app);                           // Open the file in append mode
        if (outfile.is_open())
        { // Check if the file was opened successfully
            outfile << "Sender: " << username << "\n"
                    << "Receiver: " << receiver << "\n"
                    << "Subject: " << subject << "\n"
                    << "Content: " << content << "\n"
                    << "-----\n";
            outfile.close();
            send(client_fd, okMessage.c_str(), okMessage.size() - 1, 0);
            cout << "OK" << endl;
        }
        else
        { // Error saving the message
            send(client_fd, errMessage.c_str(), errMessage.size(), 0);
            cout << "ERR" << endl;
        }
    }

    void processList(int client_fd, const std::string &message)
    { // Process the LIST command
        std::istringstream iss(message);
        std::string command, username;
        std::getline(iss, command, '\n');
        std::getline(iss, username, '\n');

        std::string errMessage = "No messages found.\n";

        std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
        std::ifstream infile(message_filename);
        if (infile.is_open())
        { // Check if the file was opened successfully
            std::string line;
            std::string response;
            int id = 0;
            bool foundMessage = false;

            std::string sender, receiver, subject, content;

            // Read lines from the file
            while (std::getline(infile, line))
            { // Read the file line by line
                if (line.substr(0, 7) == "Sender:")
                {
                    foundMessage = true;
                    sender = line;                  // Read sender line
                    std::getline(infile, receiver); // Read receiver line
                    std::getline(infile, subject);  // Read subject line
                    std::getline(infile, content);  // Read content line
                    response += "ID: " + std::to_string(id) + "\n" + sender + "\n" + receiver + "\n" + subject + "\n";
                    id++;
                    // Skip separator line ("-----")
                    std::getline(infile, line);
                }
            }

            infile.close();

            if (foundMessage)
            { // Check if messages were found
                send(client_fd, response.c_str(), response.size(), 0);
                cout << "OK" << endl;
            }
            else
            { // No messages found
                send(client_fd, errMessage.c_str(), errMessage.size(), 0);
                cout << "ERR" << endl;
            }
        }
        else
        { // Error opening the file
            send(client_fd, errMessage.c_str(), errMessage.size(), 0);
            cout << "ERR" << endl;
        }
    }

    std::string trim(const std::string &str)
    { // Trim whitespace from the string
        size_t first = str.find_first_not_of(' ');
        if (first == std::string::npos)
            return ""; // no content
        size_t last = str.find_last_not_of(' ');
        return str.substr(first, (last - first + 1));
    }

    void processRead(int client_fd, const std::string &message)
    { // Process the READ command
        std::istringstream iss(message);
        std::string command, username, messageIdStr, line;
        std::getline(iss, command, '\n');
        std::getline(iss, username, '\n');
        std::getline(iss, messageIdStr, '\n');

        std::string errInvalidFormat = "Invalid message ID format\n";
        std::string errMsgNotFound = "Error, message not found\n";
        std::string errOpeningFile = "Error occured while opening the file\n";

        // Trim messageIdStr to remove whitespace
        messageIdStr = trim(messageIdStr); // Trim whitespace from the message ID
        int messageID;

        // Validate if messageIdStr is a number before proceeding
        try
        {
            messageID = std::stoi(messageIdStr); // Convert the message ID to an integer
        }
        catch (std::invalid_argument &)
        {
            send(client_fd, errInvalidFormat.c_str(), errInvalidFormat.size(), 0); // Send an error message
            cout << "ERR" << endl;
            return;
        }

        std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
        std::ifstream infile(message_filename);

        if (infile.is_open())
        {
            std::string line;
            std::string response;
            int currentId = 0;

            // Read lines from the file
            while (std::getline(infile, line))
            { // Read the file line by line
                // Check for the start of a message
                if (line.substr(0, 7) == "Sender:")
                {
                    if (currentId == messageID)
                    {
                        response += line + "\n";    // Sender
                        std::getline(infile, line); // Receiver
                        response += line + "\n";    // Receiver
                        std::getline(infile, line); // Subject
                        response += line + "\n";
                        while (line != "-----")
                        {
                            std::getline(infile, line); // Content
                            response += line + "\n";    // Content
                        }
                        send(client_fd, response.c_str(), response.size(), 0);
                        std::cout << "OK" << std::endl;
                        return;
                    }
                    currentId++; // Increment the ID for each message
                    // Skip the next three lines (Receiver, Subject, Content)
                    std::getline(infile, line); // Skip Receiver
                    std::getline(infile, line); // Skip Subject
                    std::getline(infile, line); // Skip Content
                }
            }

            infile.close();
            send(client_fd, errMsgNotFound.c_str(), errMsgNotFound.size(), 0);
            std::cout << "ERR" << std::endl;
        }
        else
        {
            send(client_fd, errOpeningFile.c_str(), errOpeningFile.size(), 0);
            std::cout << "ERR" << std::endl;
        }
    }

    void addToBlacklist(time_t timestamp, const std::string &clientIp)
    {
        std::string blacklist = "blacklist.txt";         // Blacklist file name
        std::ofstream outfile(blacklist, std::ios::app); // Open the file in append mode
        if (outfile.is_open())
        {
            // Store the IP and timestamp of when the client is added to the blacklist
            outfile << clientIp << " " << ctime(&timestamp);
            outfile.close();
            std::cout << "Successfully added Client IP to Blacklist\n"
                      << clientIp << " " << ctime(&timestamp) << "\n";
        }
        else
            std::cout << "Error adding Client to Blacklist\n";
    }

    bool isInBlackList(const std::string &timestampStr, const std::string &clientIp)
    {
        std::string blacklist = "blacklist.txt";
        std::ifstream infile(blacklist);

        if (!infile.is_open())
        {
            std::cout << "Error opening blacklist file\n";
            return false;
        }

        std::string line;

        while (std::getline(infile, line))
        {
            std::istringstream iss(line);
            std::string ip;
            std::string timestampfromFile;

            // Extract the IP and stamp from the blacklist file
            if (iss >> ip)
            {
                std::getline(iss, timestampfromFile);
                timestampfromFile.erase(0, timestampfromFile.find_first_not_of(" \t"));

                if (ip == clientIp)
                {
                    std::cout << "Found client IP: " << line << std::endl;

                    struct std::tm tm = {};
                    std::istringstream ss(timestampfromFile);
                    ss >> std::get_time(&tm, "%a %b %d %H:%M:%S %Y"); // Parse timestamp

                    if (ss.fail())
                    {
                        std::cout << "Failed to parse timestamp\n";
                        continue;
                    }

                    // Convert timestamp to time_t
                    std::time_t fileTimestamp = std::mktime(&tm);
                    if (fileTimestamp == -1)
                    {
                        std::cout << "Failed to convert timestamp to time_t\n";
                        continue;
                    }

                    // Get the current timestamp
                    std::time_t currentTimestamp = std::time(nullptr);

                    // Check if the difference is less than 60 seconds
                    double diff = std::difftime(currentTimestamp, fileTimestamp);
                    if (diff < 60)
                    {
                        std::cout << "Client " << clientIp << " is blacklisted for one minute\n";
                        return true; // Client is still blacklisted
                    }
                    else
                    {
                        std::cout << "Blacklist expired for client: " << clientIp << "\n";
                        deleteEntryFromFile("blacklist.txt", clientIp.c_str());
                        return false; // Blacklist has expired
                    }
                }
            }
        }
        return false; // Return false if no matching line is found
    }

    void deleteEntryFromFile(const char *filePath, const char *ipToRemove)
    {
        FILE *originalFile, *tempFile;
        char line[256];

        originalFile = fopen(filePath, "r");
        if (originalFile == NULL)
        {
            perror("Error while opening file");
            return;
        }

        tempFile = fopen("temp.txt", "w");
        if (tempFile == NULL)
        {
            perror("Error opening temporary file");
            fclose(originalFile);
            return;
        }

        while (fgets(line, sizeof(line), originalFile))
        {
            // Check if the line starts with the IP to remove
            if (strncmp(line, ipToRemove, strlen(ipToRemove)) == 0 && isspace(line[strlen(ipToRemove)]))
            {
                // Skip this line as it matches the IP to remove
                continue;
            }
            fputs(line, tempFile); // Copy other lines to the temp file
        }

        fclose(originalFile);
        fclose(tempFile);

        if (remove(filePath) != 0)
            perror("Error deleting original file");
        else if (rename("temp.txt", filePath) != 0)
            perror("Error renaming the temporary file");
        else
            printf("Entry for '%s' has been removed successfully\n", ipToRemove);
    }

    bool processLogin(int client_fd, const std::string &message, int attempts)
    {
        if (attempts <= 0)
        {
            printf("Client tried to log in to much!\n");
            return false;
        }
        std::string clientIp = getClientIp(client_fd);
        time_t currentTime;
        std::string timestampStr = to_string(time(&currentTime));

        if (isInBlackList(timestampStr, clientIp))
        {
            const char blacklistMessage[] = "Your IP is blacklisted. Please wait a bit to try again\n";
            send(client_fd, blacklistMessage, sizeof(blacklistMessage), 0);
            return false;
        }

        std::stringstream iss(message);
        std::string command, username, password;
        std::getline(iss, command, '\n');
        printf("Command used: %s\n", command.c_str());
        std::getline(iss, username, '\n');
        printf("Username: %s\n", username.c_str());
        std::getline(iss, password);

        const char *ldapUri = "ldap://ldap.technikum-wien.at:389";
        const int ldapVersion = LDAP_VERSION3;
        char ldapBindUser[256];
        char rawLdapUser[128];

        if (username != "")
        {
            strcpy(rawLdapUser, username.c_str());
            sprintf(ldapBindUser, "uid=%s,ou=people,dc=technikum-wien,dc=at", rawLdapUser);
            printf("user set to: %s\n", ldapBindUser);
        }
        else
        {
            const char *rawLdapUserEnv = getenv("ldapuser");
            if (rawLdapUserEnv == NULL)
            {
                printf("user not found... set to empty string\n");
                strcpy(ldapBindUser, "");
            }
            else
            {
                sprintf(ldapBindUser, "uid%s,ou=people,dc=technikum-wien,dc=at", rawLdapUser);
                printf("user based on environment variable ldap user set to: %s\n", ldapBindUser);
            }
        }
        char ldapBindPassword[256];
        strcpy(ldapBindPassword, password.c_str());

        int rc = 0;

        LDAP *ldapHandle;
        rc = ldap_initialize(&ldapHandle, ldapUri);
        if (rc != LDAP_SUCCESS)
        {
            fprintf(stderr, "ldap_init failed\n");
            return false;
        }
        printf("connected to LDAP server %s\n", ldapUri);

        rc = ldap_set_option(ldapHandle, LDAP_OPT_PROTOCOL_VERSION, &ldapVersion);
        if (rc != LDAP_OPT_SUCCESS)
        {
            fprintf(stderr, "ldap_set_option(PROTOCOL_VERSION): %s\n", ldap_err2string(rc));
            ldap_unbind_ext_s(ldapHandle, NULL, NULL);
            return false;
        }

        rc = ldap_start_tls_s(ldapHandle, NULL, NULL);
        if (rc != LDAP_SUCCESS)
        {
            fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
            ldap_unbind_ext_s(ldapHandle, NULL, NULL);
            return false;
        }
        BerValue bindCredentials;
        bindCredentials.bv_val = (char *)ldapBindPassword;
        bindCredentials.bv_len = strlen(ldapBindPassword);
        BerValue *servercredp; // server's credentials
        rc = ldap_sasl_bind_s(
            ldapHandle,
            ldapBindUser,
            LDAP_SASL_SIMPLE,
            &bindCredentials,
            NULL,
            NULL,
            &servercredp);
        if (rc != LDAP_SUCCESS)
        {
            fprintf(stderr, "LDAP bind error: %s\n", ldap_err2string(rc));
            ldap_unbind_ext_s(ldapHandle, NULL, NULL);

            char attemptMessage[256]; // Allocate sufficient space
            attempts--;
            snprintf(attemptMessage, sizeof(attemptMessage) + 2, "Attempts Left: %d\n", attempts);
            send(client_fd, attemptMessage, sizeof(attemptMessage), 0);
            return false;
        }
        else
        {
            std::string okMessage = "Successfully Logged In\n";
            send(client_fd, okMessage.c_str(), okMessage.size(), 0);
            ldap_unbind_ext_s(ldapHandle, NULL, NULL);
            return true;
        }
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        return false;
    }

    void processDelete(int client_fd, const std::string &message)
    { // Process the DELETE command
        std::istringstream iss(message);
        std::string command, username, idStr;
        std::getline(iss, command, '\n');
        std::getline(iss, username, '\n');
        std::getline(iss, idStr, '\n');

        std::string errMsgFormat = "Invalid message ID format\n";
        std::string errOpeningMsg = "Error opening message file\n";
        std::string successMsg = "Message deleted successfully\n";
        std::string errIdNotFound = "Error, message ID not found.\n";

        int messageID; // Message ID to delete
        try
        {
            messageID = std::stoi(idStr); // Convert ID to integer
        }
        catch (const std::invalid_argument &e)
        { // Check if the ID is a number
            send(client_fd, errMsgFormat.c_str(), errMsgFormat.size(), 0);
            cout << "ERR" << endl;
            return;
        }

        std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
        std::ifstream infile(message_filename);

        if (!infile.is_open())
        {
            send(client_fd, errOpeningMsg.c_str(), errOpeningMsg.size(), 0);
            cout << "ERR" << endl;
            return;
        }

        std::vector<std::string> messages; // Vector to store messages
        std::string line;
        std::string current_message;
        int current_id = 0;
        bool foundMessage = false;

        // Read the file and separate messages
        while (std::getline(infile, line))
        {                                   // Read the file line by line
            current_message += line + "\n"; // Add the line to the current message
            if (line == "-----")
            {
                if (current_id != messageID)
                    messages.push_back(current_message); // Keep the message if not the one to delete

                else
                    foundMessage = true; // Mark that we found and are deleting the message
                    
                current_message.clear(); // Reset for the next message
                current_id++;            // Increment the message ID
            }
        }
        infile.close();

        if (foundMessage)
        { // Check if the message was found
            std::ofstream outfile(message_filename, std::ios::trunc);
            if (!outfile.is_open())
            { // Check if the file was opened successfully
                send(client_fd, errOpeningMsg.c_str(), errOpeningMsg.size(), 0);
                cout << "ERR" << endl;
                return;
            }

            // Write back all messages except the deleted one
            for (const std::string &msg : messages)
            {
                outfile << msg; // Write the message to the file
            }
            outfile.close();

            send(client_fd, successMsg.c_str(), successMsg.size(), 0);
            cout << "OK" << endl;
        }
        else
        {
            send(client_fd, errIdNotFound.c_str(), errIdNotFound.size(), 0);
            cout << "ERR" << endl;
        }
    }
};

int main(int argc, char *argv[])
{ // Entry Point
    if (argc != 3)
    {
        std::cerr << "Usage: ./twmailer-server <port> <mail-spool-directoryname>" << std::endl;
        return 1;
    }
    int port = std::stoi(argv[1]);
    std::string spoolDirectory = argv[2];

    MailServer server(port, spoolDirectory);
    server.start();

    return 0;
}
