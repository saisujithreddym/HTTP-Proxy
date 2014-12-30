#include "proxy.h"

using namespace std;

LruCache *server_cache=new LruCache;
//LruCache server_cache;
vector <Client_Info*> client_list;


int main(int argc, char** argv)
{
	struct addrinfo hints, *servinfo, *p;
	fd_set master_set;    
	fd_set read_fds;  
	int fdmax;        // maximum number of file descriptor
	int listener;     // socket descriptor for listening	
    int rv,yes = 1;
	if(argc !=3)
	{
		printf("usage: ./proxy <ip to bind> <port to bind>\n");
		return -1;
	}
	FD_ZERO(&master_set);    // clear the master_set and temp sets
	FD_ZERO(&read_fds);

	// GetDocPointer us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) 
	{
		fprintf(stderr, "proxy server: %s\n", gai_strerror(rv));
		exit(1);
	}
	       
	for(p = servinfo; p != NULL; p = p->ai_next) 
	{
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
		{ 
			continue;
		}

		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) 
		{
			close(listener);
			continue;
		}
		break;
	}

	// if we got here, it means we didn't GetDocPointer bound
	if (p == NULL)
	{
		fprintf(stderr, "proxy server: failed to bind\n");
		exit(2);
	}
	freeaddrinfo(servinfo); // all done with this

	// listen
	if (listen(listener, 10) == -1) 
	{
		perror("listen");
		exit(3);
	}
	// add the listener to the master_set set
	FD_SET(listener, &master_set);
	// record maximum number of file descriptor
	fdmax = listener; // so far, it's this one

	// main loop
	for(;;)
	{
		read_fds = master_set; // copy it
		if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) 
		{
			perror("select");
			exit(4);
		}

		// run through the existing connections looking for data to read
		for(int fd = 1; fd <= fdmax; fd++)
		{
			if (FD_ISSET(fd, &read_fds)) 
			{
				if (fd == listener) // new client came and then handle a new connection
				{
					char remote_iP[INET6_ADDRSTRLEN];
					int new_fd;        
					struct sockaddr_storage remote_addr; // client address
					socklen_t addrlen;
					addrlen = sizeof remote_addr;
					if((new_fd = accept(listener,(struct sockaddr *)&remote_addr,&addrlen))==-1)
						perror("accept");					
					else 
					{
						FD_SET(new_fd, &master_set); // add to master_set 
						if (new_fd > fdmax) 
						{    // update maximum number of file descriptor
							fdmax = new_fd;
						}
						Client_Info* client = new Client_Info;
						client->host_sock = -1;
						client->client_sock = new_fd;						
						client_list.push_back(client);
						printf("\nNew client connected from address: %s on socket: %d\n",inet_ntop(remote_addr.ss_family,
							get_in_addr((struct sockaddr*)&remote_addr),remote_iP, INET6_ADDRSTRLEN),new_fd);
					}
				} 
				else // received request from existed client or received data from http host
				{
					char buf[RESPONSE_SIZE];    // to contain received data
					int nbytes;	
					if ((nbytes = recv(fd, buf, sizeof buf, 0)) <= 0) //recv error or connection closed 
					{	
						if (nbytes<0) perror("recv");
						vector<Client_Info*>::iterator it = FindClientInfo(fd);
						CloseConnect(*it, master_set);
						client_list.erase(it);
					} 
					else //successfully received data from existed client or received data from http host
					{
						Client_Info* client = *(FindClientInfo(fd));
						if(fd == client->client_sock)// Received data from client.
						{
							bool GetNew = true, IfCondition = false;							
							ExtractPathHost(buf, client->http_path,client->http_host);
							cout << "http_host: " << client->http_host << endl;
							cout << "http_path: " << client->http_path << endl;
							DocEntity *DocPointer = GetDocPointer(client->http_path,server_cache);
							if (DocPointer)//we have this http path in our server_cache
							{
								time_t now = GetTime();
								if (DocPointer->ExpiresTimeT>now)//the doc in server_cache is not expired
								{
									GetNew = false;
									cout << "DocEntity reusable! " << client->http_path << endl;
									vector < DataBlock*>:: iterator it;
									for ( it= DocPointer->blocks.begin(); it != DocPointer->blocks.end(); it++)
									{
										int sendsd=send(client->client_sock, (*it)->data, (*it)->size, 0);
										if (sendsd == -1)
										{
											perror("send");
										}
									}
									cout << "Requested data is sent from server_cache for: " << client->http_path << endl;
									vector<Client_Info*>::iterator itt = FindClientInfo(fd);
									CloseConnect(*itt, master_set);
									client_list.erase(itt);
									
								}
								else//the doc in server_cache may be expired
								{
									cout << "Requested data is in server_cache but maby be expired for: " << client->http_path << endl;
									//cout<<"old buf"<<buf<<endl;
									IfCondition = ConditionalData(DocPointer, buf, nbytes);
									//cout<<"new buf"<<buf<<endl;
									if(IfCondition)
									{
										DocPointer->IfCondition = true;
									}
									else
									{
										cout << "Deleting DocEntity for: " << client->http_path << endl;
										DeleteEntry(client->http_path,server_cache);										
									}
								}
							}
							if(GetNew)//we don't have the requested doc in our server_cache or the doc is expired
							{
								if (IfCondition)
									cout << "DocEntity may be expired for " << client->http_path << " Sending Conditional GET" << endl;								
								else
									cout << "DocEntity doesn't in the server_cache " << client->http_path<< " Forwarding request to host" << endl;
								if(client->host_sock == -1)//connect to the requested http host
								{
									cout << "http_host: " << client->http_host << endl;
									client->host_sock = ConnectHost(client->http_host);
									FD_SET(client->host_sock, &master_set); // add to master_set set
									if (client->host_sock > fdmax) 
										fdmax = client->host_sock;          //update the largest descriptor number
									
								}
								if (send(client->host_sock, buf, nbytes, 0) == -1)
								{
									perror("send");
								}
							}
						}
						else if (fd == client->host_sock)// Received data from requested http host.
						{
							UpdateCache(client, buf, nbytes);
							DocEntity *DocPointer = GetDocPointer(client->http_path,server_cache);
							if(DocPointer && (strstr(buf, HEADER_304_1_0) ||strstr(buf, HEADER_304_1_1)))
							{
								vector < DataBlock*>:: iterator it;
								for ( it = DocPointer->blocks.begin(); it != DocPointer->blocks.end(); it++)
								{
									if (send(client->client_sock, (*it)->data, (*it)->size, 0) == -1)
									{
										perror("send");
									}
								}
								cout << "requested data is not modified and sent from DocEntity: " << client->http_path << endl;
								vector<Client_Info*>::iterator itt = FindClientInfo(fd);
								CloseConnect(*itt, master_set);
								client_list.erase(itt);
							}
							else if (send(client->client_sock, buf, nbytes, 0) == -1)
							{
								perror("send");
							}
						}
					}
				}
			}
		}
	}
	return 0;
}




