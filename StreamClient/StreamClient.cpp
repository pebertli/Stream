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
#include <types/string.hpp>
#include <archives/binary.hpp>

#define BUFSIZE 4096

void FlushSync();
void WriteSync(int option, std::string m, void* obj);
Message ReadSync();
void WriteAsync(int option, std::string m, void* obj);
bool ReadAsync();
std::string SerializeMessage(int option, std::string message, void* obj);
Message UnserializeMessage(std::string buf);
Circle RandomizeCircle();

void WINAPI CompletedReadRoutine(DWORD errorCode, DWORD bytesCopied, LPOVERLAPPED overlapped);
void WINAPI CompletedWriteRoutine(DWORD errorCode, DWORD bytesCopied, LPOVERLAPPED overlapped);

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
		pipe = CreateFile(L"\\\\.\\pipe\\mynamedpipe", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
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
	lpPipeInst = (LPPIPEINST)GlobalAlloc(GPTR, sizeof(PIPEINST));
	if (lpPipeInst == NULL)
	{
		return 0;
	}
	lpPipeInst->hPipeInst = pipe;

	std::string choices = "Write an option: 1 - Server Ping; 2 - Write a private message; 3 - Wrire a public message\n";
	std::cout << choices;
	int option = 0;
	std::cin >> option;

	while (option != 0)
	{
		switch (option)
		{
		case 0:
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
		case 3: //Continuos reading Async for 8 seconds
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
		std::cout << choices;
		std::cin >> option;
	}

	_getch();

	return status;
}

void WriteSync(int option, std::string m, void* obj)
{
	std::string localBufferW = SerializeMessage(option, m, obj);
	DWORD numBytesWritten = 0;

	bool resultW = WriteFile(pipe, localBufferW.c_str(), localBufferW.length() + 1, &numBytesWritten, NULL);
	if (!resultW)
	{
		std::cout << "Error sending the request" << std::endl;
	}
}

Message ReadSync()
{
	Message m;
	DWORD numBytesRead = 0;
	char localBufferR[BUFSIZE];

	bool resultR = ReadFile(pipe, localBufferR, sizeof(lpPipeInst->bufferRead), &numBytesRead, NULL);

	if (resultR && numBytesRead != 0) {
		std::string s(localBufferR, numBytesRead - 1);
		m = UnserializeMessage(s);

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

	tSuccess = PeekNamedPipe(pipe, NULL, 0, NULL, &avail, NULL);
	if (tSuccess && avail > 0)
	{
		DWORD numBytesRead = 0;
		char localBufferR[BUFSIZE];
		ReadFile(pipe, localBufferR, sizeof(lpPipeInst->bufferRead), &numBytesRead, NULL);
	}
}

void WriteAsync(int option, std::string m, void* obj)
{
	//buffer = SerializeMessage(option, m, NULL);	
	lpPipeInst->chReply = SerializeMessage(option, m, obj);
	bool res = WriteFileEx(pipe, lpPipeInst->chReply.c_str(), lpPipeInst->chReply.length() + 1, (LPOVERLAPPED)lpPipeInst, (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);
	DWORD bytesWritten = 0;
	if (res)
	{
		GetOverlappedResultEx(pipe, (LPOVERLAPPED)lpPipeInst, &bytesWritten, INFINITE, TRUE);
	}
}

bool ReadAsync()
{
	bool res = ReadFileEx(pipe, lpPipeInst->bufferRead, sizeof(lpPipeInst->bufferRead), (LPOVERLAPPED)lpPipeInst, (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);
	DWORD bytesRead = 0;
	if (res)
	{
		GetOverlappedResultEx(pipe, (LPOVERLAPPED)lpPipeInst, &bytesRead, 8000, TRUE);
	}

	return res;
}

std::string SerializeMessage(int option, std::string message, void* obj)
{
	Message m;
	m.command = option;
	m.message = message;

	if (obj)
	{
		Circle* c = static_cast<Circle *>(obj);
		m.circle = *c;
	}
	std::ostringstream os(std::ios::binary);
	cereal::BinaryOutputArchive oarchive(os);
	oarchive(m);

	return os.str();
}

Message UnserializeMessage(std::string buf)
{
	Message m;
	if (buf != "")
	{
		std::istringstream is(buf);
		cereal::BinaryInputArchive iarchive(is);
		iarchive(m);
	}

	return m;
}

void WINAPI CompletedWriteRoutine(DWORD errorCode, DWORD bytesCopied, LPOVERLAPPED overlapped)
{
	if (ERROR_SUCCESS == errorCode)
	{
		LPPIPEINST lpo = (LPPIPEINST)overlapped;
		Message m = UnserializeMessage(lpo->chReply);
		std::cout << "Async Message sent to the pipe: " << m.message << std::endl;
		//Sleep(2000);
		//ReadAsync();
	}
}

void WINAPI CompletedReadRoutine(DWORD errorCode, DWORD bytesCopied, LPOVERLAPPED overlapped)
{
	if (ERROR_SUCCESS == errorCode)
	{
		LPPIPEINST lpo = (LPPIPEINST)overlapped;
		std::string s(lpo->bufferRead, bytesCopied - 1);
		Message m = UnserializeMessage(s);
		std::cout << "Async Message received from the pipe: " << m.message << std::endl;
		//std::cout << m.message << std::endl;
	}
}

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