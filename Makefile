test: client.o server.o
	g++ client.o -o client
	g++ server.o -o server
server.o:
	g++ server.cpp -c
client.o:
	g++ client.cpp -c
clean:
	rm -rf client.o server.o client server
