#pragma comment(lib, "WS2_32.lib")

#include <stdio.h>
#include <winsock2.h> 
#include <windows.h>
#include <string>
#include <conio.h>
#include <iostream>
#include <process.h>

#include "gost89.h"

#define VLCADDRESS "192.168.0.10"
#define CAMADDRESS "192.168.0.54"
#define ALPHA_ADDRESS "192.168.0.11"
#define OMEGA_ADDRESS "192.168.0.12"

#define SERVPORT 554
#define OMEGA_PORT 666

//‘ункци€, производ€ща€ замену адресов
void SubtitionBuffer(char * buf,const char * initial, const char* obtained);

//‘ункци€, достающа€ из RTSP сообщени€ камеры порт клиента и сервера
int ParsePorts(char *buf,USHORT *client_port, USHORT *server_port); // 0 - OK

//обмен видеоданными по UDP
void UDPthread1 (void* port);
void UDPthread2 (void* port);

using namespace std;

int main()
{

	WSADATA *wsadata = new WSADATA;
	if (WSAStartup(0x0202,(WSADATA *) wsadata)) 
	{
		// ќшибка!
		printf("Error WSAStartup %d\n",WSAGetLastError());
		_getch();
		return -1;
	}
	//ѕќƒ Ћё„ј≈ћ  јћ≈–”
	printf("CAMERA...");
	SOCKET CLIENT;
	CLIENT=socket(AF_INET,SOCK_STREAM,0);
	if (CLIENT < 0)
	{
		printf("Socket() error %d\n",WSAGetLastError());
		return -1;
	}

	sockaddr_in dest_addr;
	dest_addr.sin_family=AF_INET;
	dest_addr.sin_port=htons(SERVPORT);
	dest_addr.sin_addr.s_addr=inet_addr(CAMADDRESS);

	// адрес сервера получен Ц пытаемс€ установить соединение 
	if (connect(CLIENT,(sockaddr *)&dest_addr,sizeof(dest_addr)))
	{
		printf("connect error %d\n",WSAGetLastError());
		getc(stdin);
		return -1;
	}
	printf("connected OK\n");

	//—оедин€емс€ с ALPHA
	printf("ALPHA...");
	SOCKET SERVER;
	SOCKET SERVERclient;
	if ((SERVER=socket(AF_INET,SOCK_STREAM,0))<0)
	{
		// ќшибка!
		printf("Error socket %d\n",WSAGetLastError());
		WSACleanup();
		getc(stdin);
		return -1;
	}
	sockaddr_in local_addr;
	local_addr.sin_family=AF_INET;
	local_addr.sin_port=htons(OMEGA_PORT);
	local_addr.sin_addr.s_addr=0;
	// —в€зываем сокет со структурой сокета
	if (bind(SERVER,(sockaddr *) &local_addr,sizeof(local_addr)))
	{
		// ќшибка
		printf("Error bind %d\n",WSAGetLastError());
		closesocket(SERVER);  // закрываем сокет!
		WSACleanup();
		getc(stdin);
		return -1;
	}

	if (listen(SERVER, 0x100)==SOCKET_ERROR)
	{
		// ќшибка
		printf("Error listen %d\n",WSAGetLastError());
		closesocket(SERVER);
		WSACleanup();
		getc(stdin);
		return -1;
	}
	//создание сокета клиента дл€ —≈–¬≈–ј
	sockaddr_in SERVERclient_addr; 
	int SERVERclient_addr_size=sizeof(SERVERclient_addr);
	SERVERclient=accept(SERVER, (sockaddr *)&SERVERclient_addr, &SERVERclient_addr_size);
	//ALPHA присоединилс€
	printf("connected OK\n");

	int bytes_recv;
	USHORT client_port, server_port;
	char buff[8*1024];
	char cipher[8*1024];
	gost_ctx c;
	gost_init(&c,&GostR3411_94_TestParamSet);
	unsigned char key[32];
	for (int i=0; i< 32; i++) key[i]=i;
	gost_key(&c,&key[0]);

	for (int i=1; i<= 5; i++)
	{
		//получаем с ALPHA
		bytes_recv=recv(SERVERclient,&cipher[0],sizeof(cipher),0);
		//дешифруем
		gost_dec(&c,(const byte*)cipher,(byte*)buff, bytes_recv / 8);
		printf("%d. FROM VLC\n%s\n\n",i,buff);
		//производим замену адресов
		//ALPHA->OMEGA OMEGA->CAM
		SubtitionBuffer(buff,OMEGA_ADDRESS,CAMADDRESS);
		SubtitionBuffer(buff,OMEGA_ADDRESS,CAMADDRESS);
		SubtitionBuffer(buff,ALPHA_ADDRESS,OMEGA_ADDRESS);
		SubtitionBuffer(buff,ALPHA_ADDRESS,OMEGA_ADDRESS);
		printf("FROM VLC(double)\n%s\n\n",buff);
		//отправл€ем камере
		send(CLIENT,&buff[0],bytes_recv,0);

		//получаем с камеры
		bytes_recv=recv(CLIENT,&buff[0],sizeof(buff),0);
		buff[bytes_recv] = 0;
		//производим замену адресов
		//CAM->OMEGA OMEGA->ALPHA
		printf("%d. FROM CAMERA\n%s\n\n",i,buff);
		if (i == 3) 
			if (ParsePorts(&buff[0],&client_port,&server_port)!=0) printf("CANT PARSE PACKET\n");
		SubtitionBuffer(buff,OMEGA_ADDRESS,ALPHA_ADDRESS);
		SubtitionBuffer(buff,OMEGA_ADDRESS,ALPHA_ADDRESS);
		SubtitionBuffer(buff,CAMADDRESS,OMEGA_ADDRESS);
		SubtitionBuffer(buff,CAMADDRESS,OMEGA_ADDRESS);
		printf("FROM CAMERA(double)\n%s\n\n",buff);
		//шифруем
		for (int j=bytes_recv; j<bytes_recv+(8-(bytes_recv % 8)); j++)
		{
			buff[j] = 0;
		}
		bytes_recv=bytes_recv+(8-(bytes_recv % 8));
		gost_enc(&c,(const byte*)buff,(byte*)cipher,bytes_recv / 8);
		//отправл€ем ALPHA
		send(SERVERclient,&cipher[0],bytes_recv,0);
		if (i == 4)
		{
			unsigned int ports = (client_port << 16)|server_port;
			_beginthread( UDPthread1, 0, (void*)ports );
			_beginthread( UDPthread2, 0, (void*)ports );
		}
		/*if (i == 5)
		{
		_endthread();
		}*/
	}

	printf("Press enter to exit");
	getc(stdin);
	return 0;
}

