#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <cmath>

const int _blockSIZE = 1024;
const int MAX_HOST = 1024;  // Hostname buffer size
const int MAX_SERV = 32;    // Service name buffer size

// Struct for TextPreset data
struct TextPreset {
    int packageNUM = 1;
    std::string delim = "\n";
    int length = 0;
    int type = 0;
    std::string argument, sender, subject, text, username, infoString;
    bool error = false;
    int ID = 0;
};

// Enum to define operation types
enum Type {
    SEND = 1,
    READ = 2,
    LIST = 3,
    DEL = 4,
    QUIT = 5,
    COMMENT = 0,
};

// Function to reset TextPreset struct
TextPreset resetTP() {
    return TextPreset{};
}

// Function to parse information
TextPreset parseINFO(TextPreset tp, const std::string& info) {
    std::string parseTemp;
    int j = 0;

    for (char c : info) {
        if (c == '\n') {
            switch (j++) {
                case 0: tp.type = std::stoi(parseTemp); break;
                case 1: tp.packageNUM = std::stoi(parseTemp); break;
                case 2: tp.length = std::stoi(parseTemp); break;
            }
            parseTemp.clear();
        } else {
            parseTemp += c;
        }
    }
    return tp;
}

// Function to parse SEND messages
TextPreset parseSEND(TextPreset tp, const std::string& sendMESS) {
    std::string parseTemp;
    int j = 0;

    for (char c : sendMESS) {
        if (c == '\n') {
            switch (j++) {
                case 0: tp.argument = parseTemp; break;
                case 1: tp.sender = parseTemp; break;
                case 2: tp.subject = parseTemp; break;
                default: tp.text += parseTemp; break;
            }
            parseTemp.clear();
            j++;
        } else {
            parseTemp += c;
        }
    }
    return tp;
}

// Helper function to handle received data and save messages
void initializeSENDSAVE(TextPreset tp, int clientSocket, std::vector<TextPreset>& n) {
    char buffer[_blockSIZE] = {0};
    int errRcv = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (errRcv == -1) {
        std::cerr << "Error in initialize single pack" << std::endl;
        return;
    }

    buffer[errRcv] = '\n';
    tp = parseSEND(tp, std::string(buffer));
    n.push_back(tp);
}

// Helper function for handling message packages
void initializeSENDSAVE_Packages(TextPreset tp, int clientSocket, std::vector<TextPreset>& n) {
    std::string completeMessage;
    int currentPackage = 1;

    while (currentPackage <= tp.packageNUM) {
        char buffer[_blockSIZE];
        int errRcv = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (errRcv == -1) {
            std::cerr << "Error receiving package " << currentPackage << std::endl;
            return;
        }
        completeMessage.append(buffer, errRcv);
        ++currentPackage;
    }

    tp = parseSEND(tp, completeMessage);
    n.push_back(tp);
}

// Function to handle the LIST functionality
void LISTsendFunct(int clientSocket, const std::vector<TextPreset>& n, const TextPreset& tp) {
    std::string LISTstring;
    bool sendUSERLIST = false;

    for (size_t i = 0; i < n.size(); ++i) {
        if (n[i].sender == tp.sender) {
            LISTstring += std::to_string(i) + "\n" + n[i].sender + "\n" + n[i].subject + "\n" + n[i].text.substr(0, 10) + "\n";
            sendUSERLIST = true;
        }
    }

    if (LISTstring.size() > _blockSIZE - 1) {
        std::cerr << "LIST too large to send" << std::endl;
    }

    if (!sendUSERLIST) {
        send(clientSocket, "ERR\n", sizeof("ERR\n"), 0);
    } else {
        send(clientSocket, LISTstring.c_str(), LISTstring.size(), 0);
    }
}

