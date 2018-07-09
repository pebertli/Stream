#include "stdafx.h"
#include <string>
#include <sstream>
#include <types/string.hpp>
#include <archives/binary.hpp>
#include <archives/json.hpp>
#include <types/polymorphic.hpp>

class Shape
{
public:

	Shape() = default;

	virtual float area() = 0;
	virtual std::string toString() = 0;
	std::string colour;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(colour);
	}

};

class Circle : public Shape
{
public:

	Circle() = default;
	Circle(std::string c, float r)
	{
		colour = c;
		radius = r;
	}

	float radius;	

	float area()
	{
		return radius * radius*3.1415;
	}

	std::string toString()
	{
		std::stringstream s;
		s << "Circle " << colour << " radius " << radius;
		return s.str();
	}

	template<class Archive>	void serialize(Archive & archive)
	{
		archive(cereal::virtual_base_class<Shape>(this), radius);
	}
};

class Square : public Shape
{
public:

	Square() = default;
	Square(std::string c, float s)
	{
		colour = c;
		side = s;
	}

	float side;

	float area()
	{
		return side * side;
	}

	std::string toString()
	{
		std::stringstream s;
		s << "Square " << colour << " side " << side;
		return s.str();
	}

	template<class Archive>	void serialize(Archive & archive)
	{
		archive(cereal::virtual_base_class<Shape>(this), side);
	}
};

CEREAL_REGISTER_TYPE(Circle);
CEREAL_REGISTER_TYPE(Square);

//CEREAL_REGISTER_POLYMORPHIC_RELATION(Shape, Circle)

