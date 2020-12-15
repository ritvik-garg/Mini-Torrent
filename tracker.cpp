#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <bits/stdc++.h>
#include <algorithm>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#define PORT 8080
#define MAX 4096
#define TRACKER_IP "127.0.0.1"
using namespace std;

struct peer
{
	string uid;
	map<string, int> groupId; // <gid, isOwner>  0/1 (1->owner)
	bool isloggedIn;
	string pwd;
	string cip;
	int cport;
	vector<string> uploadfiles;
	vector<string> down_files;
};

struct file
{
	string filepath;
	string fileName;
	string fileSize;
	string uid;
	string sha;
	string gid;
};

map<int, string> port_user;
map<string, peer> peer_details;
map<string, vector<file>> group_file_details;
map<string, string> groupId_owner;
map<string, vector<string>> groupId_users;
vector<string> tokens;
map<string, vector<pair<string, string>>> clients_having_file;

void *funcForQuit(void *arg);
// void create_acc(string peer_ip, int port, string uid, string pass, int sockfd);
void *parseInput(void *arg);

int main(int argc, char const *argv[])
{
	//server as a client for quit only.
	pthread_t tid1;
	pthread_create(&tid1, NULL, funcForQuit, NULL);

	string tracker_no = argv[2];
	string tracker_filename = argv[1];
	string tracker_details = tracker_no + " " + TRACKER_IP + ":" + to_string(PORT);
	ofstream f(tracker_filename);
	f << tracker_details;
	f.close();
	int server_socketfd;
	// below is a structure which contain IP and port address
	struct sockaddr_in address; //server address

	string hello = "Hi from server";

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
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(PORT);

	if (bind(server_socketfd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	if (listen(server_socketfd, 5) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in clientAddr;
	pthread_t tid;

	int new_socket;
	int clilen = sizeof(clientAddr);
	while ((new_socket = accept(server_socketfd, (struct sockaddr *)&clientAddr, (socklen_t *)&clilen)) >= 0)
	{
		printf("Server Connected to %s:%d: \n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
		pthread_create(&tid, NULL, parseInput, (void *)&new_socket);
	}
	pthread_join(tid, NULL);
	close(server_socketfd);
	return 0;
}

void *parseInput(void *arg)
{
	struct peer p_obj;
	int sockfd = *(int *)arg;
	char buff[MAX];
	bzero(buff, MAX);
	read(sockfd, buff, sizeof(buff));
	printf("From client: %s", buff);
	cout << "\n";
	char s = buff[0];
	if (s == 'a')
	{
		//a->create_user
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:user:pass
		// cout<<cr_acc<<"\n";

		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);
		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string uid = tokens[2];
		string pass = tokens[3];
		// create_acc(peer_ip, port, uid, pass, sockfd);
		p_obj.uid = uid;
		p_obj.pwd = pass;
		p_obj.cport = port;
		p_obj.cip = peer_ip;
		p_obj.isloggedIn = false;
		// p_arr.push_back(p_obj);
		if (peer_details.find(uid) == peer_details.end())
		{
			peer_details[uid] = p_obj;
			port_user[port] = uid;
			char buffer[MAX] = "Account creation successful!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		else
		{
			char buffer[MAX] = "UserId already exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	else if (s == 'b')
	{
		// for login
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:user:pass
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string uid = tokens[2];
		string pass = tokens[3];

		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].pwd == pass)
			{
				if (peer_details[uid].isloggedIn == false)
				{
					peer_details[uid].isloggedIn = true;
					char buffer[MAX] = "Login successful!!";
					write(sockfd, buffer, sizeof(buffer));
				}
				else
				{
					char buffer[MAX] = "Already logged in!!";
					write(sockfd, buffer, sizeof(buffer));
				}
			}
			else
			{
				char buffer[MAX] = "Pwd failed!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	else if (s == 'c')
	{ // for create_group
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:user:pass
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string gid = tokens[2];

		string uid = port_user[port];

		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				if (groupId_owner.find(gid) == groupId_owner.end())
				{
					groupId_owner[gid] = uid;
					peer_details[uid].groupId[gid] = 1;
					groupId_users[gid].push_back(uid);
					char buffer[MAX] = "Group Creation successful!!";
					write(sockfd, buffer, sizeof(buffer));
				}
				else
				{
					char buffer[MAX] = "GroupId already exist!!";
					write(sockfd, buffer, sizeof(buffer));
				}
			}
			else
			{
				char buffer[MAX] = "Login to create group!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	else if (s == 'd')
	{ // for logout
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);

		string uid = port_user[port];

		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				peer_details[uid].isloggedIn = false;
				char buffer[MAX] = "Logged Out successful!!";
				write(sockfd, buffer, sizeof(buffer));
			}
			else
			{
				char buffer[MAX] = "Already Looged Out!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	else if (s == 'e')
	{ // for joining goup
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:user:pass
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string gid = tokens[2];

		string uid = port_user[port];

		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				if (groupId_owner.find(gid) != groupId_owner.end())
				{
					string owner_id = groupId_owner[gid];
					string owner_ip = peer_details[owner_id].cip;
					int owner_port = peer_details[owner_id].cport;
					string msg = uid + ":" + owner_ip + ":" + to_string(owner_port) + "\0";
					char buffer[MAX];
					// = msg.c_str	();
					strcpy(buffer, msg.c_str());
					write(sockfd, buffer, sizeof(buffer));
				}
				else
				{
					char buffer[MAX] = "GroupId does not exist!!";
					write(sockfd, buffer, sizeof(buffer));
				}
			}
			else
			{
				char buffer[MAX] = "Login to join group!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	else if (s == 'f')
	{ // for listing groups
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:user:pass
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);

		string uid = port_user[port];
		string msg = "";
		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				// int grp_size =
				if (groupId_owner.size() == 0)
					cout << "No groups exist\n";

				else
				{
					for (auto i : groupId_owner)
						msg += i.first + "\n";
				}
				// char buffer[MAX] = "Login first!!";
				write(sockfd, msg.c_str(), sizeof(msg));
			}
			else
			{
				char buffer[MAX] = "Login first!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	else if (s == 'g')
	{ // for accepting requests
		string acc_req = buff;
		acc_req = acc_req.substr(2); //ip:port:user:pass
		// cout<<cr_acc<<"\n";
		stringstream check1(acc_req);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string req_uid = tokens[2]; // accept request for this uid
		string gid = tokens[3];
		string uid = port_user[port];

		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				if (groupId_owner.find(gid) != groupId_owner.end())
				{
					/*
					handle case when uid is not owner of gid
					*/
					groupId_users[gid].push_back(req_uid);
					peer_details[req_uid].groupId[gid] = 0;
					char buffer[MAX] = "Group request accepted!!";
					write(sockfd, buffer, sizeof(buffer));
				}
				else
				{
					char buffer[MAX] = "GroupId does not exist!!";
					write(sockfd, buffer, sizeof(buffer));
				}
			}
			else
			{
				char buffer[MAX] = "Login to join group!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	else if (s == 'h')
	{ // for leaving goup
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:gid
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string gid = tokens[2];

		string uid = port_user[port];
		// string uid_of_group_owner = groupId_owner[gid];
		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				if (groupId_owner.find(gid) != groupId_owner.end())
				{
					string owner_id = groupId_owner[gid];
					if (owner_id == uid)
					{
						cout << "uid is owner\n";
						groupId_owner.erase(gid);
						groupId_users.erase(gid);

						for (auto i : peer_details)
						{
							if (((i.second).groupId).find(gid) != ((i.second).groupId).end())
							{
								cout << i.first << endl;
								(i.second).groupId.erase(gid);
							}
						}
					}
					else
					{
						bool flag = false;
						int count = 0;
						for (auto i : groupId_users[gid])
						{
							if (i == uid)
							{
								flag = true;
								break;
							}
							count++;
						}
						if (flag == true)
						{
							groupId_users[gid].erase(groupId_users[gid].begin() + count);
						}
						else
						{
							char buffer[MAX] = "User does not belong to this groupId!!\n";
							write(sockfd, buffer, sizeof(buffer));
							tokens.clear();
							return NULL;
						}

						peer_details[uid].groupId.erase(gid);
					}
					// string owner_ip = peer_details[owner_id].cip;
					// int owner_port = peer_details[owner_id].cport;
					// string msg = uid + ":" + owner_ip + ":" + to_string(owner_port)+"\0";
					string msg = "U have left the group\n";
					char buffer[MAX];
					// = msg.c_str	();
					strcpy(buffer, msg.c_str());
					cout << msg;
					write(sockfd, buffer, sizeof(buffer));
				}
				else
				{
					char buffer[MAX] = "GroupId does not exist!!";
					write(sockfd, buffer, sizeof(buffer));
				}
			}
			else
			{
				char buffer[MAX] = "Login to leave group!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	else if (s == 'i')
	{ // for uploading file
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:filepath:gid
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string filepath = tokens[2];
		string gid = tokens[3];
		string uid = port_user[port];
		string inFile_size = tokens[4];
		string sha = tokens[5];

		string filename = "";
		for (int i = filepath.length() - 1; i >= 0; i--)
		{
			if (filepath[i] != '/')
			{
				filename = filepath[i] + filename;
			}
			else
			{
				break;
			}
		}
		// handle when repeated upload by same uid same file

		// cout << "size of vector : " << groupId_users[gid].size() << endl;

		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				if (groupId_owner.find(gid) != groupId_owner.end())
				{
					if (find(groupId_users[gid].begin(), groupId_users[gid].end(), uid) != groupId_users[gid].end())
					{

						struct file grp_file_obj;
						grp_file_obj.filepath = filepath;
						grp_file_obj.fileSize = inFile_size;
						grp_file_obj.gid = gid;
						grp_file_obj.uid = uid;
						grp_file_obj.fileName = filename;
						grp_file_obj.sha = sha;
						group_file_details[gid].push_back(grp_file_obj);
						// file_hash_size[filename] = {sha, inFile_size};
						// if(find(clients_having_file[sha].begin(), clients_having_file[sha].end(), {uid, gid}) == clients_having_file[sha].end()){
						bool flag = false;
						for (auto i : clients_having_file[sha])
						{
							if (i.first == uid)
							{
								char buffer[MAX] = "file already uploaded by this uid\n";
								write(sockfd, buffer, sizeof(buffer));
								flag = true;
								break;
							}
						}
						if (flag == false)
						{
							char buffer[MAX] = "file adding to client list\n";
							clients_having_file[sha].push_back({uid, gid});
							write(sockfd, buffer, sizeof(buffer));
						}
						// char buffer[MAX] = "File Uploaded successful!!";
					}
					else
					{
						char buffer[MAX] = "You are not part of this group!!";
						write(sockfd, buffer, sizeof(buffer));
					}
				}
				else
				{
					char buffer[MAX] = "GroupId does not exist!!";
					write(sockfd, buffer, sizeof(buffer));
				}
			}
			else
			{
				char buffer[MAX] = "Login to upload file!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	else if (s == 'j')
	{ // for listing file in a group
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:filepath:gid
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string gid = tokens[2];
		string uid = port_user[port];

		string fileList = "";
		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				if (groupId_owner.find(gid) != groupId_owner.end())
				{
					set<string> s;
					for (auto i : group_file_details[gid])
						// fileList += i.fileName +"\n";
						s.insert(i.fileName);

					for (auto i : s)
					{
						fileList += (i + "\n");
					}
					// char buffer[MAX] = fileList;
					write(sockfd, fileList.c_str(), sizeof(fileList));
				}
				else
				{
					char buffer[MAX] = "GroupId does not exist!!";
					write(sockfd, buffer, sizeof(buffer));
				}
			}
			else
			{
				char buffer[MAX] = "Login to get List of files!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}
	else if (s == 'l')
	{ // for stop sharing
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:user:pass
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate); //

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string gid = tokens[2];
		string filename = tokens[3];
		string sha;
		if (group_file_details.find(gid) != group_file_details.end())
		{
			for (auto i : group_file_details[gid])
			{
				if (i.fileName == filename)
				{
					sha = i.sha;
				}
			}
		}

		string uid = port_user[port];
		string stop_sharing_response;
		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				if (groupId_owner.find(gid) != groupId_owner.end())
				{
					if (find(groupId_users[gid].begin(), groupId_users[gid].end(), uid) != groupId_users[gid].end())
					{
						int count = 0, flag = 0;
						for (auto i : clients_having_file[sha])
						{
							if (i.first == uid)
							{
								clients_having_file[sha].erase(clients_having_file[sha].begin() + count);
								cout << "You have deleted your sharing from tracker" << endl;
								stop_sharing_response = "s";
								flag = 1;
								break;
							}
							count++;
						}
						if (flag == 0)
						{
							cout << "You done have file to stop share" << endl;
							stop_sharing_response = "f";
						}

						char buffer[MAX];
						// = msg.c_str	();
						strcpy(buffer, stop_sharing_response.c_str());
						write(sockfd, buffer, sizeof(buffer));
					}
					else
					{
						char buffer[MAX] = "You are not part of this group!!";
						write(sockfd, buffer, sizeof(buffer));
					}
				}
				else
				{
					char buffer[MAX] = "GroupId does not exist!!";
					write(sockfd, buffer, sizeof(buffer));
				}
			}
			else
			{
				char buffer[MAX] = "Login to join group!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}
	else if (s == 'k')
	{ // for downloading file
		string cr_acc = buff;
		cr_acc = cr_acc.substr(2); //ip:port:user:pass
		// cout<<cr_acc<<"\n";
		stringstream check1(cr_acc);
		string intermediate;
		while (getline(check1, intermediate, ':'))
			tokens.push_back(intermediate);

		// for(int i = 0; i < tokens.size(); i++)
		// 	cout << tokens[i] << '\n';
		string peer_ip = tokens[0];
		int port = stoi(tokens[1]);
		string gid = tokens[2];
		string filename = tokens[3];
		string sha, filesize;
		string filepath;
		if (group_file_details.find(gid) != group_file_details.end())
		{
			for (auto i : group_file_details[gid])
			{
				if (i.fileName == filename)
				{
					sha = i.sha;
					filesize = i.fileSize;
					filepath = i.filepath;
				}
			}
		}

		string uid = port_user[port];
		string download_response;
		if (peer_details.find(uid) != peer_details.end())
		{
			if (peer_details[uid].isloggedIn == true)
			{
				if (groupId_owner.find(gid) != groupId_owner.end())
				{
					if (find(groupId_users[gid].begin(), groupId_users[gid].end(), uid) != groupId_users[gid].end())
					{
						for (auto i : clients_having_file[sha])
						{
							if (i.first != uid)
							{
								if (peer_details[i.first].isloggedIn)
									download_response += (to_string(peer_details[i.first].cport) + ":");
							}
						}

						download_response += (sha + ":" + filesize + ":" + filepath);
						char buffer[MAX];
						// = msg.c_str	();
						strcpy(buffer, download_response.c_str());
						write(sockfd, buffer, sizeof(buffer));
					}
					else
					{
						char buffer[MAX] = "You are not part of this group!!";
						write(sockfd, buffer, sizeof(buffer));
					}
				}
				else
				{
					char buffer[MAX] = "GroupId does not exist!!";
					write(sockfd, buffer, sizeof(buffer));
				}
			}
			else
			{
				char buffer[MAX] = "Login to join group!!";
				write(sockfd, buffer, sizeof(buffer));
			}
		}
		else
		{
			char buffer[MAX] = "UserId does not exist!!";
			write(sockfd, buffer, sizeof(buffer));
		}
		tokens.clear();
	}

	return NULL;
}

void *funcForQuit(void *arg)
{
	string quit;
	cin >> quit;
	if (quit == "quit")
		exit(0);
	return NULL;
}