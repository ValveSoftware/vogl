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
/// @file gli/core/image.inl
/// @date 2011-10-06 / 2013-01-12
/// @author Christophe Riccio
///////////////////////////////////////////////////////////////////////////////////

namespace gli
{
	inline image::image() :
		View(0, 0, 0, 0, 0, 0)
	{}

	inline image::image
	(
		dimensions_type const & Dimensions,
		size_type const & BlockSize,
		dimensions_type const & BlockDimensions
	) :
		Storage(
			1, 1, 1, 
			storage::dimensions_type(Dimensions), 
			BlockSize, 
			storage::dimensions_type(BlockDimensions)),
		View(0, 0, 0, 0, 0, 0)
	{}

	inline image::image
	(
		format const & Format,
		dimensions_type const & Dimensions
	) :
		Storage(
			1, 1, 1, 
			storage::dimensions_type(Dimensions),
			block_size(Format),
			block_dimensions(Format)),
		View(0, 0, 0, 0, 0, 0)
	{}

	inline image::image
	(
		storage const & Storage,
		detail::view const & View
	) :
		Storage(Storage),
		View(View)
	{}

	inline bool image::empty() const
	{
		return this->Storage.empty();
	}

	inline image::size_type image::size() const
	{
		assert(!this->empty());

		return this->Storage.levelSize(this->View.BaseLevel);
	}

	inline image::dimensions_type image::dimensions() const
	{
		return image::dimensions_type(this->Storage.dimensions(this->View.BaseLevel));
	}

	inline void * image::data()
	{
		assert(!this->empty());

		size_type const offset = detail::linearAddressing(
			this->Storage, this->View.BaseLayer, this->View.BaseFace, this->View.BaseLevel);

		return this->Storage.data() + offset;
	}

	inline void const * image::data() const
	{
		assert(!this->empty());
		
		size_type const offset = detail::linearAddressing(
			this->Storage, this->View.BaseLayer, this->View.BaseFace, this->View.BaseLevel);

		return this->Storage.data() + offset;
	}

	template <typename genType>
	inline genType * image::data()
	{
		assert(!this->empty());
		assert(this->Storage.blockSize() >= sizeof(genType));

		return reinterpret_cast<genType *>(this->data());
	}

	template <typename genType>
	inline genType const * image::data() const
	{
		assert(!this->empty());
		assert(this->Storage.blockSize() >= sizeof(genType));

		return reinterpret_cast<genType const *>(this->data());
	}

	inline bool operator== (image const & ImageA, image const & ImageB)
	{
		if(!glm::all(glm::equal(ImageA.dimensions(), ImageB.dimensions())))
			return false;

		if(ImageA.size() != ImageB.size())
			return false;

		if(ImageA.data() == ImageB.data())
			return true;

		for(image::size_type i(0); i < ImageA.size(); ++i)
		{
			if(*(ImageA.data<glm::byte>() + i) != *(ImageB.data<glm::byte>() + i))
				return false;
		}

		return true;
	}

	inline bool operator!= (image const & ImageA, image const & ImageB)
	{
		return !(ImageA == ImageB);
	}
}//namespace gli
