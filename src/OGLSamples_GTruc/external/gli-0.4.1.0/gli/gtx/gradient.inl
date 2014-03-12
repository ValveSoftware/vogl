///////////////////////////////////////////////////////////////////////////////////
/// OpenGL Image (gli.g-truc.net)
///
/// Copyright (c) 2008 - 2013 G-Truc Creation (www.g-truc.net)
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.
///
/// @ref core
/// @file gli/gtx/gradient.inl
/// @date 2008-12-19 / 2013-01-13
/// @author Christophe Riccio
///////////////////////////////////////////////////////////////////////////////////

namespace gli
{
	inline texture2D radial
	(
		texture2D::dimensions_type const & Size, 
		texture2D::texcoord_type const & Center,
		float const & Radius,
		texture2D::texcoord_type const & Focal
	)
	{
		texture2D Result(1, gli::RGB8U, texture2D::dimensions_type(Size));
		glm::u8vec3 * DstData = (glm::u8vec3*)Result.data();

		for(std::size_t y = 0; y < Result.dimensions().y; ++y)
		for(std::size_t x = 0; x < Result.dimensions().x; ++x)
		{
			float Value = glm::radialGradient(
				Center * glm::vec2(Size), 
				Radius, 
				Focal * glm::vec2(Size),
				glm::vec2(x, y));

			std::size_t Index = x + y * Result.dimensions().x;

			*(DstData + Index) = glm::u8vec3(glm::u8(glm::clamp(Value * 255.f, 0.f, 255.f)));
		}

		return Result;
	}

	inline texture2D linear
	(
		texture2D::dimensions_type const & Size, 
		texture2D::texcoord_type const & Point0, 
		texture2D::texcoord_type const & Point1
	)
	{
		texture2D Result(1, gli::RGB8U, texture2D::dimensions_type(Size));
		glm::u8vec3 * DstData = (glm::u8vec3*)Result.data();

		for(std::size_t y = 0; y < Result.dimensions().y; ++y)
		for(std::size_t x = 0; x < Result.dimensions().x; ++x)
		{
			float Value = glm::linearGradient(
				Point0 * glm::vec2(Size), 
				Point1 * glm::vec2(Size),
				texture2D::texcoord_type(x, y));

			std::size_t Index = x + y * Result.dimensions().x;

			*(DstData + Index) = glm::u8vec3(glm::u8(glm::clamp(Value * 255.f, 0.f, 255.f)));
		}

		return Result;
	}

}//namespace gli
