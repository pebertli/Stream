// Stream.cpp : define o ponto de entrada para o aplicativo do console.
//

#include "stdafx.h"
#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <strsafe.h>
#include <iostream>
#include <fstream>
#include <map>
#include <time.h>
#include "Message.cpp"
#include <archives/binary.hpp>

#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

typedef struct
{
	OVERLAPPED oOverlap;
	HANDLE hPipeInst;
	char bufferRead[BUFSIZE];
	DWORD cbRead;
	std::string chReply;
	DWORD cbToWrite;
} PIPEINST, *LPPIPEINST;

VOID DisconnectAndClose(LPPIPEINST);
BOOL CreateAndConnectInstance(LPOVERLAPPED);
BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);
VOID GetAnswerToRequest(LPPIPEINST);

std::string SerializeMessage(int option, std::string message, void* obj);
Message UnserializeMessage(std::string buf);

VOID WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);
VOID WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);

HANDLE hPipe;

//private message
std::map<int, Circle> UserCircle;
//User Message
std::string PublicMessage = "Default Public Message";

int _tmain(VOID)
{
	HANDLE hConnectEvent;
	OVERLAPPED oConnect;
	LPPIPEINST lpPipeInst;
	DWORD dwWait, cbRet;
	BOOL fSuccess, fPendingIO;

	hConnectEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

	if (hConnectEvent == NULL)
	{
		return 0;
	}

	oConnect.hEvent = hConnectEvent;

	// Call a subroutine to create one instance, and wait for the client to connect. 
	fPendingIO = CreateAndConnectInstance(&oConnect);
	std::cout << "Pipe created. Waiting a client connection " << std::endl;

	while (1)
	{
		// Wait for a client to connect, or for a read or write 
		// operation to be completed, which causes a completion 
		// routine to be queued for execution. 
		dwWait = WaitForSingleObjectEx(hConnectEvent, INFINITE, TRUE);
		switch (dwWait)
		{
			// The wait conditions are satisfied by a completed connect operation. 
		case 0:
			// If an operation is pending, get the result of the connect operation. 
			if (fPendingIO)
			{
				fSuccess = GetOverlappedResult(hPipe, &oConnect, &cbRet, FALSE);
				if (!fSuccess)
				{
					return 0;
				}
			}

			// Allocate storage for this instance. 
			lpPipeInst = (LPPIPEINST)GlobalAlloc(GPTR, sizeof(PIPEINST));
			if (lpPipeInst == NULL)
			{
				return 0;
			}

			lpPipeInst->hPipeInst = hPipe;
			std::cout << "Client connected with handle " << hPipe << std::endl;

			// Start the read operation for this client. 	
			lpPipeInst->cbToWrite = 0;
			CompletedWriteRoutine(0, 0, (LPOVERLAPPED)lpPipeInst);

			// Create new pipe instance for the next client. 
			fPendingIO = CreateAndConnectInstance(&oConnect);
			break;

		case WAIT_IO_COMPLETION:// completion routine. 
			break;
			// An error occurred in the wait function. 
		default:
		{
			return 0;
		}
		}
	}
	return 0;
}

VOID WINAPI CompletedWriteRoutine(DWORD dwErr, DWORD cbWritten, LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fRead = FALSE;

	lpPipeInst = (LPPIPEINST)lpOverLap;

	int s = BUFSIZE * sizeof(TCHAR);
	if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))
		fRead = ReadFileEx(lpPipeInst->hPipeInst, lpPipeInst->bufferRead, sizeof(lpPipeInst->bufferRead), (LPOVERLAPPED)lpPipeInst, (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);
	if (!fRead)
		DisconnectAndClose(lpPipeInst);
}

VOID WINAPI CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead, LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fWrite = FALSE;

	lpPipeInst = (LPPIPEINST)lpOverLap;
	lpPipeInst->cbRead = cbBytesRead;

	if ((dwErr == 0) && (cbBytesRead != 0))
	{
		GetAnswerToRequest(lpPipeInst);
		fWrite = WriteFileEx(lpPipeInst->hPipeInst, lpPipeInst->chReply.c_str(), lpPipeInst->chReply.length() + 1, (LPOVERLAPPED)lpPipeInst, (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);
	}
	if (!fWrite)
		DisconnectAndClose(lpPipeInst);
}

