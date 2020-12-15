#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ctype.h>
#include <string>
#include <iostream>
#include <netdb.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <bits/stdc++.h>
#include <openssl/sha.h>
#include <sys/mman.h>
#include <sys/stat.h>
#define PORT 8080
#define MAX 4096
#define CLIENT_IP "127.0.0.1"
#define CHUNK_SIZE 32768
using namespace std;

//list of pending req : (gid, <users>)
map<string, vector<string>> join_reqs; 

vector<string> tokens;
map<string, vector<string>> files_completed;   // gid:filename
map<string, vector<string>> files_downloading; // gid:filename

void *read_data_from_client(void *arg);
void *peerListener(void *arg);
void create_account(string uid, string pass, string peer_address);
void login(string user_id, string pass, string peer_address);
void create_group(string gid, string peer_address);
void logout(string peer_address);
void join_group(string gid, string peer_address);
void list_groups(string peer_address);
void list_requests(string gid, string peer_address);
void accept_request(string gid, string uid, string peer_address);
void leave_group(string gid, string peer_address);
string getSHAofFile(string path, unsigned long long int sizeInBytes);
void upload_file(string filepath, string gid, string peer_address);
void list_files(string gid, string peer_address);
void *get_chunk_as_client(void *arg);
void download_file(string gid, string filename, string dest_path, string peer_address);
void show_downloads();
void stop_sharing(string gid, string filename, string peer_address);

int main(int argc, char const *argv[])
{
	// cout << "peer started : \n";
	string peer_address = argv[1];
	char *client_addr = new char[peer_address.length() + 1];
	strcpy(client_addr, peer_address.c_str());
	pthread_t thread1;
	pthread_create(&thread1, NULL, peerListener, (void *)client_addr);
	string command, user_id, pass, gid, filepath, dest_path, filename;

	while(1)
	{
		cin >> command;
		if (command == "create_user")
		{
			cin >> user_id >> pass;
			create_account(user_id, pass, peer_address);
		}
		else if (command == "login")
		{
			cin >> user_id >> pass;
			login(user_id, pass, peer_address);
		}
		else if (command == "create_group")
		{
			cin >> gid;
			create_group(gid, peer_address);
		}
		else if (command == "logout")
		{
			logout(peer_address);
		}
		else if (command == "join_group")
		{
			cin >> gid;
			join_group(gid, peer_address);
		}
		else if (command == "list_groups")
		{
			list_groups(peer_address);
		}
		else if (command == "leave_group")
		{
			cin >> gid;
			leave_group(gid, peer_address);
		}
		else if (command == "list_requests")
		{
			cin >> gid;
			list_requests(gid, peer_address);
		}
		else if (command == "accept_request")
		{
			cin >> gid >> user_id;
			accept_request(gid, user_id, peer_address);
		}
		else if (command == "upload_file")
		{
			cin >> filepath >> gid;
			upload_file(filepath, gid, peer_address);
		}
		else if (command == "list_files")
		{
			cin >> gid;
			list_files(gid, peer_address);
		}

		else if (command == "show_downloads")
			show_downloads();
		
		else if (command == "stop_share")
		{
			cin >> gid >> filename;
			stop_sharing(gid, filename, peer_address);
		}
		else if (command == "download_file")
		{
			cin >> gid >> filename >> dest_path;
			files_downloading[gid].push_back(dest_path);
			download_file(gid, filename, dest_path, peer_address);
			int count = 0;
			for (auto i : files_downloading[gid])
			{
				if (i == dest_path)
				{
					files_downloading[gid].erase(files_downloading[gid].begin() + count);
					files_completed[gid].push_back(dest_path);
					break;
				}
				count++;
			}
			upload_file(dest_path, gid, peer_address);
		}

		else
			cout << "Command : " << command << " is Invalid \n";

	}
	pthread_join(thread1, NULL);
	return 0;
}

