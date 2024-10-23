#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdio.h>

using namespace std;

const int BUFFER_SIZE = 2048;

struct Message {
    std::string sender;
    std::string receiver;
    std::string subject;
    std::string content;
};

class MailServer {
public:
    // Constructor
    MailServer(int port, const std::string& spoolDirectory) : port(port), spoolDirectory(spoolDirectory) {
        if (!std::filesystem::exists(spoolDirectory)) {
            std::filesystem::create_directory(spoolDirectory);
        }
    }
    // Start the server
    void start() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            std::cerr << "Socket creation failed!" << std::endl;
            return;
        }
        sockaddr_in server_address;
        server_address.sin_family = AF_INET; // IPv4
        server_address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
        server_address.sin_port = htons(port);  // Convert port to network byte order

        if (bind(server_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {   // Bind the socket to the address and port
            std::cerr << "Binding failed!" << std::endl;    
            return;
        }

        listen(server_fd, 3);   // Listen for incoming connections
        std::cout << "Server listening on port " << port << std::endl;  // Print the port number

        while (true) {  // Accept incoming connections
            sockaddr_in client_address;
            socklen_t addr_len = sizeof(client_address);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_address, &addr_len);
            if (client_fd < 0) {    // Check if the connection was successful
                std::cerr << "Failed to accept client connection!" << std::endl;
                continue;
            }

            handleClient(client_fd);    // Handle the client connection
            close(client_fd);   // Close the client connection
        }

        close(server_fd);   // Close the server socket
    }