VOID DisconnectAndClose(LPPIPEINST lpPipeInst)
{
	if (!DisconnectNamedPipe(lpPipeInst->hPipeInst))
	{
		printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
	}
	CloseHandle(lpPipeInst->hPipeInst);

	if (lpPipeInst != NULL)
		GlobalFree(lpPipeInst);
}

BOOL CreateAndConnectInstance(LPOVERLAPPED lpoOverlap)
{
	LPCWSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

	hPipe = CreateNamedPipe(
		lpszPipename,             // pipe name 
		PIPE_ACCESS_DUPLEX |      // read/write access 
		FILE_FLAG_OVERLAPPED,     // overlapped mode 
		PIPE_TYPE_MESSAGE |       // message-type pipe 
		PIPE_READMODE_MESSAGE |   // message read mode 
		PIPE_WAIT,                // blocking mode 
		PIPE_UNLIMITED_INSTANCES, // unlimited instances 
		BUFSIZE * sizeof(TCHAR),    // output buffer size 
		BUFSIZE * sizeof(TCHAR),    // input buffer size 
		PIPE_TIMEOUT,             // client time-out 
		NULL);                    // default security attributes
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	return ConnectToNewClient(hPipe, lpoOverlap);
}

BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
	BOOL fConnected, fPendingIO = FALSE;

	fConnected = ConnectNamedPipe(hPipe, lpo);

	//ConnectNamedPipe should return zero. 
	if (fConnected)
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	switch (GetLastError())
	{
		// The overlapped connection in progress. 
	case ERROR_IO_PENDING:
		fPendingIO = TRUE;
		break;

		// Client is already connected, so signal an event. 
	case ERROR_PIPE_CONNECTED:
		if (SetEvent(lpo->hEvent))
			break;

		// If an error occurs during the connect operation... 
	default:
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}
	}
	return fPendingIO;
}

VOID GetAnswerToRequest(LPPIPEINST pipe)
{

	std::string s(pipe->bufferRead, pipe->cbRead - 1);
	Message m = UnserializeMessage(s);

	switch (m.command)
	{
	case 1:
	{
		pipe->chReply = SerializeMessage(m.command, "Ping Successful", &m.circle);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		std::cout << "Ping has been requested by " << pipe->hPipeInst << " with message " << m.message << std::endl;
		break;
	}
	case 2:
	{
		std::cout << "Simulating a loading " << std::endl;
		Sleep(7000);

		std::cout << "A message Async has been written by " << pipe->hPipeInst << " with message " << m.message << std::endl;
		pipe->chReply = SerializeMessage(m.command, "Server returning the message", NULL);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}
	case 4:
	{
		int index = (int)pipe->hPipeInst;
		UserCircle[index] = m.circle;
		std::cout << "A Circle from " << pipe->hPipeInst << " has been saved in the server. " << m.circle.toString() << std::endl;
		pipe->chReply = SerializeMessage(m.command, "Circle saved", NULL);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}
	case 5:
	{
		std::cout << "A Circle was requested by " << pipe->hPipeInst << std::endl;
		int index = (int)pipe->hPipeInst;
		std::map<int, Circle>::iterator it = UserCircle.find(index);
		if (it == UserCircle.end())
			pipe->chReply = SerializeMessage(m.command, "There is no Circle for this user", NULL);
		else
			pipe->chReply = SerializeMessage(m.command, "The user has a Circle", &it->second);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}
	case 6:
	{
		std::cout << "A Circle Procedure Async has been requested" << pipe->hPipeInst << " with message " << m.message << std::endl;
		m.circle.show();
		pipe->chReply = std::string("Async has been returned").c_str();
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}
	case 7:
	{
		std::cout << "A new message has been saved by " << pipe->hPipeInst << ": " << m.message << std::endl;
		PublicMessage = m.message;
		pipe->chReply = SerializeMessage(m.command, "Public message saved", NULL);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
		break;
	}
	case 8:
	{
		std::cout << "The public message was requested by " << pipe->hPipeInst << std::endl;
		pipe->chReply = SerializeMessage(m.command, PublicMessage, NULL);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}
	default:
		break;
	}
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
	if (buf != "")
	{
		std::istringstream is(buf);
		cereal::BinaryInputArchive iarchive(is);

		Message m;
		iarchive(m);

		return m;
	}
	Message m;
	return m;
}