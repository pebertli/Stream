#include "stdafx.h"
#define NOMINMAX
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

VOID DisconnectAndClose(LPPIPEINST);
BOOL CreateAndConnectInstance(LPOVERLAPPED);
BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);
VOID GetAnswerToRequest(LPPIPEINST);

//Callbacks for async pipe calls
VOID WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);
VOID WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);

HANDLE pipe;

//private circles. Each user has your own.
//the key is the handle of the client, however the same handle (int) can be given to different clients at different times
std::map<int, std::shared_ptr<Shape>> UserShape;
//Public Message where any client have access
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
				fSuccess = GetOverlappedResult(pipe, &oConnect, &cbRet, FALSE);
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

			lpPipeInst->hPipeInst = pipe;
			std::cout << "Client connected with handle " << pipe << std::endl;

			// Start the read operation for this client. 	
			lpPipeInst->cbToWrite = 0;
			//Callback write because this procedure calls an Read pipe operation
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

//callback for async write pipe operations. If it was a successful read operation, calls Read Async
VOID WINAPI CompletedWriteRoutine(DWORD dwErr, DWORD cbWritten, LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fRead = FALSE;

	lpPipeInst = (LPPIPEINST)lpOverLap;

	int s = BUFSIZE * sizeof(TCHAR);
	//If was an successcul read operation, calls Read Async
	if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))
		fRead = ReadFileEx(lpPipeInst->hPipeInst, lpPipeInst->bufferRead, sizeof(lpPipeInst->bufferRead), (LPOVERLAPPED)lpPipeInst, (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);
	if (!fRead)
		DisconnectAndClose(lpPipeInst);
}

//callback for async read pipe operations. If it was a successful write operation, calls Write Async
VOID WINAPI CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead, LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fWrite = FALSE;

	lpPipeInst = (LPPIPEINST)lpOverLap;
	lpPipeInst->cbRead = cbBytesRead;

	//If it was a successful write operation, treat the content readed and calls Write Async
	if ((dwErr == 0) && (cbBytesRead != 0))
	{
		//treat the request
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
	LPCWSTR lpszPipename = TEXT("\\\\.\\pipe\\streamlabs");

	pipe = CreateNamedPipe(
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
	if (pipe == INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	return ConnectToNewClient(pipe, lpoOverlap);
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
	//trunc the vector of char
	std::string s(pipe->bufferRead, pipe->cbRead - 1);	
	Message m = Message::Unmarshalling(s);

	//which was the request?
	switch (m.command)
	{
	case 1://Ping. Answer with a ping message
	{		
		//save a Message in the write buffer		
		pipe->chReply = Message::Marshalling(m.command, "Ping Successful", m.shape);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		std::cout << "Ping has been requested by " << pipe->hPipeInst << " with message " << m.message << std::endl;
		break;
	}
	case 2://Simulate a loading. The server will queue the requisitions while this is loading
	{
		std::cout << "Simulating a loading " << std::endl;
		//TODO, make a real loading
		Sleep(5000);
		//save a Message in the write buffer
		std::cout << "A message Async has been written by " << pipe->hPipeInst << " with message " << m.message << std::endl;
		pipe->chReply = Message::Marshalling(m.command, "Server returning the message", NULL);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}
	case 4://Save the shape received for the specific client
	{
		//create or replace a shape map for the specified client
		int index = (int)pipe->hPipeInst;
		UserShape[index] = m.shape;
		std::cout << "A Shape from " << pipe->hPipeInst << " has been saved in the server. " << m.shape->toString() << std::endl;
		//save a Message in the write buffer
		pipe->chReply = Message::Marshalling(m.command, "Shape saved", NULL);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}
	case 5://Return the shape of the specific client that request
	{
		std::cout << "A Shape was requested by " << pipe->hPipeInst << std::endl;
		int index = (int)pipe->hPipeInst;
		//Retrieve the shape for the specified client, if any. Save a Message in the write buffer
		std::map<int, std::shared_ptr<Shape>>::iterator it = UserShape.find(index);
		if (it == UserShape.end())
			pipe->chReply = Message::Marshalling(m.command, "There is no Shape for this user", NULL);
		else
		{
			std::shared_ptr<Shape> shp(it->second);
			pipe->chReply = Message::Marshalling(m.command, "The user has a Shape", std::move(shp));
		}
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}	
	case 6://Save the received message and make available for all clients
	{
		std::cout << "A new message has been saved by " << pipe->hPipeInst << ": " << m.message << std::endl;
		//Save the message for posterior calls
		PublicMessage = m.message;
		//Save a Message in the write buffer
		pipe->chReply = Message::Marshalling(m.command, "Public message saved", NULL);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
		break;
	}
	case 7://Return the saved message regardless the client
	{
		std::cout << "The public message was requested by " << pipe->hPipeInst << std::endl;
		//Save a Message in the write buffer with the public message, which is available for all clients
		pipe->chReply = Message::Marshalling(m.command, PublicMessage, NULL);
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}
	case 8://call a procedure from the shape received. Doesn't save the for posterior calls
	{
		//To show the polymorphism, this respond calls the Area of the received shape, regardless his derived istance
		float radius = m.shape.get()->area();
		std::cout << "A Shape Procedure Async has been requested by "
			<< pipe->hPipeInst
			<< " with message "
			<< m.message << std::endl
			<< m.shape->toString()
			<< " My area is " << m.shape->area() << std::endl;
		;
		pipe->chReply = std::string("Async has been returned").c_str();
		pipe->cbToWrite = pipe->chReply.length() + 1;
		break;
	}
	default:
		break;
	}
}