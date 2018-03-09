#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <cerrno>
#include <fcntl.h>
#include <sstream>


using namespace std;

#define MAXLINE 	1000
#define MAXBUFFER	600000
#define LISTENQ		100
#define MAXCLI		3000
#define QSIZE		3000
#define BLANK ' ' || '\t'
//#pragma GCC optimize("O3")

int TOTALCLI = 0;

struct user
{
	string name;
	string addr;
	string port;
	int fd;
	int isFirst;
	int saveFilenameNum;
	vector<string> fileName;
	user(string a, string p){
		name = "";
		addr = a;
		port = p;
		isFirst = 1;
	}
	user()
	{
	}
};

struct user client[MAXCLI];

class unit{
	public:
		int fd;
		int size;
		string fileName;
		char sizeToSend[MAXLINE];
		char *buffer;
		unit()
		{
			fd = -1;
		}
		void buildBuffer(){
			buffer = new char[size];
		}

};

int QNUM = 0;
unit waitingQueue[QSIZE];
int saveNum = 0;


fd_set readSet, allSet, writeSet;

void skipblank(char *str){
	int i = 0;
	while(str[i++] == ' ');
	i--;
	int j = 0;
	while((j + i) < strlen(str)){
		str[j] = str[j + i];
		j++;
	}
	str[j] = '\0';
}

void skipblank(string &str){
	int i = 0;
	while(str[i++] == ' ');
	i--;
	int j = 0;
	while((j + i) < str.size()){
		str[j] = str[j + i];
		j++;
	}
	str[j] = '\0';
}

void tabToBlank(char *str){
	for(int i=0;i<strlen(str);i++){
		if(str[i] == '\t')
			str[i] = ' ';
	}
}

int getIndex(const string msg, struct user *arr){
	char addr[20];
	char port[20];
	int i = 0;
	while(msg[i]!=' '){
		addr[i] = msg[i];
		i++;
	}
	i++;	// so i will not be ' '
	int j = 0;
	while(msg[j + i]!=','){
		port[j] = msg[j + i];
		j++;
	}
	addr[i-1] = '\0';
	port[j] = '\0';
	for(i=0;i<MAXCLI;i++){
		if(arr[i].addr == string(addr) && arr[i].port == string(port))
			return i;
	}
	return -1;
}

string getCommand(const string &str, int &nextIndex){
	int i = 0;
	string command = "";
	while(str[i] != ' '){
		command += str[i++];
	}
	command += "\0";
	nextIndex = i + 1;
	return command;
}

string getFileName(const string &str, int &nextIndex){
	int i = nextIndex;
	string name = "";
	while(str[i] != ' '){
		name += str[i++];
	}
	name += "\0";
	nextIndex = i + 1;
	return name;
}

int getSize(const string &str, int nextIndex){
	char sizeChar[str.size()-nextIndex+1];
	for(int i=nextIndex;i<str.size();i++){
		sizeChar[i - nextIndex] = str[i];
	}
	sizeChar[str.size()] = '\0';
	cout << "size fuck: " << string(sizeChar) << endl;
	cout << "akjsflkasjnhldka:  " << str.size() << endl;
	return atoi(sizeChar);
}

