ECEN602 HW3 Programming Assignment 
(implementation of HTTP1.0 proxy server and client)
-----------------------------------------------------------------

Team Number: 17
Member 1 # Peng, Xuewei (UIN: 824000328)
Member 2 # Mankala, Sai Sujith Reddy  (UIN: 224002333)
---------------------------------------

Description/Comments:
--------------------
1. This package includes a simple HTTP proxy server and HTTP command line client.
2. The server will store requested document data in cache and can also check if the document expired in the cache. 
3. Clients can send HTTP request with explicit URL since it is based on HTTP-1.0

Architecture:
--------------------
The whole folder contains 4 files
1. network.h---This file includes the basic header files we use and defines global values, strings.
2. proxy.h--- This file defines structure to store client information, structure to store document and structure to store cache. 
              It also contains definition for functions to assist the proxy.cpp.
3. proxy.cpp---Implimentation the main function of the HTTP proxy server and the functions to assit the main function.
               Includes receive HTTP request from client, send HTTP request to HTTP host, receive HTTP host data,
	       store or updata the HTTP host data to cache, send requested data to client from cache or from HTTP host.
4. client.cpp---Implimentation to send HTTP request to the HTTP proxy server and receive the responding data sent by HTTP proxy.

Usage:
--------------------
Unix command for starting proxy server:
------------------------------------------
./proxy PROXY_SERVER_IP PROXY_SERVER_PORT

Unix command for starting client:
----------------------------------------------
./client PROXY_SERVER_IP PROXY_SERVER_PORT URL



Notes from your TA:
-------------------
1. While submitting, delete all the files and submit HTTP1.0 proxy server, client files along with makefile and readme.txt to Google Drive folder shared to each team.
2. Don't create new folder for submission. Upload your files to teamxx/HW3.
3. Do not make readme.txt as a google document. Keep it as a text file.
4. I have shared 'temp' folder for your personal (or team) use. You can do compilation or code share among team members using this folder.
5. Make sure to check file transfer between different machines. While grading, I'll use two different machines (lab 213A, Zachry) for your proxy server and client.
6. This is a standard protocol, your client should be compatible with proxy server of other teams. Your proxy server should be compatible with clients of other teams.

All the best. 

