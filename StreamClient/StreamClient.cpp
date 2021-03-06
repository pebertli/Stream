#include "stdafx.h"
#define NOMINMAX   
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
#include <types/polymorphic.hpp>
#include <archives/binary.hpp>
#include <archives/json.hpp>



#define BUFSIZE 4096

void FlushSync();
void WriteSync(int option, std::string m, std::shared_ptr<Shape> obj);
Message ReadSync();
void WriteAsync(int option, std::string m, std::shared_ptr<Shape> obj);
bool ReadAsync();
Circle RandomizeCircle();
Square RandomizeSquare();

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
	std::ostringstream choices;
	choices << std::endl;
	choices << "Write an option : "<<std::endl;
	choices << "1 - Write and read Sync as a Ping. the message will not be saved on the server" << std::endl;
	choices << "2 - Write Async. Server will simulate loading" << std::endl;
	choices << "3 - Keep reading Async for 8 seconds. Receive the most recent content on the read pipe" << std::endl;
	choices << "4 - Send a random circle. The circle will be saved in the server and available only for this client" << std::endl;
	choices << "5 - Retrieve the previous saved circle from the server, which is unique for this client" << std::endl;	
	choices << "6 - Write a message in the server which will be available for all clients" << std::endl;
	choices << "7 - Receive the saved message from the server, which is avaliable for all clients too" << std::endl;	
	choices << "8 - Send a random circle. A procedure of this shape will be called by server. Option 8 and 9 call the same option 8 of the server, as Area() is a function of Shape" << std::endl;
	choices << "9 - Send a random Square. A procedure of this shape will be called by server. Option 8 and 9 call the same option 8 of the server, as Area() is a function of Shape" << std::endl;
	choices << "0 - Close this Client" << std::endl;
	choices << std::endl;
	int option = 0;	
	do
	{
		//keeping ask for the user choice
		std::cout << choices.str();
		std::cin >> option;

		switch (option)
		{
		case 0: //Bye
		{
			std::cout << "Closing...";
			//Just a litle behavior feedback
			Sleep(2000);
			return 0;
			break;
		}
		case 1: //Write and read Sync. Work like a ping, in other words, the message will not be saved on the server
		{
			std::cout << "Write a message to the server" << std::endl;
			std::string str = "";
			//wait for the message
			std::cin >> str;
			//Remove the last content on the read pipe
			FlushSync();
			//call the server
			WriteSync(option, str, NULL);
			//Receive the answer
			Message message = ReadSync();
			std::cout << "Server Response: " << message.message << std::endl;
			break;
		}
		case 2: //Write Async. Server will simulate loading
		{
			//Remove the last content on the read pipe
			FlushSync();
			//call the server
			WriteAsync(option, "Write Async. Server will simulate loading", NULL);
			break;
		}
		case 3: //Continuous reading Async for 8 seconds
		{
			std::cout << "Listening the pipe for 8 seconds " << std::endl;
			//Receive the most recent content on the read pipe
			ReadAsync();
			break;
		}
		case 4: //Send a random circle
		{
			//Remove the last content on the read pipe
			FlushSync();
			//Create and send a random circle through the pipe. The circle will be saved in the "client scope" of the server
			Circle c = RandomizeCircle();
			std::string s = "Save the shape in the server: ";
			s.append(c.toString());
			std::shared_ptr<Circle> obj(new Circle(c.colour, c.radius));			
			WriteAsync(option, s, std::move(obj));
			break;
		}
		case 5: //Receive a previous circle
		{
			//Remove the last content on the read pipe
			FlushSync();
			//call the server
			WriteSync(option, "Request the last client shape from the server", NULL);
			//Receive the previous saved circle from the server. The circle is unique for this client
			Message message = ReadSync();
			//Check if there was a saved circle in the server
			if (message.shape == nullptr)
				std::cout << "Server returns the message: " << message.message << std::endl;
			else
				std::cout << "Server returns the shape: " << message.shape->toString() << std::endl;
			break;
		}		
		case 6: //Write a public message
		{
			//Write a message in the server that will be available for all clients
			std::cout << "Write a message to the server, available to all clients" << std::endl;
			std::string str = "";
			std::cin >> str;
			//Remove the last content on the read pipe
			FlushSync();
			WriteAsync(option, str, NULL);
			break;
		}
		case 7: //Receive a public message
		{
			//Remove the last content on the read pipe
			FlushSync();
			//call the server
			WriteSync(option, "Request the public message", NULL);
			//receive the message from the server, which is avaliable for all clients
			Message message = ReadSync();
			std::cout << "The public message is: " << message.message << std::endl;
			break;
		}
		case 8: //Send a random circle and call its procedure
		{
			//Remove the last content on the read pipe
			FlushSync();
			Circle c = RandomizeCircle();
			std::shared_ptr<Circle> obj(new Circle(c.colour, c.radius));
			//Create and send a random circle through the pipe. The procedure of shape will be called by server. The serve will not save the shape
			//Option 8 and option 9 of the client call the same option 8 of the server as square and circle Area() inherit from Shape
			WriteAsync(8, "Circle procedure call", std::move(obj));
			break;
		}
		case 9: //Send a random square and call its procedure
		{
			//Remove the last content on the read pipe
			FlushSync();
			Square sqr = RandomizeSquare();
			std::shared_ptr<Square> obj(new Square(sqr.colour, sqr.side));
			//Create and send a random circle through the pipe. The procedure of this shape will be called by server. The serve will not save the shape
			//Option 8 and option 9 of the client call the same option 8 of the server as square and circle Area() inherit from Shape
			WriteAsync(8, "Square procedure call", std::move(obj));
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
void WriteSync(int option, std::string m, std::shared_ptr<Shape> obj)
{	
	//generate a package to send through the pipe		
	std::string localBufferW = Message::Marshalling(option, m, std::move(obj));
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
void WriteAsync(int option, std::string m, std::shared_ptr<Shape> obj)
{	
	//generate a package to send through the 	
	lpPipeInst->chReply = Message::Marshalling(option, m, std::move(obj));
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
	Circle circle;
	circle.radius = (float)(rand() % 10 + 1);
	circle.colour = colour;

	return circle;
}

Square RandomizeSquare()
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
	Square square;
	square.side = (float)(rand() % 10 + 1);
	square.colour = colour;

	return square;
}