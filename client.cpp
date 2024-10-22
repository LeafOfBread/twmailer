#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

const int BUFFER_SIZE = 1024;

class MailClient {
public:
    MailClient(const std::string& serverIP, int serverPort) : serverIP(serverIP), serverPort(serverPort) {}

    void start() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            std::cerr << "Socket creation failed!" << std::endl;
            return;
        }

        sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(serverPort);
        inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr);

        if (connect(sock, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            std::cerr << "Connection failed!" << std::endl;
            return;
        }

        std::string command;
        while (true) {
            std::cout << "Enter command | SEND | LIST | READ | DEL | QUIT |";
            std::getline(std::cin, command);

            if (command == "QUIT") {
                send(sock, "QUIT\n", 5, 0);
                break;
            }

            handleCommand(sock, command);
        }

        close(sock);
    }

private:
    std::string serverIP;
    int serverPort;

    void handleCommand(int sock, const std::string& command) {
        if (command == "SEND") {
            sendMessage(sock);
        } else if (command == "LIST") {
            listMessages(sock);
        } else if (command == "READ") {
            readMessage(sock);
        } else if (command == "DEL") {
            deleteMessage(sock);
        } else {
            std::cerr << "Unknown command!" << std::endl;
        }
    }

    void sendMessage(int sock) {
        std::string username, subject, content;
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        std::cout << "Enter subject: ";
        std::getline(std::cin, subject);
        std::cout << "Enter content: ";
        std::getline(std::cin, content);

        std::string message = "SEND\n" + username + "\n" + subject + "\n" + content + "\n";
        send(sock, message.c_str(), message.size(), 0);

        char buffer[BUFFER_SIZE];
        recv(sock, buffer, sizeof(buffer) - 1, 0);
        buffer[BUFFER_SIZE - 1] = '\0';
        std::cout << buffer;
    }

    void listMessages(int sock) {
    std::string username;
    std::cout << "Enter username: ";
    std::getline(std::cin, username);

    std::string message = "LIST\n" + username + "\n";
    send(sock, message.c_str(), message.size(), 0);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    buffer[bytes_received] = '\0';  // Ensure null-termination
    std::cout << buffer;
}

    void readMessage(int sock) {
        std::string username, messageId;
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        std::cout << "Enter message ID: ";
        std::getline(std::cin, messageId);

        std::string message = "READ\n" + username + "\n" + messageId + "\n";
        send(sock, message.c_str(), message.size(), 0);

        char buffer[BUFFER_SIZE];
        recv(sock, buffer, sizeof(buffer) - 1, 0);
        buffer[BUFFER_SIZE - 1] = '\0';
        std::cout << buffer;
    }

    void deleteMessage(int sock) {
        std::string username, messageId;
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        std::cout << "Enter message ID: ";
        std::getline(std::cin, messageId);

        std::string message = "DEL\n" + username + "\n" + messageId + "\n";
        send(sock, message.c_str(), message.size(), 0);

        char buffer[BUFFER_SIZE];
        recv(sock, buffer, sizeof(buffer) - 1, 0);
        buffer[BUFFER_SIZE - 1] = '\0';
        std::cout << buffer;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./twmailer-client <server-ip> <port>" << std::endl;
        return 1;
    }

    std::string serverIP = argv[1];
    int serverPort = std::stoi(argv[2]);

    MailClient client(serverIP, serverPort);
    client.start();

    return 0;
}
