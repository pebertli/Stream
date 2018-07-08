#include "stdafx.h"
#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <string>
#include <cassert>
#include <iostream>
#include <fstream>
#include <time.h>
#include "Message.cpp"
//#include <types/string.hpp>
//#include <archives/binary.hpp>

#define BUFSIZE 4096

void FlushSync();
void WriteSync(int option, std::string m, void* obj);
Message ReadSync();
void WriteAsync(int option, std::string m, void* obj);
bool ReadAsync();
std::string SerializeMessage(int option, std::string message, void* obj);
Message UnserializeMessage(std::string buf);
Circle RandomizeCircle();

//Callbacks for async pipe calls
void WINAPI CompletedReadRoutine(DWORD errorCode, DWORD bytesCopied, LPOVERLAPPED overlapped);
void WINAPI CompletedWriteRoutine(DWORD errorCode, DWORD bytesCopied, LPOVERLAPPED overlapped);

//Overlap structure for async pipe
typedef struct
{
	OVERLAPPED oOverlap;
	HANDLE hPipeInst;
	char bufferRead[BUFSIZE];
	DWORD cbRead;
	std::string chReply;
	DWORD cbToWrite;
} PIPEINST, *LPPIPEINST;

LPPIPEINST lpPipeInst;

HANDLE pipe;
std::string buffer;
DWORD status = ERROR_SUCCESS;

bool isConnected = false;

