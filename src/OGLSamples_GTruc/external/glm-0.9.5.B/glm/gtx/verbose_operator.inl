///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenGL Mathematics Copyright (c) 2005 - 2013 G-Truc Creation (www.g-truc.net)
///////////////////////////////////////////////////////////////////////////////////////////////////
// Created : 2006-04-20
// Updated : 2008-09-29
// Licence : This source is under MIT License
// File    : glm/gtx/verbose_operator.inl
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace glm
{
	template <typename genType>
	GLM_FUNC_QUALIFIER genType add(genType const & a, genType const & b)
	{
		return a + b;
	}

	template <typename genType>
	GLM_FUNC_QUALIFIER genType sub(genType const & a, genType const & b)
	{
		return a - b;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER detail::tmat2x2<T, P> mul
	(
		detail::tmat2x2<T, P> const & a, 
		detail::tmat2x2<T, P> const & b
	)
	{
		return a * b;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER detail::tmat3x3<T, P> mul
	(
		detail::tmat3x3<T, P> const & a, 
		detail::tmat3x3<T, P> const & b
	)
	{
		return a * b;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER detail::tmat4x4<T, P> mul
	(
		detail::tmat4x4<T, P> const & a, 
		detail::tmat4x4<T, P> const & b
	)
	{
		return a * b;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER detail::tvec2<T, P> mul
	(
		detail::tmat2x2<T, P> const & m, 
		detail::tvec2<T, P> const & v
	)
	{
		return m * v;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER detail::tvec3<T, P> mul
	(
		detail::tmat3x3<T, P> const & m, 
		detail::tvec3<T, P> const & v)
	{
		return m * v;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER detail::tvec4<T, P> mul
	(
		detail::tmat4x4<T, P> const & m, 
		detail::tvec4<T, P> const & v
	)
	{
		return m * v;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER detail::tvec2<T, P> mul
	(
		detail::tvec2<T, P> const & v, 
		detail::tmat2x2<T, P> const & m
	)
	{
		return v * m;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER detail::tvec3<T, P> mul
	(
		detail::tvec3<T, P> const & v, 
		detail::tmat3x3<T, P> const & m
	)
	{
		return v * m;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER detail::tvec4<T, P> mul
	(
		detail::tvec4<T, P> const & v, 
		detail::tmat4x4<T, P> const & m
	)
	{
		return v * m;
	}

	template <typename genType>
	GLM_FUNC_QUALIFIER genType div(genType const & a, genType const & b)
	{
		return a / b;
	}

	template <typename genTypeT, typename genTypeU, typename genTypeV> 
	GLM_FUNC_QUALIFIER genTypeT mad(genTypeT const & a, genTypeU const & b, genTypeV const & c)
	{
		return a * b + c;
	}
}//namespace glm
