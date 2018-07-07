#include "stdafx.h"
#include <string>
#include <sstream>
#include <string>
#include <types/string.hpp>

class Circle
{
public:
	std::string colour;
	float radius;
	int centerX;
	int centerY;

	Circle() = default;
	Circle(std::string c, float r, int x, int y)
	{
		colour = c;
		radius = r;
		centerX = x;
		centerY = y;
	}

	void show()
	{
		std::cout << toString();
	}

	std::string toString()
	{
		std::ostringstream ret;
		ret << "I am a circle in the server. My attributes are: colour - " << colour << ", radius -  " << radius << ", centerX -  " << centerX << ", centerY -  " << centerY;
		return ret.str();
	}

	template<class Archive>	void serialize(Archive & archive)
	{
		archive(colour, radius, centerX, centerY);
	}
};

class Message
{

public:
	int command;
	std::string message;
	Circle circle;

	Message() = default;
	template<class Archive>	void serialize(Archive & archive)
	{
		archive(command, message, circle);
	}



};
