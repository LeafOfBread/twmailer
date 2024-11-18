all: server client

server: server.cpp
	g++ -std=c++17 -Wall -Werror -o server server.cpp -lldap -llber

client: client.cpp
	g++ -std=c++17 -Wall -Werror -o client client.cpp -lldap -llber

clean:
	rm -f server client
