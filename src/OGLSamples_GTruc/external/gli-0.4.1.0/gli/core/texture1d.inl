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
/// @file gli/core/texture1d.inl
/// @date 2013-01-12 / 2013-01-12
/// @author Christophe Riccio
///////////////////////////////////////////////////////////////////////////////////

namespace gli
{
	inline texture1D::texture1D() :
		View(0, 0, 0, 0, 0, 0),
		Format(FORMAT_NULL)
	{}

	inline texture1D::texture1D
	(
		size_type const & Levels,
		format_type const & Format,
		dimensions_type const & Dimensions
	) :
		Storage(
			1, 
			1, 
			Levels,
			Format,
			storage::dimensions_type(Dimensions, 1, 1)),
		View(
			0, 0,
			0, 0,
			0, Levels - 1),
		Format(Format)
	{}

	inline texture1D::texture1D
	(
		storage const & Storage
	) :
		Storage(Storage),
		View(0, 0, 0, 0, 0, Storage.levels() - 1),
		Format(Storage.format())
	{}

	inline texture1D::texture1D
	(
		format_type const & Format,
		storage const & Storage,
		detail::view const & View
	) :
		Storage(Storage),
		View(View),
		Format(Format)
	{}
 
	inline texture1D::operator storage() const
	{
		return this->Storage;
	}

	inline image texture1D::operator[]
	(
		texture1D::size_type const & Level
	) const
	{
		assert(Level < this->levels());

		return image(
			this->Storage,
			detail::view(
				this->View.BaseLayer, 
				this->View.MaxLayer, 
				this->View.BaseFace,
				this->View.MaxFace,
				Level,
				Level));
	}

	inline bool texture1D::empty() const
	{
		return this->Storage.empty();
	}

	inline texture1D::size_type texture1D::size() const
	{
		return this->Storage.faceSize();
	}

	template <typename genType>
	inline texture1D::size_type texture1D::size() const
	{
		assert(sizeof(genType) <= this->Storage.blockSize());
		return this->size() / sizeof(genType);
	}

	inline texture1D::dimensions_type texture1D::dimensions() const
	{
		return texture1D::dimensions_type(this->Storage.dimensions(this->View.BaseLevel).x);
	}

	inline texture1D::format_type texture1D::format() const
	{
		return this->Format;
	}

	inline texture1D::size_type texture1D::layers() const
	{
		return 1;
	}

	inline texture1D::size_type texture1D::faces() const
	{
		return 1;
	}

	inline texture1D::size_type texture1D::levels() const
	{
		return this->View.MaxLevel - this->View.BaseLevel + 1;
	}

	inline void * texture1D::data()
	{
		assert(!this->empty());

		size_type const offset = detail::linearAddressing(
			this->Storage, this->View.BaseLayer, this->View.BaseFace, this->View.BaseLevel);

		return this->Storage.data() + offset;
	}

	inline void const * texture1D::data() const
	{
		assert(!this->empty());
		
		size_type const offset = detail::linearAddressing(
			this->Storage, this->View.BaseLayer, this->View.BaseFace, this->View.BaseLevel);

		return this->Storage.data() + offset;
	}

	template <typename genType>
	inline genType * texture1D::data()
	{
		assert(!this->empty());
		assert(this->Storage.blockSize() >= sizeof(genType));

		return reinterpret_cast<genType *>(this->data());
	}

	template <typename genType>
	inline genType const * texture1D::data() const
	{
		assert(!this->empty());
		assert(this->Storage.blockSize() >= sizeof(genType));

		return reinterpret_cast<genType const *>(this->data());
	}
}//namespace gli
