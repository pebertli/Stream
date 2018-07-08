#include "stdafx.h"
#include <string>
#include <sstream>
#include <types/string.hpp>
#include <types/polymorphic.hpp>

// Base class
class Shape {
public:
	std::string colour;	
	int centerX;
	int centerY;

	float area();	
	std::string toString();
};

class Circle : public Shape
{


public:	
	float radius;	

	/*Circle() = default;
	Circle(std::string c, float r, int x, int y)
	{
		colour = c;
		radius = r;
		centerX = x;
		centerY = y;
	}*/

	float area();
	

	std::string toString();

	template<class Archive>	void serialize(Archive & archive)
	{
		archive(colour, radius, centerX, centerY);
	}
};

class Square : public Shape
{


public:	
	float side;
	
	/*Square() = default;
	Square(std::string c, float r, int x, int y)
	{
		colour = c;
		side = r;
		centerX = x;
		centerY = y;
	}*/

	float area();

	std::string toString();	

	template<class Archive>	void serialize(Archive & archive)
	{
		archive(colour, side, centerX, centerY);
	}
};

#include <archives/binary.hpp>

// Register DerivedClassOne
CEREAL_REGISTER_TYPE(Circle);
CEREAL_REGISTER_TYPE(Square);

// Note that there is no need to register the base class, only derived classes
//  However, since we did not use cereal::base_class, we need to clarify
//  the relationship (more on this later)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Shape, Circle)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Shape, Square)

//CEREAL_REGISTER_DYNAMIC_INIT(Message)

class Message
{

public:
	int command;
	std::string message;
	Shape obj;

	Message() = default;

	////using the Cereal library to serialize/unserialize a option, a message and an object of type Circle
	static std::string Marshalling(int option, std::string m, void* obj)
	{		
		Message msg;
		msg.command = option;
		msg.message = m;
		if (obj)
		{
			Shape* c = static_cast<Shape *>(obj);
			msg.obj = *c;
		}

		std::ostringstream os(std::ios::binary);
		cereal::BinaryOutputArchive oarchive(os);
		oarchive(msg);

		return os.str();
	}

	////using the Cereal library to serialize/unserialize a option, a message and an object of type Circle
	static Message Unmarshalling(std::string buffer)
	{
		Message m;
		if (buffer != "")
		{
			std::istringstream is(buffer);
			cereal::BinaryInputArchive iarchive(is);
			iarchive(m);
		}

		return m;
	}

	template<class Archive>	void serialize(Archive & archive)
	{
		archive(command, message, obj);
	}

	

};