void *read_data_from_client(void *arg)
{
	int sockfd = *(int *)arg;
	char buff_req[MAX];
	int n;
	bzero(buff_req, MAX);
	read(sockfd, buff_req, sizeof(buff_req));
	char s = buff_req[0];
	if (s == 'z')
	{ 
		// join request update
		string j_req = buff_req;
		j_req = j_req.substr(2); //ip:port:user:pass

		stringstream check1(j_req);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// uid of requester
		string uid = tokens[0]; 
		string gid = tokens[1];

		join_reqs[gid].push_back(uid);
		char buff_response[MAX] = "request for joining has been received\n";
		write(sockfd, buff_response, sizeof(buff_response));
		tokens.clear();
	}

	if (s == 'b')
	{ 
		//download_file_request
		string j_req = buff_req;
		j_req = j_req.substr(2);
		tokens.clear();
		stringstream check1(j_req);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		int chunk_no = stoi(tokens[0]); // chunk_no_requested
		string filepath = tokens[1];	
		char buffer[CHUNK_SIZE];		// buffer to be sent to requester
		FILE *fptr;
		fptr = fopen(filepath.c_str(), "r");
		fseek(fptr, chunk_no * CHUNK_SIZE, SEEK_SET);
		int val = fread(buffer, 1, CHUNK_SIZE, fptr);
		string buff_response(buffer);
		char check_response[MAX];
		buff_response = to_string(val) + ":" + buff_response;
		strcpy(check_response, buff_response.c_str());
		cout << "Buffer Response from func = " << check_response << endl;
		//char buff_response[MAX] = "request for joining has been received\n";
		write(sockfd, check_response, sizeof(check_response));
		fclose(fptr);
		tokens.clear();
	}
	return NULL;
}