private:
    int port;
    std::string spoolDirectory;     

    void handleClient(int client_fd) {  // Handle the client connection
        char buffer[BUFFER_SIZE];           // Buffer to store received data
        std::string message;

        while (true) {  // Receive messages from the client
            ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);    // Receive data from the client
            if (bytes_received <= 0) {  // Check if the connection was closed or an error occurred
                std::cerr << "Client disconnected or error occurred!" << std::endl;
                break;
            }

            buffer[bytes_received] = '\0';  // Null-terminate the received data
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

    void processSend(int client_fd, const std::string& message) {   // Process the SEND command
        std::istringstream iss(message);
        std::string command, username, receiver, subject, content;  
        std::getline(iss, command, '\n');   
        std::getline(iss, username, '\n');
        std::getline(iss, receiver, '\n');
        std::getline(iss, subject, '\n');
        std::getline(iss, content, '\n');

        std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";   // Create the message filename
        std::ofstream outfile(message_filename, std::ios::app); // Open the file in append mode
        if (outfile.is_open()) {    // Check if the file was opened successfully
            outfile << "Sender: " << username << "\n"
                    << "Receiver: " << receiver << "\n"
                    << "Subject: " << subject << "\n"
                    << "Content: " << content << "\n"
                    << "-----\n";
            outfile.close();
            send(client_fd, "Message sent successfully.\n", 28, 0); 
            cout << "OK" << endl;
        } else {    // Error saving the message
            send(client_fd, "Error saving message.\n", 22, 0);
            cout << "ERR" << endl;
        }
    }

    void processList(int client_fd, const std::string& message) {   // Process the LIST command
    std::istringstream iss(message);
    std::string command, username;
    std::getline(iss, command, '\n');
    std::getline(iss, username, '\n');

    std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
    std::ifstream infile(message_filename);
    if (infile.is_open()) { // Check if the file was opened successfully
        std::string line;
        std::string response;
        int id = 0;
        bool foundMessage = false;

        std::string sender, receiver, subject, content;

        // Read lines from the file
        while (std::getline(infile, line)) {    // Read the file line by line
            if (line.substr(0, 7) == "Sender:") {
                foundMessage = true;
                sender = line;  // Read sender line
                std::getline(infile,receiver);  // Read receiver line
                std::getline(infile, subject);  // Read subject line
                std::getline(infile, content);  // Read content line
                response += "ID: " + std::to_string(id) + "\n" + sender + "\n" + receiver + "\n" + subject + "\n";
                id++;
                // Skip separator line ("-----")
                std::getline(infile, line);
            }
        }

        infile.close();

        if (foundMessage) { // Check if messages were found
            send(client_fd, response.c_str(), response.size(), 0);
            cout << "OK" << endl;
        } else {    // No messages found
            send(client_fd, "No messages found.\n", 19, 0);
            cout << "ERR" << endl;
        }
    } else {    // Error opening the file
        send(client_fd, "No messages found.\n", 19, 0);
        cout << "ERR" << endl;
    }
}


std::string trim(const std::string& str) {  // Trim whitespace from the string
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return ""; // no content
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

    void processRead(int client_fd, const std::string& message) {   // Process the READ command
    std::istringstream iss(message);
    std::string command, username, messageIdStr;
    std::getline(iss, command, '\n');
    std::getline(iss, username, '\n');
    std::getline(iss, messageIdStr, '\n');

    // Trim messageIdStr to remove whitespace
    messageIdStr = trim(messageIdStr);  // Trim whitespace from the message ID
    int messageID;

    // Validate if messageIdStr is a number before proceeding
    try {
        messageID = std::stoi(messageIdStr);    // Convert the message ID to an integer
    } catch (std::invalid_argument&) {
        send(client_fd, "Invalid message ID format.\n", 26, 0);   // Send an error message
        cout << "ERR" << endl;
        return;
    }

    std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
    std::ifstream infile(message_filename); 
    
    if (infile.is_open()) {
        std::string line;
        std::string response;
        int currentId = 0;

        // Read lines from the file
        while (std::getline(infile, line)) {    // Read the file line by line
            // Check for the start of a message
            if (line.substr(0, 7) == "Sender:") {
                if (currentId == messageID) {
                    response += line + "\n";  // Sender
                    std::getline(infile, line); // Receiver
                    response += line + "\n";  // Receiver
                    std::getline(infile, line); // Subject
                    response += line + "\n";  // Subject
                    std::getline(infile, line); // Content
                    response += line + "\n";  // Content
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
        send(client_fd, "Message ID not found.\n", 22, 0);
        std::cout << "ERR" << std::endl;
    } else {    
        send(client_fd, "Error opening message file.\n", 28, 0);
        std::cout << "ERR" << std::endl;
    }
}




void processDelete(int client_fd, const std::string& message) { // Process the DELETE command
    std::istringstream iss(message);
    std::string command, username, idStr;
    std::getline(iss, command, '\n');
    std::getline(iss, username, '\n');
    std::getline(iss, idStr, '\n');

    int messageID;  // Message ID to delete
    try {
        messageID = std::stoi(idStr); // Convert ID to integer
    } catch (const std::invalid_argument& e) {  // Check if the ID is a number
        send(client_fd, "Invalid message ID format.\n", 27, 0); 
        cout << "ERR" << endl;
        return;
    }

    std::string message_filename = spoolDirectory + "/" + username + "_messages.txt";
    std::ifstream infile(message_filename);
    
    if (!infile.is_open()) {
        send(client_fd, "Error opening message file.\n", 28, 0);
        cout << "ERR" << endl;
        return;
    }

    std::vector<std::string> messages;  // Vector to store messages
    std::string line;
    std::string current_message;
    int current_id = 0;
    bool foundMessage = false;

    // Read the file and separate messages
    while (std::getline(infile, line)) {    // Read the file line by line
        current_message += line + "\n"; // Add the line to the current message
        if (line == "-----") {
            if (current_id != messageID) {
                messages.push_back(current_message);  // Keep the message if not the one to delete
            } else {
                foundMessage = true;  // Mark that we found and are deleting the message
            }
            current_message.clear();  // Reset for the next message
            current_id++;  // Increment the message ID
        }
    }
    infile.close();

    if (foundMessage) { // Check if the message was found
        std::ofstream outfile(message_filename, std::ios::trunc);
        if (!outfile.is_open()) {   // Check if the file was opened successfully
            send(client_fd, "Error updating message file.\n", 29, 0);
            cout << "ERR" << endl;
            return;
        }

        // Write back all messages except the deleted one
        for (const std::string& msg : messages) {
            outfile << msg; // Write the message to the file
        }
        outfile.close();

        send(client_fd, "Message deleted successfully.\n", 31, 0);  
        cout << "OK" << endl;
    } else {
        send(client_fd, "Message ID not found.\n", 23, 0);
        cout << "ERR" << endl;
    }
}

};

int main(int argc, char* argv[]) {  //Entry Point
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
