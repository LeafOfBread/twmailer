#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

const int BUFFER_SIZE = 1024;

struct Message {
    std::string sender;
    std::string subject;
    std::string content;
};

class MailServer {
public:
    MailServer(int port, const std::string& spoolDirectory) : port(port), spoolDirectory(spoolDirectory) {
        if (!std::filesystem::exists(spoolDirectory)) {
            std::filesystem::create_directory(spoolDirectory);
        }
    }

    void start() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            std::cerr << "Socket creation failed!" << std::endl;
            return;
        }

        sockaddr_in server_address;
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
            std::cerr << "Binding failed!" << std::endl;
            return;
        }

        listen(server_fd, 3);
        std::cout << "Server listening on port " << port << std::endl;

        while (true) {
            sockaddr_in client_address;
            socklen_t addr_len = sizeof(client_address);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_address, &addr_len);
            if (client_fd < 0) {
                std::cerr << "Failed to accept client connection!" << std::endl;
                continue;
            }

            handleClient(client_fd);
            close(client_fd);
        }

        close(server_fd);
    }

private:
    int port;
    std::string spoolDirectory;

    void handleClient(int client_fd) {
        char buffer[BUFFER_SIZE];
        std::string message;

        while (true) {
            ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                std::cerr << "Client disconnected or error occurred!" << std::endl;
                break;
            }

            buffer[bytes_received] = '\0';
            message = buffer;

            if (message.substr(0, 4) == "SEND") {
                processSend(client_fd, message);
            } else if (message.substr(0, 4) == "LIST") {
                processList(client_fd, message);
            } else if (message.substr(0, 4) == "READ") {
                processRead(client_fd, message);
            } else if (message.substr(0, 3) == "DEL") {
                processDelete(client_fd, message);
            } else if (message.substr(0, 4) == "QUIT") {
                break;
            }
        }
    }

    void processSend(int client_fd, const std::string& message) {
        std::istringstream iss(message);
        std::string command, username, subject, content;
        std::getline(iss, command, '\n');
        std::getline(iss, username, '\n');
        std::getline(iss, subject, '\n');
        std::getline(iss, content, '\n');

        std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
        std::ofstream outfile(message_filename, std::ios::app);
        if (outfile.is_open()) {
            outfile << "Sender: " << username << "\n"
                    << "Subject: " << subject << "\n"
                    << "Content: " << content << "\n"
                    << "-----\n";
            outfile.close();
            send(client_fd, "Message sent successfully.\n", 28, 0);
        } else {
            send(client_fd, "Error saving message.\n", 22, 0);
        }
    }

    void processList(int client_fd, const std::string& message) {
    std::istringstream iss(message);
    std::string command, username;
    std::getline(iss, command, '\n');
    std::getline(iss, username, '\n');

    std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
    std::ifstream infile(message_filename);
    if (infile.is_open()) {
        std::string line;
        std::string response;
        int id = 0;
        bool foundMessage = false;

        // Read lines from the file
        while (std::getline(infile, line)) {
            if (line.substr(0, 8) == "Subject: ") {
                foundMessage = true;
                response += "ID: " + std::to_string(id) + "\n" + line + "\n";
                id++;
            }
            // Skip the content line to avoid reading it in the response
            std::getline(infile, line); // Skip the content line
        }

        infile.close();

        if (foundMessage) {
            send(client_fd, response.c_str(), response.size(), 0);
        } else {
            send(client_fd, "No messages found.\n", 19, 0);
        }
    } else {
        send(client_fd, "No messages found.\n", 19, 0);
    }
}


    void processRead(int client_fd, const std::string& message) {
        std::istringstream iss(message);
        std::string command, username, messageId;
        std::getline(iss, command, '\n');
        std::getline(iss, username, '\n');
        std::getline(iss, messageId, '\n');

        std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
        std::ifstream infile(message_filename);
        if (infile.is_open()) {
            std::string line;
            int id = std::stoi(messageId);
            int currentId = 0;
            std::string response;
            while (std::getline(infile, line)) {
                if (line.substr(0, 8) == "Sender: ") {
                    if (currentId == id) {
                        response += line + "\n";  // Sender
                        std::getline(infile, line); // Subject
                        response += line + "\n";  // Subject
                        std::getline(infile, line); // Content
                        response += line + "\n";  // Content
                        response += "-----\n";  // Separator
                        send(client_fd, response.c_str(), response.size(), 0);
                        return;
                    }
                    currentId++;
                    std::getline(infile, line); // Skip subject
                    std::getline(infile, line); // Skip content
                }
            }
            infile.close();
            send(client_fd, "Message not found.\n", 20, 0);
        } else {
            send(client_fd, "Message not found.\n", 20, 0);
        }
    }

    void processDelete(int client_fd, const std::string& message) {
    std::istringstream iss(message);
    std::string command, username, messageId;
    std::getline(iss, command, '\n');
    std::getline(iss, username, '\n');
    std::getline(iss, messageId, '\n');

    std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
    std::ifstream infile(message_filename);
    std::vector<std::string> messages;
    std::string line;

    if (infile.is_open()) {
        while (std::getline(infile, line)) {
            messages.push_back(line);
            // Skip the next lines (subject and content)
            std::getline(infile, line);
            std::getline(infile, line);
        }
        infile.close();
    } else {
        send(client_fd, "Message file not found.\n", 24, 0);
        return;
    }

    // Change id to size_t and check bounds
    std::size_t id;
    try {
        id = std::stoi(messageId);
    } catch (...) {
        send(client_fd, "Invalid message ID format.\n", 28, 0);
        return;
    }

    if (id >= messages.size() / 3) {
        send(client_fd, "Invalid message ID.\n", 20, 0);
        return;
    }

    // Calculate the starting index for the message to be deleted
    std::size_t start_index = id * 3; // 3 lines per message (sender, subject, content)
    messages.erase(messages.begin() + start_index, messages.begin() + start_index + 3); // Remove the message and its subject and content

    std::ofstream outfile(message_filename);
    if (outfile.is_open()) {
        for (const auto& msg : messages) {
            outfile << msg << "\n"; // Write the remaining messages back to the file
        }
        outfile.close();
        send(client_fd, "Message deleted successfully.\n", 30, 0);
    } else {
        send(client_fd, "Error saving changes.\n", 22, 0);
    }
}

};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./twmailer-server <port> <mail-spool-directoryname>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string spoolDirectory = argv[2];

    MailServer server(port, spoolDirectory);
    server.start();

    return 0;
}
