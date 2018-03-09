#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <wait.h>
#include <typeinfo>

using namespace std;

#define MAXLINE 1000

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

int getNum(const string &str, int nextIndex){
	char numChar[str.size()-nextIndex];
	for(int i=nextIndex;i<str.size();i++)
		numChar[i-nextIndex] = str[i];
	return atoi(numChar);
}

string getFileName(const string &str, int nextIndex){
	char numChar[str.size()-nextIndex+1];
	for(int i=nextIndex;i<str.size();i++)
		numChar[i-nextIndex] = str[i];
	numChar[str.size()-nextIndex] = '\0';
	return string(numChar);
}


int main(int argc, char **argv)
{
	int sockfd;
	int n;
	struct sockaddr_in servaddr;	// sockaddr_in is in <netinet/in.h>

	if(argc != 4)
		cerr << "argc error.....\n";
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		cerr << "socket error.....\n";

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));

	if(argv[1][0] <= '9' && argv[1][0] >= '0'){
		if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
			cerr << "inethtop error for " << argv[1] << ".....\n";	
	}

	//	**************************************
	//			Blocking Connect
	//	**************************************
	if(connect(sockfd, (sockaddr *) &servaddr, sizeof(servaddr)))
		cerr << "connect error.....\n";

	//	**********************
	//		Send Username
	//	**********************

	string name = string(argv[3]);
	send(sockfd, name.c_str(), name.size(), MSG_CONFIRM);
	//cout << "my name: " << name << "\n";
	//cout << "Username send complete...\n";
	
	//	*******************************
	//		Read welcoming message
	//	*******************************

	char recvline[MAXLINE] = {};
	if((n = read(sockfd, recvline, MAXLINE)) > 0){
		//cout << "welcome n: " << n << endl;
		cout << string(recvline);
	}
	

	//	***********************************************************************
	
	int 	maxfdp1;
	fd_set	rset;

	FD_ZERO(&rset);
	for(;;){
		FD_SET(0, &rset);
		FD_SET(sockfd, &rset);
		maxfdp1 = max(0, sockfd) + 1;
		select(maxfdp1, &rset, NULL, NULL, NULL);

		
		if(FD_ISSET(sockfd, &rset)){	//	socket is readable

			//	**************************
			//		Get file name
			//	**************************

			string FILENAME;
			int n;
			char recvline[MAXLINE] = {};
			bzero(recvline, MAXLINE);

			//cout << "Get file name...\n";
			while(1){
				if((n = read(sockfd, recvline, MAXLINE)) > 0){
					//cout << "Get: " << n << endl;
					recvline[n] = 0;

					//cout << "File Name: " << string(recvline) << "..." << endl;
					FILENAME = string(recvline);
					if(n == MAXLINE)
						break;
				}
			}
			
			cout << "Downloading file : " << FILENAME << endl;
			cout << "Progress : [                             ]\r";

			//	**************************
			//		Get file size
			//	**************************

			int FILESIZE;
			//cout << "Get file size...\n";
			char RECV[MAXLINE] = {};
			if((n = read(sockfd, RECV, MAXLINE)) > 0){
				//cout << "Get size's n: " << n << endl;
				//cout << RECV[0] << endl;

				//cout << "File Size: " << string(RECV);
				FILESIZE = atoi(string(RECV).c_str());
			}
			cout << "Progress : [#######                      ]\r";

			//	**************************
			//		Get file data
			//	**************************

			char *buffData;
			// allocate memory to contain the whole file:
			buffData = (char*) malloc (sizeof(char)*FILESIZE);
			if (buffData == NULL) {fputs ("Memory error",stderr); exit (2);}
			cout << "Progress : [###############              ]\r";
			size_t result;
			FILE *pFile;

			// Open file for both writing
			pFile = fopen(FILENAME.c_str(), "w+");

			cout << "Progress : [######################       ]\r";
			int total = 0;
			while(1){
				char TEMPBUFF[MAXLINE] = {};
				if((n = read(sockfd, TEMPBUFF, MAXLINE)) > 0){
					//cout << string(TEMPBUFF) << endl;
					//cout << "Get " << n << " datas...\n";
					//  *****************************************************
					//                     Write to File
					//  *****************************************************

					if(n > MAXLINE){
						total += FILESIZE;
						for(int iii=0;iii<FILESIZE;iii++)
							buffData[iii] = TEMPBUFF[iii];
						fwrite(buffData, 1, FILESIZE, pFile);   
						break;
					}
					else if(n < MAXLINE){
						total += n;
						char TEMPBUFF2[n] = {};
						for(int iii=0;iii<n;iii++)
							TEMPBUFF2[iii] = TEMPBUFF[iii];
						fwrite(TEMPBUFF2, 1, n, pFile);   
					}
					else if(FILESIZE - total < MAXLINE){
						char TEMPBUFF2[FILESIZE - total] = {};
						for(int iii=0;iii<FILESIZE - total;iii++)
							TEMPBUFF2[iii] = TEMPBUFF[iii];
						fwrite(TEMPBUFF2, 1, FILESIZE - total, pFile);
						total += n;
					}
					else{
						total += n;
						//cout << string(TEMPBUFF) << endl; 
						fwrite(TEMPBUFF, 1, n, pFile);
					}

				    
				    //cout << "total: " << total << endl;
				    if(total >= FILESIZE){
				    	break;	
				    }
				}
			}

			cout << "Progress : [#############################]\n";
			cout << "Download " << FILENAME << " complete!\n";
	    	fclose(pFile);
		}
		

		if(FD_ISSET(0, &rset)){			//	input is readable
			string buff;
			getline(cin, buff);

			if(buff[0] == '/')
				buff = buff.substr(1, buff.size());
			else{
				cout << "Wrong input...\n";
				continue;
			}

			if(buff == "exit\0"){
				//	terminate connection
				//cout << "---------------------Close------------------------\n";
				shutdown(sockfd, 2);
				exit(0);
			}
			else{
				//	********************
				//		Get command
				//	********************
				int nextIndex;
				string command = getCommand(buff, nextIndex);

				//	********************
				//		If is Sleep
				//	********************
				if(command == "sleep\0"){
					int sleepNum = getNum(buff, nextIndex);
					//cout << "Sleep " << sleepNum << " seconds..." << endl;
					
					cout << "Client starts to sleep\n";			
					//	sleep
					int countSleep = 0;
					while(countSleep < sleepNum){
						cout << "Sleep " << countSleep + 1 << endl;
						sleep(1);
						countSleep++;
					}
					cout << "Client wakes up\n";
				}
				else if(command == "put\0"){
			      	
			      	//  *****************************************************
				    //                      Read From File
				    //  *****************************************************

			      	string fileName = getFileName(buff, nextIndex);
			      	cout << "Uploading file : " << fileName << endl;
			      	cout << "Progress : [                           ]\r";
			      	long lSize;
				    size_t result;
				    FILE *pFile;
					char * buffer;

					// Open file for both reading
					pFile = fopen(fileName.c_str(), "r+");
					if(pFile == NULL){
						cout << "Wrong File...\n";
						continue;
					}
					//if (pFile==NULL) {fputs ("File error",stderr); exit (1);}

					cout << "Progress : [#######                    ]\r";

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
					
					// the whole file is now loaded in the memory buffer.
	  			  	// terminate
					fclose (pFile);
					cout << "Progress : [#############              ]\r";
					//	**********************************
					//		Send filename and size
					//	**********************************

					int num = lSize;
					char msgChar[MAXLINE];
					sprintf(msgChar, "%d", num);

					buff += " " + string(msgChar);
					//cout << "send name and size: " << buff << endl;
					
					char MSG[MAXLINE] = {};
					int k;
					for(k=0;k<buff.size();k++){
						MSG[k] = buff[k];
					}
					MSG[k] = '\0';
					cout << "Progress : [####################       ]\r";
					int n = 0;
					if((n = send(sockfd, MSG, MAXLINE, 0)) > 0 ) {
			       		//cout << "send n: " << n << endl;	
			      	}

			      	//	**********************************************************************
			      	

			      	//cout << "send file data, lSize: " << lSize << endl;
					if(( n = write(sockfd, buffer, lSize)) > 0 ) {
			       		//cout << "send n: " << n << endl;	
			      	}
			      	cout << "Progress : [###########################]\n";
			      	cout << "Upload " << fileName << " complete!\n";
			      	free(buffer);
				}
			}
		}
	}
	
	return 0;
}