void SubtitionBuffer(char * buf,const char * initial, const char* obtained)
{
	int index=0,i;
	if (strstr(buf,initial) != NULL)
	{
		int pos = (int)(strstr(buf,initial)-&buf[0]);
		//printf("%d\n",pos);
		//int i= sizeof(strstr(buf,initial)-buf[0]);
		for (i = pos; i<=pos+11;i++)
		{
			buf[i]=obtained[index];
			index++;
		}
	}
}

int ParsePorts(char *buf,USHORT *client_port, USHORT *server_port) // 0 - OK
{
	string str(buf);
	int pos = str.find("client_port=");
	str.erase(0,pos+12);
	pos = str.find("-");
	string strclient = str.substr(0,pos);
	pos = str.find("server_port=");
	str.erase(0,pos+12);
	pos = str.find("-");
	string strserver = str.substr(0,pos);
	*client_port = (USHORT)stoi(strclient,0,10);
	*server_port = (USHORT)stoi(strserver,0,10);
	return 0;
}

void UDPthread1 (void* port)
{
	//сервер засел на client_port и слушает его, а клиент на server_port и отправл€ет с него. 
	//фактически, это два сервера. 
	unsigned int ports = (unsigned int) port;

	USHORT SERVERPORT = ports|0x0;
	USHORT CLIENTPORT = (ports >> 16)|0x0; 
	cout << SERVERPORT << '\n' << CLIENTPORT<< '\n';
	SOCKET UDP_SERVER_1 = socket(AF_INET, SOCK_DGRAM, 0);
	if (UDP_SERVER_1==INVALID_SOCKET)
	{
		printf("UDP socket error: %d\n",WSAGetLastError());
		WSACleanup();
		return;
	}
	sockaddr_in local_addr;
	local_addr.sin_family=AF_INET;
	local_addr.sin_addr.s_addr=INADDR_ANY;
	local_addr.sin_port=htons(CLIENTPORT);
	if (bind(UDP_SERVER_1,(sockaddr *) &local_addr,sizeof(local_addr)))
	{
		printf("UDP server bind error: %d\n",WSAGetLastError());
		closesocket(UDP_SERVER_1);
		WSACleanup();
		return;
	}

	SOCKET UDP_CLIENT_1 = socket(AF_INET, SOCK_DGRAM, 0);
	if (UDP_CLIENT_1==INVALID_SOCKET)
	{
		printf("UDP client error: %d\n",WSAGetLastError());
		WSACleanup();
		return;
	}
	local_addr.sin_port=htons(55000);
	if (bind(UDP_CLIENT_1,(sockaddr *) &local_addr,sizeof(local_addr)))
	{
		printf("UDP client bind error: %d\n",WSAGetLastError());
		closesocket(UDP_CLIENT_1);
		WSACleanup();
		return;
	}
	sockaddr_in dest_addr;
	dest_addr.sin_family=AF_INET;
	dest_addr.sin_port=htons(55001);
	dest_addr.sin_addr.s_addr=inet_addr(ALPHA_ADDRESS);

	char inbuf[8*1024];
	char cipher[8*1024];
	gost_ctx c;
	//инициализируем s-box
	gost_init(&c,&GostR3411_94_TestParamSet);
	unsigned char key[32];
	//тестовый ключ = {0,1,2,3,4,...,31}
	for (int i=0; i< 32; i++) key[i]=i;
	//инициализировали ключ
	gost_key(&c,&key[0]);

	while(true)
	{
		sockaddr_in client_addr;
		int client_addr_size = sizeof(client_addr);
		//получаем с камеры
		int bsize=recvfrom(UDP_SERVER_1,&inbuf[0],sizeof(inbuf),0,(sockaddr *) &client_addr, &client_addr_size);
		if (bsize==SOCKET_ERROR)
			printf("recvfrom() error: %d\n",WSAGetLastError());
		else
		{
			//ƒЋя Ў»‘–ќ¬јЌЌќ√ќ —ќ≈ƒ»Ќ≈Ќ»я:
			//первые 20 байт оставл€ем неизмененными: заголовки RTP и RFC-JPEG
			for (int i=0;i<20;i++) cipher[i] = inbuf[i];
			int mod = (8-((bsize-20) % 8)); 
			bsize=bsize+mod;
			gost_enc(&c,(const byte*)&inbuf[20],(byte*)&cipher[20],(bsize-20) / 8);
			cipher[bsize] = mod;
			//отправл€ем  Alpha
			sendto(UDP_CLIENT_1,&cipher[0],bsize+1,0,(sockaddr *) &dest_addr,sizeof(dest_addr));
			//ƒЋя ќ“ –џ“ќ√ќ —ќ≈ƒ»Ќ≈Ќ»я 
			//sendto(UDP_CLIENT_1,&inbuf[0],bsize,0,(sockaddr *) &dest_addr,sizeof(dest_addr));
		}
	}
	closesocket(UDP_CLIENT_1);
	closesocket(UDP_SERVER_1);
	return;
}