vector<Client_Info*>::iterator FindClientInfo(int sock)
{
	vector<Client_Info*>::iterator it;
	for(it = client_list.begin();it != client_list.end(); it++)
	{
		if ((*it)->client_sock == sock || (*it)->host_sock == sock)
			return it;
	}
	return client_list.end();
}

// GetDocPointer sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}



int CloseConnect(Client_Info* client, fd_set& master_set)
{
	if(client->client_sock != -1)
	{
		close(client->client_sock);
		FD_CLR(client->client_sock, &master_set); 
		printf("client_sock associated with client closed on socket %d\n", client->client_sock); 
	}
	if(client->host_sock != -1)
	{
		close(client->host_sock);
		FD_CLR(client->host_sock, &master_set); 
		printf("host_sock associated with client closed on socket %d\n", client->host_sock); 
	}
	delete client;
	return 0;
}


int ExtractPathHost(const char* buf, string& http_path,char* http_host)
{
	char rec_buf[REQ_SIZE];
	strcpy(rec_buf, buf);
	/*GetDocPointer the http host name*/
	char* host_pointer = strstr(rec_buf, HTTP_HOST_HEADER);
	host_pointer += strlen(HTTP_HOST_HEADER);
	char* end_pointer = strchr(host_pointer, ' ');
	if (!end_pointer) end_pointer = strchr(host_pointer, '\r');
    strncpy(http_host, host_pointer, end_pointer - host_pointer);
	http_host[end_pointer - host_pointer] = '\0';

    /*GetDocPointer the http path*/
	http_path.append(http_host);
	const char *ss = " \r\n";  
	char *path_pointer;	
	path_pointer = strtok(rec_buf, ss);

	if( path_pointer != NULL ) 
	{
		path_pointer = strtok(NULL, ss);
		http_path.append(path_pointer);
	}
	return 0;
}