void dealString(string msg){
	int index = getIndex(msg, client);
	int sockfd = client[index].fd;
	char recvline[msg.size()];
	int i=0;
	while(msg[i++]!=',');
	//	ignore addr and port
	int j;
	for(j=i;j<msg.size();j++)
		recvline[j-i] = msg[j];
	recvline[j-i] = '\0';


	tabToBlank(recvline);
	skipblank(recvline);

	int nextIndex;
	string command = getCommand(string(recvline), nextIndex);

	if(command == "put\0"){

		string fileName = getFileName(string(recvline), nextIndex);
		int fileSize = getSize(string(recvline), nextIndex);

		cout << "Command: " << command << endl;
		cout << "File name: " << fileName << endl;
		cout << "size: " << fileSize << endl;
		
		//  ***********************
		//       Write to File
		//  ***********************

		int n;
		char *buffData;
		// allocate memory to contain the whole file:
		buffData = (char*) malloc (sizeof(char)*fileSize);
		if (buffData == NULL) {fputs ("Memory error",stderr); exit (2);}

		
		// Open file for both writing
		stringstream ss;
		ss << client[index].saveFilenameNum;
		string str = ss.str();
		FILE *pFile;
		string RealName = str + fileName;
		pFile = fopen(RealName.c_str(), "w+");

		int total = 0;
		while(1){
			if((n = read(sockfd, buffData, MAXLINE)) > 0){
				cout << "Get " << n << " datas...\n";
				if(n > fileSize){
					fwrite(buffData, 1, fileSize, pFile);
					break;	
				}
			    fwrite(buffData, 1, n, pFile);

			    total += n;
			    cout << "total: " << total << endl;
			    if(total == fileSize)
			    	break;
			}			
		}

    	fclose(pFile);
		client[index].fileName.push_back(fileName);
		cout << "Server get file done...\n";


		//	*********************************
		//		save & send to other user
		//	*********************************

		for(int i=0;i<MAXCLI;i++){
			if(client[i].name == client[index].name && i != index){
				cout << "AAAAAAAAAAAAAAAAAAAAAAA\n";
				client[i].fileName.push_back(fileName);

				int qIndex;
				if(client[i].fd != -2){
					int isError = 0;

					//	************************
					//		Send File Name
					//	************************

					string msg = fileName;
					cout << "send file name: " << msg << endl;
					char MSG[MAXLINE] = {};
					int k;
					for(k=0;k<msg.size();k++)
						MSG[k] = msg[k];
					MSG[k] = '\0';

					if(( n = send(client[i].fd, MSG, MAXLINE, 0)) > 0 ) {
					   	cout << "send n: " << n;
					}
					else if(n == 0){
						cout << "edededededaaaaaaaaasas\n";
					   	if(errno == EWOULDBLOCK){
					   		cout << "One\n";
					   		isError = 1;
					   		for(int jj=0;jj<QSIZE;jj++){
					   			if(waitingQueue[jj].fd == -1){
					   				qIndex = jj;
					   				break;
					   			}
					   		}
					   		waitingQueue[qIndex].fileName = msg;
					 	}
					}
					else{
						cout << "error...\n";
						exit(0);
					}
					
					//  *****************************************************
				    //                      Read From File
				    //  *****************************************************

			      	size_t result;
				    FILE *pFile;
					char * buffer;

					// Open file for both reading
					stringstream ss;
					ss << client[i].saveFilenameNum;
					string str = ss.str();
					string RealName = str + fileName;
					pFile = fopen(RealName.c_str(), "r+");
					//if (pFile==NULL) {fputs ("File error",stderr); exit (1);}

					// allocate memory to contain the whole file:
					buffer = (char*) malloc (sizeof(char)*fileSize);
					if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}
					
					// copy the file into the buffer:
					result = fread (buffer,1,fileSize,pFile);
					if (result != fileSize) {fputs ("Reading error",stderr); exit (3);}
					
					/* the whole file is now loaded in the memory buffer. */
	  			  	// terminate
					fclose (pFile);

					//	************************
					//		Send File Size
					//	************************

					int num = fileSize;
					char sizeTemp[MAXLINE];
					sprintf(sizeTemp, "%d\n", num);

					if(isError){
						cout << "Two\n";
						strcpy(waitingQueue[qIndex].sizeToSend, sizeTemp);
					}
					else{
						string temp = string(sizeTemp);
						
						char MSG2[MAXLINE] = {};
						int k;
						for(k=0;k<temp.size();k++)
							MSG2[k] = temp[k];
						MSG2[k] = '\0';

						if(( n = send(client[i].fd, MSG2, MAXLINE, 0)) > 0 ) {
					       	cout << "send n: " << n << endl;
					    }
					}

					//	************************
					//		Send File Data
					//	************************
					
					if(isError){
						cout << "errno: EWOULDBLOCK...\n";
					   	waitingQueue[qIndex].fd = client[i].fd;
					   	waitingQueue[qIndex].size = fileSize;
					   	waitingQueue[qIndex].buildBuffer();
					   	for(int jj=0;jj<fileSize;jj++){
					   		waitingQueue[qIndex].buffer[jj] = buffer[jj];	
					   	}
					   	cout << "qIndex = " << qIndex << endl;
					   	cout << "fd: " << client[i].fd << endl;
					   	cout << "-------------------------------------------\n";
					   	QNUM++;
					}
					else{
						cout << "send file data, fileSize: " << fileSize << endl;
						int sendn = 0;
						while(1){
							char TEMP[MAXLINE] = {};
							if(fileSize-sendn < MAXLINE){
								int fuck;
								for(fuck=0;fuck<fileSize-sendn;fuck++)
									TEMP[fuck] = buffer[fuck + sendn];
								TEMP[fuck] = '\0';
							}
							else{
								for(int fuck=0;fuck<MAXLINE;fuck++)
									TEMP[fuck] = buffer[fuck + sendn];	
							}
							
							if(( n = write(client[i].fd, TEMP, MAXLINE)) > 0 ) {	
					      		sendn += n;
					      		cout << "sendn: " << sendn << endl;
					      		if(sendn >= fileSize)
					      			break;
					      	}	
						}
						cout << "Done a file again...\n";
					}
				}
			}
		}
		cout << "Send to other users done...\n";
	}

}

