

#Enable g++ complier
CPP = g++
# Set Wall 
CPPFLAGS=-Wall -g 

proxy: proxy.cpp proxy.h network.h client
	$(CPP) $(CPPFLAGS) proxy.cpp -o proxy -lrt
httpclient: client.cpp network.h
	$(CPP) $(CPPFLAGS) client.cpp -o client -lrt
clean:
	rm -rf *o httpclient proxy