int main()
{


	//keep trying to connect
	while (!isConnected)
	{
		//open the pipe with overlapped enabled
		pipe = CreateFile(L"\\\\.\\pipe\\streamlabs", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
		if (INVALID_HANDLE_VALUE == pipe)
		{
			// The pipe is not accessible.
			status = ::GetLastError();
			std::cout << "Server pipe not found. Trying again in 3 seconds" << std::endl;
			Sleep(3000);
		}
		else
		{
			std::cout << "Server pipe connected" << std::endl;
			isConnected = true;
		}
	}
	//overlap structure memory allocation
	lpPipeInst = (LPPIPEINST)GlobalAlloc(GPTR, sizeof(PIPEINST));
	if (lpPipeInst == NULL)
	{
		return 0;
	}
	lpPipeInst->hPipeInst = pipe;

	//Menu
	std::string choices = "Write an option: 1 - Server Ping; 2 - Write a private message; 3 - Wrire a public message\n";	
	int option = 0;	
	do
	{
		//ask for another
		std::cout << choices;
		std::cin >> option;

		switch (option)
		{
		case 0: //Bye
		{
			std::cout << "Closing...";
			Sleep(2000);
			return 0;
			break;
		}
		case 1: //Write and read Sync
		{
			std::cout << "Write a message to the server" << std::endl;
			std::string str = "";
			std::cin >> str;
			FlushSync();
			WriteSync(option, str, NULL);
			Message message = ReadSync();
			std::cout << "Server Response: " << message.message << std::endl;
			break;
		}
		case 2: //Write Async. Server will simulate loading
		{
			FlushSync();
			WriteAsync(option, "Write Async. Server will simulate loading", NULL);
			break;
		}
		case 3: //Continuous reading Async for 8 seconds
		{
			ReadAsync();
			break;
		}
		case 4: //Send a random circle
		{
			FlushSync();
			Circle c = RandomizeCircle();
			std::string s = "Save the circle in the server: ";
			s.append(c.toString());
			WriteAsync(option, s, &c);
			break;
		}
		case 5: //Receive a previous circle
		{
			FlushSync();
			WriteSync(option, "Request the last client circle from the server", NULL);
			Message message = ReadSync();
			if (message.circle.colour.empty())
				std::cout << "Server returns the message " << message.message << std::endl;
			else
				std::cout << "Server returns the circle " << message.circle.toString() << std::endl;
			break;
		}
		case 6: //Call a server circle procedure
		{
			FlushSync();
			Circle c = RandomizeCircle();
			WriteAsync(option, "Save the circle and call the procedure", &c);
			break;
		}
		case 7: //Write a public message
		{
			std::cout << "Write a message to the server, available to all clients" << std::endl;
			std::string str = "";
			std::cin >> str;
			FlushSync();
			WriteAsync(option, str, NULL);
			break;
		}
		case 8: //Receive a public message
		{
			FlushSync();
			WriteSync(option, "Request the public message", NULL);
			Message message = ReadSync();
			std::cout << "The public message is: " << message.message << std::endl;
			break;
		}
		default:
			break;
		}		
	}
	while (option != 0);

	_getch();

	return status;
}

//Write on the pipe synchronally
void WriteSync(int option, std::string m, void* obj)
{	
	//generate a package to send through the pipe	
	std::string localBufferW = Message::Marshalling(option, m, obj);
	DWORD numBytesWritten = 0;
	//Sync function
	bool resultW = WriteFile(pipe, localBufferW.c_str(), localBufferW.length() + 1, &numBytesWritten, NULL);
	if (!resultW)
	{
		std::cout << "Error sending the request" << std::endl;
	}
}

//Read from the pipe synchronally
//Blocks the thread even if there is nothing on the pipe to be readed
Message ReadSync()
{
	Message m;
	DWORD numBytesRead = 0;
	char localBufferR[BUFSIZE];
	//sync function
	bool resultR = ReadFile(pipe, localBufferR, sizeof(lpPipeInst->bufferRead), &numBytesRead, NULL);

	if (resultR && numBytesRead != 0) {
		//trunc the vector of char
		std::string s(localBufferR, numBytesRead - 1);
		//turn the string (a bunch of bytes) in the Message pack		
		m = Message::Unmarshalling(s);

		if (m.command == 0)
			std::cout << "Nothing read from the pipe" << std::endl;
		else
			return m;
	}
	else
	{
		std::cout << "Error receiving the answer" << std::endl;
	}

	return m;
}

void FlushSync()
{
	DWORD avail;
	bool bSuccess = FALSE;
	bool tSuccess = FALSE;
	//read without remove the content of the pipe
	tSuccess = PeekNamedPipe(pipe, NULL, 0, NULL, &avail, NULL);
	////There is something in the pipe, then remove it
	if (tSuccess && avail > 0)
	{
		DWORD numBytesRead = 0;
		char localBufferR[BUFSIZE];
		//Sync function
		ReadFile(pipe, localBufferR, sizeof(lpPipeInst->bufferRead), &numBytesRead, NULL);
	}
}

//Write on the pipe asynchronally
//When finishes the writing task, call the callback CompletedWriteRoutine
void WriteAsync(int option, std::string m, void* obj)
{	
	//generate a package to send through the pipe	
	lpPipeInst->chReply = Message::Marshalling(option, m, obj);
	//async function. the last argument is an callback pointer
	bool res = WriteFileEx(pipe, lpPipeInst->chReply.c_str(), lpPipeInst->chReply.length() + 1, (LPOVERLAPPED)lpPipeInst, (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);

	DWORD bytesWritten = 0;
	if (res)
	{
		//get the result as an alertable wait
		GetOverlappedResultEx(pipe, (LPOVERLAPPED)lpPipeInst, &bytesWritten, INFINITE, TRUE);
	}
}

//Read from the pipe asynchronally
//When finishes the reading task, call the callback CompletedReadRoutine
bool ReadAsync()
{
	//async function. the last argument is an callback pointer. The result of read, if any, will be put in the bufferRead
	bool res = ReadFileEx(pipe, lpPipeInst->bufferRead, sizeof(lpPipeInst->bufferRead), (LPOVERLAPPED)lpPipeInst, (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);
	DWORD bytesRead = 0;
	if (res)
	{
		//get the result as an alertable wait
		GetOverlappedResultEx(pipe, (LPOVERLAPPED)lpPipeInst, &bytesRead, 8000, TRUE);
	}

	return res;
}

//callback for async write pipe operations 
void WINAPI CompletedWriteRoutine(DWORD errorCode, DWORD bytesCopied, LPOVERLAPPED overlapped)
{
	if (ERROR_SUCCESS == errorCode)
	{
		LPPIPEINST lpo = (LPPIPEINST)overlapped;		
		Message m = Message::Unmarshalling(lpo->chReply);
		//just show my own message
		std::cout << "Async Message sent to the pipe: " << m.message << std::endl;
	}
}

//callback for async read pipe operations 
void WINAPI CompletedReadRoutine(DWORD errorCode, DWORD bytesCopied, LPOVERLAPPED overlapped)
{
	if (ERROR_SUCCESS == errorCode)
	{
		LPPIPEINST lpo = (LPPIPEINST)overlapped;
		//trunc the vector of char
		std::string s(lpo->bufferRead, bytesCopied - 1);
		Message m = Message::Unmarshalling(s);
		//message received from the server
		std::cout << "Async Message received from the pipe: " << m.message << std::endl;		
	}
}

//auto create a random Circle with random attibutes
Circle RandomizeCircle()
{
	srand(time(NULL));
	int c = rand() % 3;
	std::string colour;
	switch (c)
	{
	case 0:
		colour = "White";
		break;
	case 1:
		colour = "Black";
		break;
	case 2:
		colour = "Blue";
		break;
	default:
		break;
	}

	return Circle(colour, (float)(rand() % 10 + 1), rand() % 10, rand() % 10);
}