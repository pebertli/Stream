C++ project as requested by StreamLabs. Thank you for the opportunity

Pebertli Nils Alho Barata - pebertli@gmail.com
PhD in Virtual Reality
MSc in Artificial Intelligence
BSc in Computer Engineering
Love Games and Skateboard.

-------------------------------------------------------------------------------------------------------------------------
Create 2 C++ console applications, one will be the client and the other one, the server. 
https://paste.ee/p/L7ZCu
-------------------------------------------------------------------------------------------------------------------------

- All the requirements were developed
- The target platform is only Windows
- I have used the Cereal library for serialization, included in the project
- The data sent trhough the pipe, is a serialization of a class Message, which encapsulates a int command, a string message and the content itself
- Any object can be the content, since its inherit from the base class.  The Shape base classe was choosen to demonstrate tha client can send/receive different objects to/from the server. (Try the options 8 and 9 of the server)
- I have used the approach with Completion Routines because only one thread is used for each client connected (especially useful on Windows). That also helps to share data between clients (Try the options 4, 5, 6 and 7)
- The Client and Server has Async and Sync functions. The Async functions, however, keep listening the pipe until that some content has been put in there, in other words, the async is in the read and write functions, not in the Listening. One way to overcome that is use two event handlers. One event to liestening that new data is availabe in the pipe and another to indicate a write task is finished. (Try functions 1, 2 and 3)
- You can try to connect multiple clients to the same server (Try the options 4, 5, 6 and 7)

The 8 hours wasn't enough to check things as:
- atomic instructions
- duplicated servers
- The two event handlers approach
- The multithread approach instead overlapped approach

The executable files are in Stream\Release folder

I loved this chalenge and, again, thank you for the opportunity