// Helper function to calculate and package info string
TextPreset calcINFOstring(TextPreset tp, int type) {
    tp.type = type;
    std::string tempString;

    if (type == SEND) {
        tempString = tp.argument + "\n" + tp.sender + "\n" + tp.subject + "\n" + tp.text + "\n";
        tp.length = tempString.size();
    } else if (type == READ) {
        tempString = tp.username + "\n" + std::to_string(tp.ID) + "\n";
        tp.length = tempString.size();
    }

    tp.packageNUM = std::ceil(static_cast<double>(tp.length) / _blockSIZE);
    tp.infoString = std::to_string(tp.type) + "\n" + std::to_string(tp.packageNUM) + "\n" + std::to_string(tp.length) + "\n";

    return tp;
}

// Function to send message string in packages
int sendMESSstring_Packages(int clientSocket, const TextPreset& tp) {
    std::string SENDstring = tp.argument + "\n" + tp.sender + "\n" + tp.subject + "\n" + tp.text + "\n";
    int totalLength = SENDstring.size();
    int sentBytes = 0;

    for (int i = 0; i < tp.packageNUM; ++i) {
        int remainingBytes = totalLength - sentBytes;
        int currentBlockSize = std::min(_blockSIZE - 1, remainingBytes);
        std::string currentBlock = SENDstring.substr(sentBytes, currentBlockSize) + '\n';

        int bytesSent = send(clientSocket, currentBlock.c_str(), currentBlock.size(), 0);
        if (bytesSent == -1) {
            std::cerr << "Error sending package " << i + 1 << std::endl;
            return -1;
        }
        sentBytes += currentBlockSize;
    }

    return 0;
}

// Function to handle receiving and processing client messages
int recvFromClient(int clientSocket, std::vector<TextPreset>& n) {
    TextPreset tpRECV = resetTP();
    char buffer[_blockSIZE] = {0};
    int errRCV = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (errRCV == -1) {
        std::cerr << "Error in recvFromClient" << std::endl;
        send(clientSocket, "ERR\n", sizeof("ERR\n"), 0);
        return -1;
    }

    buffer[errRCV] = '\n';
    tpRECV = parseINFO(tpRECV, std::string(buffer));

    switch (tpRECV.type) {
        case SEND:
            send(clientSocket, "OK\n", sizeof("OK\n"), 0);
            (tpRECV.packageNUM == 1) ? initializeSENDSAVE(tpRECV, clientSocket, n) : initializeSENDSAVE_Packages(tpRECV, clientSocket, n);
            return SEND;
        case READ:
            if (n.empty()) {
                send(clientSocket, "ERR\n", sizeof("ERR\n"), 0);
            } else {
                send(clientSocket, "OK\n", sizeof("OK\n"), 0);
                // Call function for handling READ
            }
            return READ;
        case LIST:
            if (n.empty()) {
                send(clientSocket, "ERR\n", sizeof("ERR\n"), 0);
            } else {
                send(clientSocket, "OK\n", sizeof("OK\n"), 0);
                LISTsendFunct(clientSocket, n, tpRECV);
            }
            return LIST;
        case DEL:
            if (!n.empty()) {
                send(clientSocket, "OK\n", sizeof("OK\n"), 0);
                // Handle deletion process
            } else {
                send(clientSocket, "ERR\n", sizeof("ERR\n"), 0);
            }
            return DEL;
        case QUIT:
            send(clientSocket, "OK\n", sizeof("OK\n"), 0);
            return QUIT;
        default:
            std::cout << "CLIENT: " << buffer << std::endl;
    }

    return 0;
}

// Main function to handle the server and client connections
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./server <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[2]);
    std::vector<TextPreset> savedMSG;

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Error binding socket" << std::endl;
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == -1) {
        std::cerr << "Error listening on socket" << std::endl;
        return 1;
    }

    sockaddr_in clientAddress{};
    socklen_t clientSize = sizeof(clientAddress);
    char host[MAX_HOST];
    char service[MAX_SERV];

    while (true) {
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientSize);
        if (clientSocket == -1) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        memset(host, 0, MAX_HOST);
        memset(service, 0, MAX_SERV);

        recvFromClient(clientSocket, savedMSG);
        close(clientSocket);
    }

    close(serverSocket);
    return 0;
}