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
/// @file gli/core/image.hpp
/// @date 2011-10-06 / 2013-01-12
/// @author Christophe Riccio
///
/// @defgroup core_image Image 
/// @ingroup core
/// 
/// Image1D 
///////////////////////////////////////////////////////////////////////////////////

#ifndef GLI_CORE_IMAGE_INCLUDED
#define GLI_CORE_IMAGE_INCLUDED

#include "storage.hpp"
#include "addressing.hpp"
#include "header.hpp"

namespace gli
{
	//template <typename addressing = linearAddressing>
	class image
	{
	public:
		typedef storage::dimensions3_type dimensions_type;
		typedef storage::size_type size_type;

		image();

		/// Allocate a new storage constructor
		explicit image(
			dimensions_type const & Dimensions,
			size_type const & BlockSize,
			dimensions_type const & BlockDimensions);

		/// Allocate a new storage constructor
		explicit image(
			format const & Format,
			dimensions_type const & Dimensions);

		/// Reference an exiting storage constructor
		explicit image(
			storage const & Storage,
			detail::view const & View);

		/// Cast an image into a storage
		//operator storage

		bool empty() const;
		size_type size() const;
		dimensions_type dimensions() const;

		void * data();
		void const * data() const;

		template <typename genType>
		genType * data();
		template <typename genType>
		genType const * data() const;

		bool isReference() const;

	private:
		storage Storage;
		detail::view View;
	};

	bool operator== (image const & ImageA, image const & ImageB);

	bool operator!= (image const & ImageA, image const & ImageB);
}//namespace gli

#include "image.inl"

#endif//GLI_CORE_IMAGE_INCLUDED