void dealName(string msg){
	int index = getIndex(msg, client);
	int sockfd = client[index].fd;
	char name[msg.size()];
	int i=0;
	while(msg[i++]!=',');
	//	ignore addr and port
	int j;
	for(j=i;j<msg.size();j++)
		name[j-i] = msg[j];
	name[j-i] = '\0';


	tabToBlank(name);
	skipblank(name);
	cout << "\n===================================\n\n";
	cout << "Name is: " << string(name) << endl;

	//	***********************************
	//			Update Save Filename
	//	***********************************

	int fay;
	for(fay=0;fay<MAXCLI;fay++){

		//	*********************
		//		If Old User
		//	*********************

		if(client[fay].name == string(name) && index != fay){
			client[index].saveFilenameNum = client[fay].saveFilenameNum;
			cout << "Save Num: " << client[index].saveFilenameNum << endl;
			break;
		}
	}
	if(fay == MAXCLI){
		//	*******************
		//		New User
		//	*******************
		saveNum++;
		cout << "New Save Num: " << saveNum << endl;
		client[index].saveFilenameNum = saveNum;
	}


	//	***********************************
	//			Send old file to user
	//	***********************************

	for(int i=0;i<MAXCLI;i++){
		if(client[i].name == string(name) && index != i){
			for(int j=0;j<client[i].fileName.size();j++){
				cout << "How many file to send: " << client[i].fileName.size() << endl;

				int isError = 0;
				int qIndex, n;

				//	******************************
				//			Send file name
				//	******************************

				string msg = client[i].fileName[j];
				cout << "send file name: " << msg << endl;
				char MSG[MAXLINE] = {};
				int k;
				for(k=0;k<msg.size();k++)
					MSG[k] = msg[k];
				MSG[k] = '\0';
				
				if(( n = send(sockfd, MSG, MAXLINE, 0)) > 0 ) {
				   	cout << "send n: " << n;
				}
				else if(n == 0){
					cout << "edededededaaaaaaaaasas\n";
				   	if(errno == EWOULDBLOCK){
				   		cout << "One\n";
				   		isError = 1;
				   		for(int jj=0;jj<QSIZE;jj++){
				   			if(waitingQueue[jj].fd == -1){
				   				qIndex = jj;
				   				break;
				   			}
				   		}
				   		waitingQueue[qIndex].fileName = msg;
				 	}
				}
				else{
					cout << "error...\n";
					exit(0);
				}
				
			
				//  *****************************************************
				//                      Read From File
				//  *****************************************************

				size_t result;
				FILE *pFile;
				char * buffer;
				long lSize;
				
				// Open file for both reading
				stringstream ss;
				ss << client[i].saveFilenameNum;
				string str = ss.str();
				string RealName = str + client[i].fileName[j];
				pFile = fopen(RealName.c_str(), "r+");
				//if (pFile==NULL) {fputs ("File error",stderr); exit (1);}
				
				// obtain file size:
				fseek (pFile , 0 , SEEK_END);
				lSize = ftell (pFile);
				rewind (pFile);
				
				// allocate memory to contain the whole file:
				buffer = (char*) malloc (sizeof(char)*lSize);
				if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}
					
				// copy the file into the buffer:
				result = fread (buffer,1,lSize,pFile);
				if (result != lSize) {fputs ("Reading error",stderr); exit (3);}
					
				/* the whole file is now loaded in the memory buffer. */
	  			// terminate
				fclose (pFile);

				//	************************
				//		Send File Size
				//	************************

				int num = lSize;
				char sizeTemp[MAXLINE];
				sprintf(sizeTemp, "%d\n", num);

				if(isError){
					cout << "Two\n";
					strcpy(waitingQueue[qIndex].sizeToSend, sizeTemp);
				}
				else{
					string temp = string(sizeTemp);
					
					char MSG2[MAXLINE] = {};
					int k;
					for(k=0;k<temp.size();k++)
						MSG2[k] = temp[k];
					MSG2[k] = '\0';

					if(( n = send(sockfd, MSG2, MAXLINE, 0)) > 0 ) {
				       	cout << "send n: " << n << endl;
				    }
				}

				//	************************
				//		Send File Data
				//	************************

				if(isError){
					cout << "errno: EWOULDBLOCK...\n";
				   	waitingQueue[qIndex].fd = sockfd;
				   	waitingQueue[qIndex].size = lSize;
				   	waitingQueue[qIndex].buildBuffer();
				   	for(int jj=0;jj<lSize;jj++){
				   		waitingQueue[qIndex].buffer[jj] = buffer[jj];	
				   	}
				   	cout << "qIndex = " << qIndex << endl;
				   	cout << "fd: " << sockfd << endl;
				   	cout << "-------------------------------------------\n";
				   	QNUM++;
				}
				else{
					cout << "send file data, lSize: " << lSize << endl;
					int sendn = 0;
					while(1){
						char TEMP[MAXLINE] = {};
						if(lSize-sendn < MAXLINE){
							int fuck;
							for(fuck=0;fuck<lSize-sendn;fuck++)
								TEMP[fuck] = buffer[fuck + sendn];
							TEMP[fuck] = '\0';
						}
						else{
							for(int fuck=0;fuck<MAXLINE;fuck++)
								TEMP[fuck] = buffer[fuck + sendn];	
						}
						
						if(( n = write(sockfd, TEMP, MAXLINE)) > 0 ) {	
				      		sendn += n;
				      		cout << "sendn: " << sendn << endl;
				      		if(sendn >= lSize)
				      			break;
				      	}	
					}
					cout << "Done a file again...\n";
				}
			}
			cout << "Break...\n";
			break;
		}
	}
}