void *peerListener(void *arg)
{ 
	// client as a server
	char *maddr = (char *)arg;
	string peer_address = maddr; // client's server address
	int server_socketfd;
	struct sockaddr_in address;
	int pos = peer_address.find(":");
	string my_ip = peer_address.substr(0, pos);
	string po = peer_address.substr(pos + 1);
	int port = stoi(po);

	if ((server_socketfd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	else
		printf("Socket created successful...\n");

	int opt = 1;
	if (setsockopt(server_socketfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(my_ip.c_str());
	address.sin_port = htons(port);

	if (bind(server_socketfd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	else
		printf("bind successful\n");
	

	if (listen(server_socketfd, 5) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}
	else	
		printf("listen successful\n");
	

	struct sockaddr_in clientAddr;
	pthread_t tid;
	int new_socket;
	int clilen = sizeof(clientAddr);
	while ((new_socket = accept(server_socketfd, (struct sockaddr *)&clientAddr, (socklen_t *)&clilen)) >= 0)
	{
		printf("new connection from %s:%d: \n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
		pthread_create(&tid, NULL, read_data_from_client, (void *)&new_socket);
	}

	close(server_socketfd);

	return NULL;
}

void create_account(string uid, string pass, string peer_address)
{
	string msg = "a:" + peer_address + ":" + uid + ":" + pass + "\0";
	// cout<<msg<<"\n";
	char sms_to_Tracker[256];
	// new char[msg.length()+1];
	strcpy(sms_to_Tracker, msg.c_str());
	// cout<<sms<<"\n";
	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	write(socketfd, sms_to_Tracker, sizeof(sms_to_Tracker));
	// cout << "after write : \n" << sms << endl;
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	cout << buff << endl;
	close(socketfd);
}

void login(string user_id, string pass, string peer_address)
{
	string msg = "b:" + peer_address + ":" + user_id + ":" + pass + "\0";
	// cout<<msg<<"\n";
	char sms_to_Tracker[256];
	// new char[msg.length()+1];
	strcpy(sms_to_Tracker, msg.c_str());
	// cout<<sms<<"\n";
	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		cout<<"\n Socket creation error\n";
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	write(socketfd, sms_to_Tracker, sizeof(sms_to_Tracker));
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	cout << buff << endl;
	close(socketfd);
}

void create_group(string gid, string peer_address)
{
	string msg_to_Tracker = "c:" + peer_address + ":" + gid + "\0";
	// cout<<msg<<"\n";
	char sms_to_tracker[256];
	// new char[msg.length()+1];
	strcpy(sms_to_tracker, msg_to_Tracker.c_str());
	// cout<<sms<<"\n";
	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address\n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	write(socketfd, sms_to_tracker, sizeof(sms_to_tracker));
	// cout << "after write : \n" << sms << endl;
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	printf("%s\n", buff);
	close(socketfd);
}

void logout(string peer_address)
{
	string msg = "d:" + peer_address + "\0";
	// cout<<msg<<"\n";
	char sms[256];
	// new char[msg.length()+1];
	strcpy(sms, msg.c_str());
	// cout<<sms<<"\n";
	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	write(socketfd, sms, sizeof(sms));
	// cout << "after write : \n" << sms << endl;
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	printf("%s\n", buff);
	close(socketfd);
}

void join_group(string gid, string peer_address)
{
	int pos = peer_address.find(":");
	string my_ip = peer_address.substr(0, pos);
	int port = stoi(peer_address.substr(pos + 1));
	// int port = stoi(po);

	string msg = "e:" + peer_address + ":" + gid + "\0";
	// cout<<msg<<"\n";
	char sms_to_tracker[256];
	// new char[msg.length()+1];
	strcpy(sms_to_tracker, msg.c_str());
	// cout<<sms<<"\n";
	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	write(socketfd, sms_to_tracker, sizeof(sms_to_tracker));
	// cout << "after write : \n" << sms << endl;
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	cout << "response form server : " << buff << endl;
	string owner_addr(buff);
	pos = -1;
	pos = owner_addr.find(":");
	if (pos == -1)
	{
		cout << buff << endl;
		return;
	}
	string uid = owner_addr.substr(0, pos);
	string addr = owner_addr.substr(pos + 1);
	cout << "addre  : " << addr << endl;
	pos = addr.find(":");
	my_ip = addr.substr(0, pos);
	cout << "my ip : " << my_ip << endl;
	port = stoi(addr.substr(pos + 1));
	cout << "popo : " << port << endl;
	// port = stoi(po);
	close(socketfd);

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	cout << "ip : " << my_ip << "  port : " << port << endl;
	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, my_ip.c_str(), &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	string req_msg = "z:" + uid + ":" + gid;
	write(socketfd, req_msg.c_str(), sizeof(req_msg));
	string req = "request sent to owner\n";
	cout << req << endl;
	close(socketfd);
}

void list_groups(string peer_address)
{
	string msg = "f:" + peer_address + "\0";
	// cout<<msg<<"\n";
	char sms[256];
	// new char[msg.length()+1];
	strcpy(sms, msg.c_str());
	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	write(socketfd, sms, sizeof(sms));
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	cout << buff << endl;
	close(socketfd);
}

void list_requests(string gid, string peer_address)
{
	vector<string> pending_users = join_reqs[gid];
	for (auto i : pending_users)
		cout << i << endl;
}

void accept_request(string gid, string uid, string peer_address)
{
	bool flag = false;
	int count = 0;
	for (auto i : join_reqs[gid])
	{
		if (i == uid)
		{
			flag = true;
			join_reqs[gid].erase(join_reqs[gid].begin() + count);
			break;
		}
		count++;
	}
	if (flag == true)
	{
		string msg = "g:" + peer_address + ":" + uid + ":" + gid + "\0";
		// cout<<msg<<"\n";
		char sms_to_tracker[256];
		// new char[msg.length()+1];
		strcpy(sms_to_tracker, msg.c_str());
		// cout<<sms<<"\n";
		int socketfd;
		struct sockaddr_in serv_addr;
		char buff[MAX] = {0};

		if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			printf("\n Socket creation error\n");
			return;
		}

		bzero(&serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(PORT);

		// Convert IPv4 and IPv6 addresses from text to binary form
		if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
		{
			printf("\nInvalid Address/ Address not supported \n");
			return;
		}

		if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		{
			printf("\nConnection Failed \n");
			return;
		}
		write(socketfd, sms_to_tracker, sizeof(sms_to_tracker));
		bzero(buff, MAX);
		read(socketfd, buff, MAX);
		cout << buff << endl;
		close(socketfd);
	}
}

void leave_group(string gid, string peer_address)
{
	// int pos = peer_address.find(":");
	// string my_ip = peer_address.substr(0, pos);
	// string po = peer_address.substr(pos + 1);
	// int port = stoi(po);

	string msg = "h:" + peer_address + ":" + gid + "\0";
	// cout<<msg<<"\n";
	char sms[256];

	if (join_reqs.find(gid) != join_reqs.end())
	{
		cout << "deleting in join reqs \n";
		join_reqs.erase(gid);
	}

	// new char[msg.length()+1];
	strcpy(sms, msg.c_str());
	// cout<<sms<<"\n";
	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	write(socketfd, sms, sizeof(sms));
	// cout << "after write : \n" << sms << endl;
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	cout << "response form server : " << buff << endl;
	close(socketfd);
}

string getSHAofFile(string path, unsigned long long int sizeInBytes)
{
	char *file_buffer;
	unsigned char *result = new unsigned char[20];
	int fd = open(path.c_str(), O_RDONLY);
	file_buffer = (char *)mmap(0, sizeInBytes, PROT_READ, MAP_SHARED, fd, 0);
	SHA1((unsigned char *)file_buffer, sizeInBytes, result);
	munmap(file_buffer, sizeInBytes);
	close(fd);
	char *sha1hash = (char *)malloc(sizeof(char) * 41);
	sha1hash[41] = '\0';
	int i;
	for (i = 0; i < SHA_DIGEST_LENGTH; i++)
		sprintf(&sha1hash[i * 2], "%02x", result[i]);

	string final(sha1hash);
	return final;
}

void upload_file(string filepath, string gid, string peer_address)
{
	ifstream inputFile(filepath, ios::binary);
	inputFile.seekg(0, ios::end);
	int inFile_size = inputFile.tellg();
	cout << "Size of the file is"
		 << " " << inFile_size << " "
		 << "bytes" << endl;
	string sha = getSHAofFile(filepath, inFile_size);
	string msg = "i:" + peer_address + ":" + filepath + ":" + gid + ":" + to_string(inFile_size) + ":" + sha + "\0";
	char sms[256];
	strcpy(sms, msg.c_str());

	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	write(socketfd, sms, sizeof(sms));
	// cout << "after write : \n" << sms << endl;
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	printf("%s\n", buff);
	close(socketfd);
}

void list_files(string gid, string peer_address)
{
	string msg = "j:" + peer_address + ":" + gid + "\0";

	char smsToTracker[256];

	strcpy(smsToTracker, msg.c_str());

	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	write(socketfd, smsToTracker, sizeof(smsToTracker));
	// cout << "after write : \n" << sms << endl;
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	cout << buff << endl;
	close(socketfd);
}

void *get_chunk_as_client(void *arg)
{
	char *addr = (char *)arg;
	string peer_address(addr);
	cout << "Data received from peer client = " << peer_address << endl;

	// tokens.clear();
	vector<string> tokens2;
	stringstream check1(peer_address);
	string intermediate;
	while (getline(check1, intermediate, ':'))
		tokens2.push_back(intermediate);

	int tokens_size = tokens2.size();
	string my_ip = tokens2[0];
	int port = stoi(tokens2[1]);
	string chunk_no = tokens2[2];
	string filename = tokens2[3]; 
	string destination_path = tokens2[4];

	cout << "my ip : " << my_ip << endl;
	cout << "IP - " << my_ip << " PORT = " << port << endl;

	string sms = "b:" + chunk_no + ":" + filename;
	cout << "Meesage sent to peerlistener server = " << sms << endl;

	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return NULL;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, my_ip.c_str(), &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return NULL;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return NULL;
	}
	write(socketfd, sms.c_str(), sizeof(sms));

	bzero(buff, MAX);

	read(socketfd, buff, MAX);
	//printf("%s\n", buff);
	cout << "Response to be written to file " << buff << endl;

	string response_buffer(buff);
	int pos = response_buffer.find(":");
	int val = stoi(response_buffer.substr(0, pos));
	string chunk_data = response_buffer.substr(pos + 1);
	//chunk_data+="\0";
	cout << "CHUNK data to be written to file" << chunk_data << endl;
	fstream f;
	f.open(destination_path, ios::out | ios::binary | ios::in);
	f.seekg(stoi(chunk_no) * CHUNK_SIZE, f.beg);
	f.write(chunk_data.c_str(), val);
	f.close();

	close(socketfd);

	return NULL;
}

void download_file(string gid, string filename, string dest_path, string peer_address)
{
	int pos = peer_address.find(":");
	string my_ip = peer_address.substr(0, pos);
	string po = peer_address.substr(pos + 1);
	int port = stoi(po);
	string msg = "k:" + peer_address + ":" + gid + ":" + filename + "\0";
	cout << "Message sent to tracker = " << msg << "\n";
	char sms[256];

	strcpy(sms, msg.c_str());

	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	write(socketfd, sms, sizeof(sms));
	// cout << "after write : \n" << sms << endl;
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	cout << "response form server : " << buff << endl;
	string peer_ports(buff);
	// int no_of_chunks =
	tokens.clear();
	stringstream check1(peer_ports);
	string intermediate;
	while (getline(check1, intermediate, ':'))
		tokens.push_back(intermediate);
	int tokens_size = tokens.size();
	int no_of_peer_ports = tokens_size - 3;
	cout << "No. of peer ports = " << no_of_peer_ports << endl;
	if (no_of_peer_ports == 0)
	{
		cout << "No Peers Online";
		close(socketfd);
		return;
	}

	string sha_org = tokens[tokens_size - 3];
	string file_size = tokens[tokens_size - 2];
	string filepath = tokens[tokens_size - 1];
	int no_of_chunks = ceil(stof(file_size) / float(CHUNK_SIZE)); // CHANGE LATER

	pthread_t tid[no_of_chunks];


	FILE *fptr;
	fptr = fopen(dest_path.c_str(), "w");
	fclose(fptr);
	
	//PIECE SELECTION ALGO
	for (int i = 0; i < no_of_chunks; i++)
	{
		string colon = ":";
		string msg = CLIENT_IP + colon + tokens[i % no_of_peer_ports] + colon + to_string(i) + colon + filepath + colon + dest_path;
		cout << "Message to peer server = " << msg << endl;
		pthread_create(&tid[i], NULL, get_chunk_as_client, (void *)msg.c_str());
		pthread_join(tid[i], NULL);
	}

	ifstream inputFile(dest_path, ios::binary);
	inputFile.seekg(0, ios::end);
	int inFile_size = inputFile.tellg();
	cout << "Size of the file is"
		 << " " << inFile_size << " "
		 << "bytes" << endl;
	string sha_downloaded = getSHAofFile(dest_path, inFile_size);

	if (strcmp(sha_org.c_str(), sha_downloaded.c_str()) == 0)
		cout << "File Downloaded Successfully!!" << endl;
	else
	{
		cout << "Error Occurred in Downloading" << endl;
	}


	close(socketfd);
}

void show_downloads()
{
	for (auto i : files_downloading)
		for (auto j : i.second)
			cout << "[D]\t" << i.first << "\t" << j << endl;
	
	for (auto i : files_completed)
		for (auto j : i.second)
			cout << "[C]\t" << i.first << "\t" << j << endl;
	
	return ;
}

void stop_sharing(string gid, string filename, string peer_address)
{

	string msg = "l:" + peer_address + ":" + gid + ":" + filename + "\0";
	cout << "Message sent to tracker = " << msg << "\n";
	char sms[256];

	strcpy(sms, msg.c_str());

	int socketfd;
	struct sockaddr_in serv_addr;
	char buff[MAX] = {0};

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error\n");
		return;
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid Address/ Address not supported \n");
		return;
	}

	if (connect(socketfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return;
	}
	// cout << "before write : \n" << sms << endl;
	write(socketfd, sms, sizeof(sms));
	// cout << "after write : \n" << sms << endl;
	bzero(buff, MAX);
	read(socketfd, buff, MAX);
	cout << "response form server : " << buff << endl;
	if (buff[0] == 's')
	{
		cout << "Succesfully stopped sharing " << filename << endl;
	}
	else
	{
		cout << "You dont have the file to stop sharing " << endl;
	}
}