#include "network.h"

#define HTTP_STR		 		"http://"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int GetInfo(char* url, char* req_buf, char* filename)
{

    int len,total_len;
	char* char_pointer = strstr(url, HTTP_STR);
	if (char_pointer) url += strlen(HTTP_STR);

	char* end_pointer = strchr(url, '/');

	if(end_pointer)
	{
		total_len=strlen(end_pointer);
		//printf("total_len is %d\n",total_len);
		if(total_len>1)
		{
			int i;
			for(i=total_len-1;i>=0;i--)
			{
				if (*(end_pointer+i)=='/')
					break;
			}
			//filename=end_pointer+i+1;
			sprintf(filename, "%s", end_pointer+i+1);
			//printf("filename is:%s\n",filename);
		}
		char http_host[URL_SIZE];
		strncpy(http_host, url, end_pointer - url);
		http_host[end_pointer - url] = '\0';

		printf("packet:GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", end_pointer, http_host);
		len=sprintf(req_buf, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", end_pointer, http_host);
		return 1+len;
	}
	sprintf(filename, "%s",url);
        strcat(filename,".html");
	len=sprintf(req_buf, "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", url); 
	return 1+len;
	
}

int main(int argc, char *argv[])
{
	
	int sockfd, nbytes,rv;
	char url[URL_SIZE];
	char request_buf[REQ_SIZE];	 
	char filename[50];
	char recv_buf[RESPONSE_SIZE];
	char proxy_server[INET6_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;
	

	if (argc != 4) 
	{
		printf("usage: ./client <proxy server ip> <proxy server port> <url>\n");
		return -1;
	}

	strcpy(url, argv[3]);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0)
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
		fprintf(stderr, "failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),proxy_server, sizeof proxy_server);
	printf("connecting to %s\n", proxy_server);

	freeaddrinfo(servinfo); // all done with this structure
    memset(filename, 0, 50);
	
	nbytes = GetInfo(url, request_buf,filename);
	printf("filename is:%s\n",filename);
	FILE *ostream;
	if(*filename)
	{
		 if((ostream= fopen(filename,"wb"))==NULL)
		 {
			 printf("open error!\n");
			 return -1;
		 }
	}	
	if (send(sockfd, request_buf, nbytes, 0) == -1)
	{
		perror("send");
	}

	printf("Response from proxy_server:\n");

	int block_no = 0;
	size_t bytes_no = 0;
	int IfHeader=1;
	char* str_pointer;
	do
	{
		nbytes = recv(sockfd, recv_buf, RESPONSE_SIZE-1 , 0);
		if (nbytes == -1)
		{
			perror("recv");
			exit(1);
		}
		//printf("nbytes is %d\n", nbytes);

		recv_buf[nbytes] = '\0';		
		bytes_no += nbytes;
		//printf("bytes_no is %d\n", bytes_no);
		block_no++;
		
		if(*filename)
		{
			if(IfHeader)
			{
				//printf("nbytes is %d\n", nbytes);
				//printf("bytes_no is %d\n", bytes_no);
				str_pointer = strstr(recv_buf, "\r\n\r\n");
				if (str_pointer) 
					str_pointer += strlen("\r\n\r\n");
				fwrite(str_pointer,1,nbytes-(str_pointer-recv_buf),ostream);					
				IfHeader=0;
				
				//printf("%s", str_pointer );
			}
			else
			{
				fwrite(recv_buf,1,nbytes,ostream);
				
			}
		}
	} while(nbytes);
	if(*filename)
		{
			fclose(ostream);
		}

	close(sockfd);
	printf("\nclient received %d blocks which are total %lu bytes\n", block_no,bytes_no);
	
	return 0;
}


