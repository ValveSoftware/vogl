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
/// @file gli/core/clone.inl
/// @date 2013-01-23 / 2013-01-23
/// @author Christophe Riccio
///////////////////////////////////////////////////////////////////////////////////

namespace gli{

inline image clone(image const & Image)
{
	image Result;

	memcpy(Result.data<glm::byte>(), Image.data<glm::byte>(), Image.size());
		
	return Result;
}

/*
template <typename texture>
texture clone(texture const & Texture)
{
#		error Invalid texture typename argument. No specialization available for the clone function.
}
*/

template <>
inline texture1D clone(texture1D const & Texture)
{
	texture1D Clone(
		Texture.levels(), 
		Texture.format(), 
		Texture.dimensions());

	memcpy(
		Clone.data<glm::byte>(), 
		Texture.data<glm::byte>(), 
		Texture.size<glm::byte>());
		
	return Clone;
}

template <>
inline texture1DArray clone(texture1DArray const & Texture)
{
	texture1DArray Clone(
		Texture.layers(),
		Texture.levels(), 
		Texture.format(), 
		Texture.dimensions());

	memcpy(
		Clone.data<glm::byte>(), 
		Texture.data<glm::byte>(), 
		Texture.size<glm::byte>());
		
	return Clone;
}

template <>
inline texture2D clone(texture2D const & Texture)
{
	texture2D Clone(
		Texture.levels(), 
		Texture.format(), 
		Texture.dimensions());

	memcpy(
		Clone.data<glm::byte>(), 
		Texture.data<glm::byte>(), 
		Texture.size<glm::byte>());
		
	return Clone;
}

template <>
inline texture2DArray clone(texture2DArray const & Texture)
{
	texture2DArray Clone(
		Texture.layers(),
		Texture.levels(), 
		Texture.format(), 
		Texture.dimensions());

	memcpy(
		Clone.data<glm::byte>(), 
		Texture.data<glm::byte>(), 
		Texture.size<glm::byte>());
		
	return Clone;
}

template <>
inline texture3D clone(texture3D const & Texture)
{
	texture3D Clone(
		Texture.levels(), 
		Texture.format(), 
		Texture.dimensions());

	memcpy(
		Clone.data<glm::byte>(), 
		Texture.data<glm::byte>(), 
		Texture.size<glm::byte>());
		
	return Clone;
}

template <>
inline textureCube clone(textureCube const & Texture)
{
	textureCube Clone(
		Texture.faces(),
		Texture.levels(), 
		Texture.format(), 
		Texture.dimensions());

	memcpy(
		Clone.data<glm::byte>(), 
		Texture.data<glm::byte>(), 
		Texture.size<glm::byte>());
		
	return Clone;
}

template <>
inline textureCubeArray clone(textureCubeArray const & Texture)
{
	textureCubeArray Clone(
		Texture.layers(),
		Texture.faces(),
		Texture.levels(), 
		Texture.format(), 
		Texture.dimensions());

	memcpy(
		Clone.data<glm::byte>(), 
		Texture.data<glm::byte>(), 
		Texture.size<glm::byte>());
		
	return Clone;
}

}//namespace gli
