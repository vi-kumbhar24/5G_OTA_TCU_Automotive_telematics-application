/*
* Configuration Manager C language Application Program Interface
*
* Entry points:
*    setMemoryMgt(void * (*myMalloc)(size_t), void(*myFree)(void *))
*       Optional memory management functions to replace malloc() and free()
*
*    KVPair * getValues(char * key, char * ip)
*       Returns KVPair structure linked list of key / value pairs for given key and IP address.
*       Search key may end in * to indicate wildcard search.
*
*    void freeKVList(KVPair * keyValueList)
*       Frees memory allocated for linked list.  It is the caller's responsibility to invoke
*       this function for every getValues call to prevent memory leaks.
*/
#ifndef CONFIG_API
#define CONFIG_API
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

using namespace std;

#define BUFFER_SIZE 20481

//  C language list element for a single key / value pair
struct kvPair{
	char * key;
	char * val;
	kvPair * next;
};
typedef struct kvPair KVPair;

//  Memory management functions
void * (*configAlloc)(size_t) = malloc;
void (*configFree)(void *) = free;

void setMemoryMgt(void * (*myMalloc)(size_t), void(*myFree)(void *))
{
	configAlloc = myMalloc;
	configFree = myFree;
}

//  Function to unpack serialized key / value pairs into a list
KVPair * unpackKeyValues(char * serializedString, KVPair * lastval)
{
	KVPair * retval = lastval;
	KVPair * lastKVPair = retval;
	while (lastKVPair && lastKVPair->next)
		lastKVPair = lastKVPair->next;
	char * remaining_string = serializedString;
	char * p = strtok_r(serializedString, "\r\n", &remaining_string);
	while (p != NULL)
	{
		if (strlen(p) == 0) continue;
		char * pos = strchr(p,'=');
		if (pos)
		{
			//  Allocate space for list element
			KVPair * newKVPair = (KVPair *) configAlloc(sizeof(KVPair));
			newKVPair->next = NULL;
			if (retval == NULL)
				retval = newKVPair;
			else
				lastKVPair->next = newKVPair;
			lastKVPair = newKVPair;

			//  Save key and value
			int keyLen = pos - p;
			int valLen = strlen(pos + 1);
			newKVPair->key = (char *) configAlloc(keyLen + 1);
			newKVPair->val = (char *) configAlloc(valLen + 1);
			strncpy(newKVPair->key, p, keyLen);
			strncpy(newKVPair->val, pos+1, valLen);
			newKVPair->key[keyLen] = '\0';
			newKVPair->val[valLen] = '\0';
		}
		p = strtok_r(NULL, "\r\n", &remaining_string);
	}
	return retval;
}


//  Function to return key / value pairs for a given key 
KVPair * getValues(const char * key, const char * ip)
{
	int sockfd = 0, n = 0;
	char recvBuff[BUFFER_SIZE];
	struct sockaddr_in serv_addr;

	KVPair * retval = NULL;

	//  Open socket to configuration manager
	memset(recvBuff, '0', sizeof(recvBuff));
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Config Manager error : Could not create socket \n");
		return retval;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(50000);  //  Hard coded socket since not configurable

	if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
	{
		printf("\n Config Manager inet_pton error occured\n");
		return retval;
	}

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\n Config Manager error : Connect Failed \n");
		return retval;
	}

	//  Send request to configuration manager service
	n = write(sockfd, key, strlen(key));
	if (n <= 0) {
		printf("\n Config Manager error : writing to socket\n");
		return retval;
	}

	//  Read response
	if ((n = read(sockfd, recvBuff, sizeof(recvBuff) - 1)) > 0)
	{
		recvBuff[n] = 0;
		while (n > 0 && recvBuff[n-1] == '\x01D')  //  More config data pending CONLAREINS-556
		{
			write(sockfd, key, strlen(key));
			recvBuff[n-1] = 0;
			retval = unpackKeyValues(recvBuff, retval);
			n = read(sockfd, recvBuff, sizeof(recvBuff) - 1);
			recvBuff[n] = 0;
		}
		//  Parse serialized string into key, value pairs
		if (n > 0) retval = unpackKeyValues(recvBuff, retval);
	}

	else
	{
		printf("\n Config Manager read error \n");
	}

	return retval;
}

//  Function to free allocated memory for key / value pairs
void freeKVList(KVPair * keyValueList)
{
	while (keyValueList != NULL)
	{
		configFree(keyValueList->key);
		configFree(keyValueList->val);
		KVPair * thsKVPair = keyValueList;
		keyValueList = keyValueList->next;
		configFree(thsKVPair);
	}
}
#endif