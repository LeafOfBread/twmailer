#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <limits>
#include "getpass.h"

const int BUFFER_SIZE = 2048;

class MailClient
{
public:
    MailClient(const std::string &serverIP, int serverPort) : serverIP(serverIP), serverPort(serverPort) {}
    bool isLoggedIn = false;
    std::string userName = "";

    void start()
    { // Start the client
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
            std::cerr << "Socket creation failed!" << std::endl;
            return;
        }

        sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(serverPort);
        inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr);

        if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
        {
            std::cerr << "Connection failed!" << std::endl;
            return;
        }

        std::string command;
        while (true)
        { // Main loop
            if (!isLoggedIn)
                std::cout << "Enter command | LOGIN | QUIT |";
            else
                std::cout << "Enter command | SEND | LIST | READ | DEL | QUIT |";
            
            std::getline(std::cin, command);

            if (command == "QUIT")
            {
                send(sock, command.c_str(), command.size(), 0);
                break;
            }

            handleCommand(sock, command);
        }

        close(sock);
    }

private:
    std::string serverIP;
    int serverPort;

    void handleCommand(int sock, const std::string &command)
    { // Handle the user's command
        if (!isLoggedIn)
        {
            if (command == "LOGIN")
                userLogin(sock);
            else if (command == "QUIT")
                exit(0);
            else
                std::cerr << "User isn't logged in!\n";
        }
        else
        {
            if (command == "SEND")
            {
                sendMessage(sock);
            }
            else if (command == "LIST")
            {
                listMessages(sock);
            }
            else if (command == "READ")
            {
                readMessage(sock);
            }
            else if (command == "DEL")
            {
                deleteMessage(sock);
            }
            else
            {
                std::cerr << "Unknown command!" << std::endl;
            }
        }
    }

    void sendMessage(int sock)
    { // Send a message to the server
        std::string receiver, subject, content, line;
        std::cout << "Sender: " << this->userName << std::endl;
        std::cout << "Enter receiver: ";
        std::getline(std::cin, receiver);
        std::cout << "Enter subject: ";
        std::getline(std::cin, subject);

        std::cout << "Enter content (end with a single '.' on a new line):" << std::endl; // lese content line-by-line
        while (true)
        { // lese content bis "." vorkommt
            std::getline(std::cin, line);
            if (line == ".")
            {
                break;
            }
            content += line + "\n"; // Append new line an content
        }

        // contatenate message fuer server
        std::string message = "SEND\n" + this->userName + "\n" + receiver + "\n" + subject + "\n" + content + "\n";
        send(sock, message.c_str(), message.size(), 0);
        std::cout << message << std::endl;

        // erhalte response
        char buffer[BUFFER_SIZE]; // response wird im buffer gespeichert
        memset(buffer, 0, sizeof(buffer));  //buffer wird zur sicherheit nochmal gecleart um extra daten zu vermeiden
        ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';   //null terminate!
            std::cout << buffer << std::endl;
        }
        else
        {
            std::cerr << "Error receiving the message from the server\n";
        }
    }

    void listMessages(int sock)
    { // List all messages for a user
        std::string message = "LIST\n" + this->userName + "\n";
        send(sock, message.c_str(), message.size(), 0);

        char buffer[BUFFER_SIZE];
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        buffer[bytes_received] = '\0'; // null terminate
        std::cout << buffer;
    }

    void readMessage(int sock)
    { // Read a message
        std::string messageId;
        std::cout << "Enter message ID: ";
        std::getline(std::cin, messageId);

        std::string message = "READ\n" + this->userName + "\n" + messageId + "\n";
        send(sock, message.c_str(), message.size(), 0);

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';
            std::cout << buffer << "\n";
        }
        else
        {
            std::cerr << "Error receiving message from server\n";
        }
    }

    void deleteMessage(int sock)
    { // Delete a message
        std::string messageId;
        std::cout << "Enter message ID: ";
        std::getline(std::cin, messageId);

        std::string message = "DEL\n" + this->userName + "\n" + messageId + "\n";
        send(sock, message.c_str(), message.size(), 0);

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';
            std::cout << buffer << "\n";
        }
        else
        {
            std::cerr << "Error deleting message\n";
        }
    }

    void userLogin(int sock)
    { // log in the user via ldap
        std::string username;
        std::cout << "Enter Username: ";
        std::getline(std::cin, username);
        char password[256];
        strcpy(password, getpass());
        std::string message = "LOGIN\n" + username + "\n" + password + "\n";
        send(sock, message.c_str(), message.size(), 0);

        char buffer[BUFFER_SIZE];
        recv(sock, buffer, sizeof(buffer) - 1, 0);
        buffer[BUFFER_SIZE - 1] = '\0';
        std::cout << buffer;

        std::string response(buffer);
        if (response == "Successfully Logged In\n")
        {
            isLoggedIn = true;
            this->userName = username;
        }
        else
        {
            isLoggedIn = false;
        }
    }
};

int main(int argc, char *argv[])
{ // Entry Point
    if (argc != 3)
    {
        std::cerr << "Usage: ./twmailer-client <server-ip> <port>" << std::endl;
        return 1;
    }

    std::string serverIP = argv[1];
    int serverPort = std::stoi(argv[2]);

    MailClient client(serverIP, serverPort);
    client.start(); // Start the client

    return 0;
}
