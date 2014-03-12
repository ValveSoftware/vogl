///////////////////////////////////////////////////////////////////////////////////
/// OpenGL Mathematics (glm.g-truc.net)
///
/// Copyright (c) 2005 - 2013 G-Truc Creation (www.g-truc.net)
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
/// @ref gtc_angle
/// @file glm/gtc/angle.hpp
/// @date 2013-05-07 / 2012-05-07
/// @author Christophe Riccio
///
/// @see core (dependence)
/// @see gtc_angle (dependence)
///
/// @defgroup gtc_angle GLM_GTC_angle
/// @ingroup gtc
/// 
/// @brief Handling angles in both radians and degrees.
/// 
/// <glm/gtc/angle.hpp> need to be included to use these features.
///////////////////////////////////////////////////////////////////////////////////

#ifndef GLM_GTC_angle
#define GLM_GTC_angle GLM_VERSION

namespace glm
{
	template <typename T>
	class angle
	{
	public:
		typedef T value_type;

		angle(T const & x) :
			data(x)
		{}

	private:
		value_type data;
	};

	typedef angle<glm::half> angle16;
	typedef angle<float> angle32;
	typedef angle<double> angle64;

	GLM_FUNC_DECL angle<float> operator "" _rad_f(long double const radians)
	{
		return static_cast<float>(radians);
	}

	GLM_FUNC_DECL angle<double> operator "" _rad(long double const radians)
	{
		return static_cast<double>(radians);
	}

	GLM_FUNC_DECL angle<long double> operator "" _rad_l(long double const radians)
	{
		return radians;
	}

	GLM_FUNC_DECL angle<float> operator "" _deg_f(long double const degrees)
	{
		return static_cast<float>(degrees) * static_cast<float>(0.01745329251994329576923690768489L);
	}

	GLM_FUNC_DECL angle<double> operator "" _deg(long double const degrees)
	{
		return static_cast<double>(degrees) * static_cast<double>(0.01745329251994329576923690768489L);
	}

	GLM_FUNC_DECL angle<long double> operator "" _deg_l(long double const degrees)
	{
		return degrees * 0.01745329251994329576923690768489L;
	}

}//namespace glm

#endif//GLM_GTC_angle
