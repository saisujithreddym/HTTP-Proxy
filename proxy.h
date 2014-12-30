#ifndef PROXYSERVER_H
#define PROXYSERVER_H

#include "network.h"


#define HTTP_GET		 		"GET "
#define HTTP_HOST_HEADER 		"Host: "
#define HTTP_PORT 				"80"

#define HEADER_304_1_0			"HTTP/1.0 304 Not Modified"
#define HEADER_304_1_1			"HTTP/1.1 304 Not Modified"
#define HEADER_200_1_0			"HTTP/1.0 200 OK"
#define HEADER_200_1_1			"HTTP/1.1 200 OK"

#define HEADER_ETAG1 			"Etag: "
#define HEADER_ETAG2 			"ETag: "
#define HEADER_IF_MODIFIED 	    "If-Modified-Since: "
#define HEADER_IF_MATCH 		"If-None-Match: "



using namespace std;
typedef struct Client_Info
{
	int client_sock;
	int host_sock;
	string http_path;
	char http_host[URL_SIZE];	
} Client_Info;

typedef struct DataBlock
{
	char data[RESPONSE_SIZE];
	int size;
} DataBlock;

typedef struct DocEntity
{
	string path;
	vector< DataBlock* > blocks;
	char ExpiresTimeString[EXPIRE_SIZE];
	time_t LastModified, ExpiresTimeT;
	char etag[ETAG_SIZE];
	bool IfCondition;;
} DocEntity;

typedef struct LruCache
{
	list<string> path_list;
	map<string, DocEntity*> PathDocMap;
} LruCache;



bool ExtractHeader(const char* header, char* buf, char* output);
time_t ExtractTime(const char* header, char* buf);
time_t GetTime();

void InitialDocEntity(string url_in,DocEntity* DocPointer);
void AddBlock(char* buf, int nbytes,DocEntity* DocPointer);
void ExtractTimeInfo(char* buf, const int nbytes,DocEntity* DocPointer);
void DeleteBlocks(DocEntity* DocPointer);

bool AddDoc(DocEntity* DocPointer,LruCache *server_cache);
DocEntity* GetDocPointer(string path,LruCache *server_cache);
bool DeleteEntry(string path,LruCache *server_cache);

vector<Client_Info*>::iterator FindClientInfo(int sock);
void *get_in_addr(struct sockaddr *sa);
int CloseConnect(Client_Info* client, fd_set& master);
int ExtractPathHost(const char* buf, string& http_path,char* http_host);
int ConnectHost(char* httphost);
void UpdateCache(Client_Info* client, char* buf, int nbytes);
bool ConditionalData(DocEntity* DocPointer, char* buf, int& nbytes);


#endif