int ConnectHost(char* httphost)
{
	int sockfd,rv;
	char host_name[INET6_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;	

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	if ((rv = getaddrinfo(httphost, HTTP_PORT, &hints, &servinfo)) != 0) 
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) 
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) 
		{
			perror("socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("connect");
			continue;
		}

		break;
	}

	if (p == NULL) 
	{
		fprintf(stderr, "failed to connect to host\n");
		return -1;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			host_name, sizeof host_name);
	printf("connecting to host: %s\n", host_name);
	freeaddrinfo(servinfo); // all done with this structure
	return sockfd;
}

void UpdateCache(Client_Info* client, char* buf, int nbytes)
{
	DocEntity* DocPointer = GetDocPointer(client->http_path,server_cache);
	//cout<<"DocPointer: "<<DocPointer<<endl;

	if (!DocPointer && (strstr(buf, HEADER_200_1_0) ||strstr(buf, HEADER_200_1_1)))
	{
		DocPointer = new DocEntity;
		InitialDocEntity(client->http_path,DocPointer);
		cout << "Caching " << client->http_path << endl;
		ExtractTimeInfo(buf, nbytes,DocPointer);
		AddDoc(DocPointer,server_cache);
	}
	// Check if this is respond for a conditional GET 
	if(DocPointer->IfCondition)
	{
		//cout<<"Check if this is respond for a conditional GET "<<endl;
		// Extract Info only when the message contains the HTTP header
		if(strstr(buf, HEADER_304_1_0) ||strstr(buf, HEADER_304_1_1))
		{
			cout<<"HTTP/1.0 304 Not Modified"<<endl;
			ExtractTimeInfo(buf, nbytes,DocPointer);
			return;
		}
		else if(strstr(buf, HEADER_200_1_0) ||strstr(buf, HEADER_200_1_1))
		{
			cout<<"HTTP/1.0 200 OK"<<endl;
			ExtractTimeInfo(buf, nbytes,DocPointer);
			DeleteBlocks(DocPointer);
			//cout<<"finish delete"<<endl;
		}
		DocPointer->IfCondition = false;
		//cout<<"after delete"<<endl;
	}
	//cout<<"before add"<<endl;
	AddBlock(buf, nbytes,DocPointer);	
}

bool ConditionalData(DocEntity* DocPointer, char* buf, int& nbytes)
{
	char* end = strstr(buf, "\r\n\r\n");

	if (!end || !DocPointer->ExpiresTimeT || !strlen(DocPointer->etag)) 
		return false;
	end += 2; 

	nbytes += sprintf(end, "%s%s\r\n%s%s\r\n\r\n", 
						   HEADER_IF_MODIFIED, DocPointer->ExpiresTimeString,
						   HEADER_IF_MATCH, DocPointer->etag);
	
	nbytes -= 2;
	return true;
}


void InitialDocEntity(string url_in,DocEntity* DocPointer)
	{
		memset(DocPointer->ExpiresTimeString, 0, EXPIRE_SIZE);
		memset(DocPointer->etag, 0, ETAG_SIZE);
		DocPointer->LastModified = 0;
		DocPointer->ExpiresTimeT = 0;
		DocPointer->IfCondition = false;
		DocPointer->path=url_in;
	}
void AddBlock(char* buf, int nbytes,DocEntity* DocPointer)
	{
		DataBlock *block_pointer = new DataBlock;
		//cout<<"block_pointer: "<<block_pointer<<endl;
		memcpy(block_pointer->data, buf, nbytes);
		block_pointer->size = nbytes;
		DocPointer->blocks.push_back(block_pointer);
		//cout<<"DocPointer: "<<DocPointer<<endl;
	}