void UDPthread2 (void* port)
{
	//сервер засел на client_port и слушает его, а клиент на server_port и отправл€ет с него. 
	//фактически, это два сервера. 
	unsigned int ports = (unsigned int) port;

	USHORT SERVERPORT = (ports|0x0) + 1;
	USHORT CLIENTPORT = ((ports >> 16)|0x0) + 1; 
	SOCKET UDP_SERVER_2 = socket(AF_INET, SOCK_DGRAM, 0);
	if (UDP_SERVER_2==INVALID_SOCKET)
	{
		printf("UDP socket error: %d\n",WSAGetLastError());
		WSACleanup();
		return;
	}
	sockaddr_in local_addr;
	local_addr.sin_family=AF_INET;
	local_addr.sin_addr.s_addr=INADDR_ANY;
	local_addr.sin_port=htons(CLIENTPORT);
	if (bind(UDP_SERVER_2,(sockaddr *) &local_addr,sizeof(local_addr)))
	{
		printf("UDP server bind error: %d\n",WSAGetLastError());
		closesocket(UDP_SERVER_2);
		WSACleanup();
		return;
	}

	SOCKET UDP_CLIENT_2 = socket(AF_INET, SOCK_DGRAM, 0);
	if (UDP_CLIENT_2==INVALID_SOCKET)
	{
		printf("UDP client error: %d\n",WSAGetLastError());
		WSACleanup();
		return;
	}
	local_addr.sin_port=htons(55002);
	if (bind(UDP_CLIENT_2,(sockaddr *) &local_addr,sizeof(local_addr)))
	{
		printf("UDP client bind error: %d\n",WSAGetLastError());
		closesocket(UDP_CLIENT_2);
		WSACleanup();
		return;
	}
	sockaddr_in dest_addr;
	dest_addr.sin_family=AF_INET;
	dest_addr.sin_port=htons(55003);
	dest_addr.sin_addr.s_addr=inet_addr(ALPHA_ADDRESS);

	char inbuf[8*1024];
	char cipher[8*1024];
	gost_ctx c;
	gost_init(&c,&GostR3411_94_CryptoProParamSet);
	unsigned char key[32];
	for (int i=0; i< 32; i++) key[i]=i;
	gost_key(&c,&key[0]);

	while(true)
	{
		sockaddr_in client_addr;
		int client_addr_size = sizeof(client_addr);
		//получаем с камеры
		int bsize=recvfrom(UDP_SERVER_2,&inbuf[0],sizeof(inbuf),0,(sockaddr *) &client_addr, &client_addr_size);
		if (bsize==SOCKET_ERROR)
			printf("recvfrom() error: %d\n",WSAGetLastError());
		else
		{
			//ƒЋя Ў»‘–ќ¬јЌЌќ√ќ —ќ≈ƒ»Ќ≈Ќ»я:
			//первые 20 байт оставл€ем неизмененными: заголовки RTP и RFC-JPEG
			for (int i=0;i<20;i++) cipher[i] = inbuf[i];
			int mod = (8-((bsize-20) % 8)); 
			bsize=bsize+mod;
			gost_enc(&c,(const byte*)&inbuf[20],(byte*)&cipher[20],(bsize-20) / 8);
			cipher[bsize] = mod;
			//отправл€ем  Alpha
			sendto(UDP_CLIENT_2,&cipher[0],bsize+1,0,(sockaddr *) &dest_addr,sizeof(dest_addr));
			//ƒЋя ќ“ –џ“ќ√ќ —ќ≈ƒ»Ќ≈Ќ»я 
			sendto(UDP_CLIENT_2,&inbuf[0],bsize,0,(sockaddr *) &dest_addr,sizeof(dest_addr));
		}
	}
	closesocket(UDP_CLIENT_2);
	closesocket(UDP_SERVER_2);
	return;
}
