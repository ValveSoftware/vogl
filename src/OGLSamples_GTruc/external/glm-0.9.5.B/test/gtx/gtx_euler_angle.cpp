// Code sample from Filippo Ramaciotti

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <iostream>

using namespace glm;

int main()
{ 
	f32 first =  1.046f;
	f32 second = 0.52f;
	f32 third = -0.785f;

	fmat4 rotationEuler = eulerAngleYXZ(first, second, third); 

	fmat4 rotationInvertedY  = eulerAngleY(-1.f*first) * eulerAngleX(second) * eulerAngleZ(third); 
	fmat4 rotationDumb = glm::fmat4(); 
	rotationDumb = rotate(rotationDumb, first, glm::fvec3(0,1,0)); 
	rotationDumb = rotate(rotationDumb, second, glm::fvec3(1,0,0)); 
	rotationDumb = rotate(rotationDumb, third, glm::fvec3(0,0,1)); 

	std::cout << glm::to_string(fmat3(rotationEuler)) << std::endl; 
	std::cout << glm::to_string(fmat3(rotationDumb)) << std::endl; 
	std::cout << glm::to_string(fmat3(rotationInvertedY )) << std::endl; 

	std::cout <<"\nRESIDUAL\n"; 
	std::cout << glm::to_string(fmat3(rotationEuler-(rotationDumb))) << std::endl; 
	std::cout << glm::to_string(fmat3(rotationEuler-(rotationInvertedY ))) << std::endl; 

	return 0; 
}
