//#include "stdafx.h"
#include <string>
#include <sstream>
#include "Shape.cpp"
#include <types/string.hpp>
#include <archives/binary.hpp>
#include <archives/json.hpp>

class Message
{

public:
	int command;
	std::string message;	
	std::shared_ptr<Shape> shape;

	Message() = default;

	////using the Cereal library to serialize/unserialize a option, a message and an object of type Circle
	static std::string Marshalling(int option, std::string m, std::shared_ptr<Shape> s)
	{		
		Message msg;
		msg.command = option;
		msg.message = m;
		if (s)
		{					
			msg.shape = std::move(s);				
		}		

		std::stringstream os;
		{
			cereal::BinaryOutputArchive oarchive(os);
			oarchive(msg);
		}				

		return os.str();
	}

	////using the Cereal library to serialize/unserialize a option, a message and an object of type Circle
	static Message Unmarshalling(std::string buffer)
	{
		Message m;
		if (buffer != "")
		{
			std::stringstream is(buffer);
			{
				cereal::BinaryInputArchive iarchive(is);
				iarchive(m);
			}
		}

		return m;
	}

	template<class Archive>	void serialize(Archive & archive)
	{
		archive(command, message, shape);
	}

	

};