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
/// @file gli/core/texture_cube.inl
/// @date 2011-04-06 / 2012-12-12
/// @author Christophe Riccio
///////////////////////////////////////////////////////////////////////////////////

namespace gli
{
	inline textureCube::textureCube() :
		View(0, 0, 0, 0, 0, 0),
		Format(FORMAT_NULL)
	{}

	inline textureCube::textureCube
	(
		size_type const & Faces,
		size_type const & Levels,
		format_type const & Format,
		dimensions_type const & Dimensions
	) :
		Storage(
			1,
			Faces,
			Levels,
			Format,
			storage::dimensions_type(Dimensions, 1)),
		View(0, 0, 0, Faces - 1, 0, Levels - 1),
		Format(Format)
	{}

	inline textureCube::textureCube
	(
		storage const & Storage
	) :
		Storage(Storage),
		View(0, 0, 0, Storage.faces() - 1, 0, Storage.levels() - 1),
		Format(Storage.format())
	{}

	inline textureCube::textureCube
	(
		format_type const & Format,
		storage const & Storage,
		detail::view const & View
	) :
		Storage(Storage),
		View(View),
		Format(Format)
	{}

	inline textureCube::operator storage() const
	{
		return this->Storage;
	}

	inline texture2D textureCube::operator[] (size_type const & Face) const
	{
		assert(Face < this->faces());

		return texture2D(
			this->format(),
			this->Storage,
			detail::view(
				this->View.BaseLayer,
				this->View.MaxLayer,
				Face,
				Face,
				this->View.BaseLevel,
				this->View.MaxLevel));
	}

	inline bool textureCube::empty() const
	{
		return this->Storage.empty();
	}

	inline textureCube::size_type textureCube::size() const
	{
		assert(!this->empty());

		return this->Storage.layerSize();
	}

	template <typename genType>
	inline textureCube::size_type textureCube::size() const
	{
		assert(!this->empty());
		assert(sizeof(genType) <= this->Storage.blockSize());

		return this->size() / sizeof(genType);
	}

	inline textureCube::dimensions_type textureCube::dimensions() const
	{
		return textureCube::dimensions_type(this->Storage.dimensions(this->View.BaseLevel));
	}

	inline textureCube::format_type textureCube::format() const
	{
		return this->Format;
	}

	inline textureCube::size_type textureCube::layers() const
	{
		return 1;
	}
	
	inline textureCube::size_type textureCube::faces() const
	{
		return this->View.MaxFace - this->View.BaseFace + 1;
	}
	
	inline textureCube::size_type textureCube::levels() const
	{
		return this->View.MaxLevel - this->View.BaseLevel + 1;
	}

	inline void * textureCube::data()
	{
		assert(!this->empty());

		size_type const offset = detail::linearAddressing(
			this->Storage, this->View.BaseLayer, this->View.BaseFace, this->View.BaseLevel);

		return this->Storage.data() + offset;
	}

	inline void const * textureCube::data() const
	{
		assert(!this->empty());
		
		size_type const offset = detail::linearAddressing(
			this->Storage, this->View.BaseLayer, this->View.BaseFace, this->View.BaseLevel);

		return this->Storage.data() + offset;
	}

	template <typename genType>
	inline genType * textureCube::data()
	{
		assert(!this->empty());
		assert(this->Storage.blockSize() >= sizeof(genType));

		return reinterpret_cast<genType *>(this->data());
	}

	template <typename genType>
	inline genType const * textureCube::data() const
	{
		assert(!this->empty());
		assert(this->Storage.blockSize() >= sizeof(genType));

		return reinterpret_cast<genType const *>(this->data());
	}
}//namespace gli