int isOldClient(string addr, string port){
	for(int i=0;i<MAXCLI;i++){
		if(client[i].fd > 0){
			if(addr == client[i].addr && port == client[i].port){
				// old client
				return client[i].fd;
			}
		}
	}
	return -1;
}

int main(int argc, char ** argv)
{	
	if(argc < 2){
		cerr << "command error...\n";
		exit(0);
	}

	int maxi, maxfd, listenfd, connectfd, sockfd;
	int nready;


	socklen_t clientLen;

	struct sockaddr_in cliaddr, seraddr;


	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&seraddr, sizeof(seraddr));
	seraddr.sin_family = AF_INET;
	seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	seraddr.sin_port = htons(atoi(argv[1]));

	bind(listenfd, (sockaddr *) &seraddr, sizeof(seraddr));
	listen(listenfd, LISTENQ);

	maxfd = listenfd;
	maxi = -1;
	for(int i=0;i<MAXCLI;i++)
		client[i].fd = -1;	// initialize with no one in client array
	FD_ZERO(&allSet);	// initialize
	FD_ZERO(&writeSet);
	FD_SET(listenfd, &allSet);	//	listen to listenfd


	int STAGE = 0;
	while(1){
		readSet = allSet;
		if(QNUM != 0){
			for(int jj=0;jj<QSIZE;jj++){
				if(waitingQueue[jj].fd != -1){
					FD_SET(waitingQueue[jj].fd, &writeSet);
					maxfd = max(maxfd, waitingQueue[jj].fd);
				}
			}
		}

		nready = select(maxfd + 1, &readSet, &writeSet, NULL, NULL);

		for(int i=0;i<QSIZE;i++){
			if(waitingQueue[i].fd != -1){
				int n;
				int socketFd = waitingQueue[i].fd;
				int size = waitingQueue[i].size;
				string fileName = waitingQueue[i].fileName;
				char sizeToSend[MAXLINE];
				strcpy(sizeToSend, waitingQueue[i].sizeToSend);
				char buffer[size];
				for(int jj=0;jj<size;jj++)
					buffer[jj] = waitingQueue[i].buffer[jj];
				
				if(FD_ISSET(socketFd, &writeSet)){
					write(socketFd, fileName.c_str(), MAXLINE);
					write(socketFd, sizeToSend, MAXLINE);

					//	*************************
					//		Re-send data
					//	*************************

					cout << "send file data, size: " << size << endl;
					int sendn = 0;
					while(1){
						char TEMP[MAXLINE] = {};
						if(size-sendn < MAXLINE){
							int fuck;
							for(fuck=0;fuck<size-sendn;fuck++)
								TEMP[fuck] = buffer[fuck + sendn];
							TEMP[fuck] = '\0';
						}
						else{
							for(int fuck=0;fuck<MAXLINE;fuck++)
								TEMP[fuck] = buffer[fuck + sendn];	
						}
						
						if(( n = write(socketFd, TEMP, MAXLINE)) > 0 ) {	
				      		sendn += n;
				      		cout << "sendn: " << sendn << endl;
				      		if(sendn >= size)
				      			break;
				      	}	
					}
					cout << "Done a file again...\n";

					if(n > 0){
						cout << "QNUM: " << QNUM << endl;
						waitingQueue[i].fd = -1;

						char tempBuff[MAXLINE];
						recv(sockfd, tempBuff, MAXLINE, 0);
						
						FD_CLR(socketFd, &writeSet);
						QNUM--;
					}
				}
			}
		}

		if(FD_ISSET(listenfd, &readSet)){
			clientLen = sizeof(cliaddr);
			connectfd = accept(listenfd, (sockaddr *) &cliaddr, &clientLen);
			
			/**********************************************/
            /* Accept each incoming connection.  If       */
            /* accept fails with EWOULDBLOCK, then we     */
            /* have accepted all of them.  Any other      */
            /* failure on accept will cause us to end the */
            /* server.                                    */
            /**********************************************/
            
            if (connectfd < 0)
            {
                if (errno != EWOULDBLOCK)
                {
                    cout << "accept() failed\n";
                    exit(0);
                }
                break;
            }

            /**********************************************/
            /* Add the new incoming connection to the     */
            /* master read set                            */
            /**********************************************/


			int flag = fcntl(connectfd, F_GETFL, 0);
			fcntl(connectfd, F_SETFL, flag|O_NONBLOCK);

			int i;
			for(i=0;i<MAXCLI;i++){
				if(client[i].fd == -1){
					//	client in
					char temp[5];
					sprintf(temp,"%4d",(int)cliaddr.sin_port);

					cout << "-------------------------------------\n";
					cout << "New client...\n";

					cout << string(inet_ntoa(cliaddr.sin_addr)) << ", " << temp << ", " << connectfd << endl;

					client[i].addr = string(inet_ntoa(cliaddr.sin_addr));
					client[i].port = temp;
					client[i].fd = connectfd;
					client[i].isFirst = 1;

						
					TOTALCLI++;
					break;
				}
			}
			if(i == MAXCLI){
				cout << "Too many client...\n";
				exit(0);
			}
			
			FD_SET(connectfd, &allSet);
			if(connectfd > maxfd)
				maxfd = connectfd;
			if(maxi < i)
				maxi = i;
			if(--nready <= 0)
				continue;
		}

		for(int i=0;i<=maxi;i++){
			if((sockfd = client[i].fd) < 0)
				continue;
			if(FD_ISSET(sockfd, &readSet)){
				int n;
				char buffer[MAXLINE];
				bzero(buffer, MAXLINE);
				if((n = recv(sockfd, buffer, MAXLINE, 0)) > 0){
					cout << "read: " << n << endl;
					buffer[n] = 0;
					if(client[i].isFirst){
						string msg = string("Welcome to the dropbox-like server! : " + string(buffer)) + "\n\0";
						char MSG[MAXLINE] = {};
						int k;
						for(k=0;k<msg.size();k++)
							MSG[k] = msg[k];
						MSG[k] = '\0';

						n = send(connectfd, MSG, MAXLINE, 0);	//	send welcoming msg to new client
						cout << "welcome msg: " << msg;
						cout << "send welcome n: " << n << endl;
						client[i].name = string(buffer);
						client[i].isFirst = 0;

						msg = client[i].addr + " " + client[i].port + "," + string(buffer);
						
						dealName(msg);
					}
					else{
						string msg = client[i].addr + " " + client[i].port + "," + string(buffer);
						dealString(msg);
					}

				}
				if(n == 0){
					cout << "Client terminates...\n";
					string msg = "[Server] " + client[i].name + " is offline.\n";
					close(sockfd);
					TOTALCLI--;
					client[i].fd = -2;	//	client terminates
					FD_CLR(sockfd, &allSet);
				}
				if(--nready <= 0)
					break;	// no more readable descriptors
			}
		}


	}


	return 0;
}