void ExtractTimeInfo(char* buf, const int nbytes,DocEntity* DocPointer)
	{
		time_t t1,t2;
		t1 = ExtractTime("Expires:", buf);
		if (t1)			DocPointer->ExpiresTimeT = t1;
		t2 = ExtractTime("Last-Modified:", buf);
		if (t2)			DocPointer->LastModified = t2;
		if(	ExtractHeader(HEADER_ETAG1, buf, DocPointer->etag)==false) 
			ExtractHeader(HEADER_ETAG2, buf, DocPointer->etag);

		ExtractHeader("Expires:", buf, DocPointer->ExpiresTimeString);
		cout << "Expire Time: " << DocPointer->ExpiresTimeString<< endl;
		cout << "Etag: " << DocPointer->etag << endl;

	}
void DeleteBlocks(DocEntity* DocPointer)
	{
		//cout<<"delete DocPointer: "<<DocPointer<<endl;
		vector<DataBlock*>::iterator it;
		
		for (it = DocPointer->blocks.begin();it != DocPointer->blocks.end(); it++)
		{
			//cout<<"*it: "<<*it<<endl;
			delete *it;
			//cout<<"*it: "<<*it<<endl;
			//cout<<"delete"<<endl;
		}
        DocPointer->blocks.clear();
	}



bool AddDoc(DocEntity* DocPointer,LruCache *server_cache)
	{
		server_cache->path_list.push_front(DocPointer->path);
		server_cache->PathDocMap[DocPointer->path] = DocPointer;
		if(server_cache->path_list.size() <= DOC_ENTRY_NO) 
		{
			cout<<"not overflow"<<endl;
			return false;
		}
		string path = server_cache->path_list.back();
		cout << "DocEntity overflow. delete path: " << path << endl;
		// delete DocPointer data and delete references from the server_cache->PathDocMap and server_cache->path_list
		DeleteBlocks(server_cache->PathDocMap[path]);
		delete server_cache->PathDocMap[path];
		server_cache->PathDocMap.erase(path);
		server_cache->path_list.pop_back();
		return true;
	}
DocEntity* GetDocPointer(string path,LruCache *server_cache)
	{
		if (server_cache->PathDocMap.find(path) == server_cache->PathDocMap.end())
		return NULL;
		// record the path list with Least recently used method
		server_cache->path_list.splice(server_cache->path_list.begin(), server_cache->path_list, find(server_cache->path_list.begin(), server_cache->path_list.end(), path));
		return server_cache->PathDocMap[path];
	}
bool DeleteEntry(string path,LruCache *server_cache)
	{
		DeleteBlocks(server_cache->PathDocMap[path]);
		delete server_cache->PathDocMap[path];
		// Erase references from the map and list
		server_cache->PathDocMap.erase(path);
		server_cache->path_list.erase(find(server_cache->path_list.begin(), server_cache->path_list.end(), path));
		return true;
	}


bool ExtractHeader(const char* header, char* buf, char* output)
{
	char *start = strstr(buf, header);
	if(!start) return false;

	char *end = strstr(start, "\r\n");
	start += strlen(header);
	while(*start == ' ') ++start;
	while(*(end - 1) == ' ') --end;
	strncpy(output, start, end - start);
	output[end - start] = '\0';
	return true;
}



time_t ExtractTime(const char* header, char* buf)
{
	char TimeString[255] = {0};
	bool extract=ExtractHeader(header, buf, TimeString);
	if(!extract)
		return 0;

	struct tm TimeTm = {0};	
	char *TimeTmPointer=strptime(TimeString, "%A, %d %B %Y %H:%M:%S %Z", &TimeTm);
	if(!TimeTmPointer)
		return 0;
	else
		return mktime(&TimeTm);
}

time_t GetTime()
{
	time_t OriginalTime;
	time ( &OriginalTime );
	//cout << "OriginalTime: " << OriginalTime << endl;
	struct tm * TmPointer;
	TmPointer = gmtime ( &OriginalTime );
	//cout << "mktime(TmPointer): " << mktime(TmPointer) << endl;

	return mktime(TmPointer);
}
