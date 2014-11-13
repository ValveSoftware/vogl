# Hand generated
required-props:
param:			retval retained
dlflags:		notlistable handcode nop
aglflags:		client-handcode server-handcode
vectorequiv:	*
category:		cgl

CheckExtension(extName, extString)
	return 		GLboolean
	param		extName				GLubyte					in  reference
	param		extString			GLubyte					in  reference
	category	glu

ChoosePixelFormat(attribs, pix, npix)
	return 		CGLError
	param		attribs				CGLPixelFormatAttribute	in  reference
	param		pix					CGLPixelFormatObj		out	reference
	param		npix				GLint					out	reference
	category	cgl

ClearDrawable(ctx)
	return 		CGLError
	param		ctx					CGLContextObj			in value
	category	cgl

CopyContext(src, dst, mask)
	return 		CGLError
	param		src					CGLContextObj			in  value
	param		dst					CGLContextObj			in  value
	param		mask				GLbitfield				in  value
	category	cgl

CreateContext(pix, share, ctx)
	return 		CGLError
	param		pix					CGLPixelFormatObj		in  value
	param		share				CGLContextObj			in  value
	param		ctx					CGLContextObj			out reference
	category	cgl

CreatePBuffer(width, height, target, internalFormat, max_level, pbuffer)
	return 		CGLError
	param		width				GLsizei					in  value
	param		height				GLsizei					in  value
	param		target				GLenum					in  value
	param		internalFormat		GLenum					in  value
	param		max_level			GLint					in  value
	param		pbuffer				CGLPBufferObj			out reference
	category	cgl

DescribePBuffer(obj, width, height, target, internalFormat, mipmap)
	return 		CGLError
	param		obj					CGLPBufferObj			in  value
	param		width				GLsizei					out reference
	param		height				GLsizei					out reference
	param		target				GLenum					out reference
	param		internalFormat		GLenum					out reference
	param		mipmap				GLint					out reference
	category	cgl

DescribePixelFormat(pix, pix_num, attrib, value)
	return 		CGLError
	param		pix					CGLPixelFormatObj       in  value
	param		pix_num				GLint                   in  value
	param		attrib				CGLPixelFormatAttribute in  value
	param		value				GLint                   out reference
	category	cgl

DescribeRenderer(rend, rend_num, prop, value)
	return 		CGLError
	param		rend				CGLRendererInfoObj		in  value
	param		rend_num			GLint					in  value
	param		prop				CGLRendererProperty		in  value
	param		value				GLint					out	reference
	category	cgl

DestroyContext(ctx)
	return 		CGLError
	param		ctx					CGLContextObj			in value
	category	cgl

DestroyPBuffer(pbuffer)
	return 		CGLError
	param		pbuffer				CGLPBufferObj			in value
	category	cgl

DestroyPixelFormat(pix)
	return 		CGLError
	param		pix					CGLPixelFormatObj       in  value
	category	cgl

DestroyRendererInfo(rend)
	return 		CGLError
	param		rend				CGLRendererInfoObj		in  value
	category	cgl

Disable(rend, pname)
	return 		CGLError
	param		rend				CGLRendererInfoObj		in  value
	param		pname				CGLContextEnable		in  value
	category	cgl

Enable(ctx, pname)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		pname				CGLContextEnable		in  value
	category	cgl

ErrorString(error)
	return 		CharPointer
	param		error				CGLError				in  value
	category	cgl

FlushDrawable(ctx)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	category	cgl

GetCurrentContext()
	return 		CGLContextObj
	category	cgl

GetOffScreen(ctx, width, height, rowbytes, baseaddr)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		width				GLsizei					out reference
	param		height				GLsizei					out reference
	param		rowbytes			GLint					out reference
	param		baseaddr			VoidPointer				out reference
	category	cgl

GetOption(pname, param)
	return 		CGLError
	param		pname				CGLGlobalOption			in  value
	param		param				GLint					out reference
	category	cgl

GetParameter(ctx, pname, params)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		pname				CGLContextParameter		in  value
	param		params				GLint					out reference
	category	cgl

GetPBuffer(ctx, pbuffer, face, level, screen)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		pbuffer				CGLPBufferObj			out reference
	param		face				GLenum					out reference
	param		level				GLint					out reference
	param		screen				GLint					out reference
	category	cgl

GetPixelFormat(ctx)
	return 		CGLPixelFormatObj
	param		ctx					CGLContextObj			in value
	category	cgl

GetShareGroup(ctx)
	return 		CGLShareGroupObj
	param		ctx					CGLContextObj			in value
	category	cgl

GetSurface(ctx, cid, wid, sid)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		cid					CGSConnectionID			out reference
	param		wid					CGSWindowID				out reference
	param		sid					CGSSurfaceID			out reference
	category	cgl

GetVersion(majorvers, minorvers)
	return 		void
	param		majorvers			GLint					out reference
	param		minorvers			GLint					out reference
	category	cgl

GetVirtualScreen(ctx, screen)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		screen				GLint					out reference
	category	cgl

LockContext(ctx)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	category	cgl

LookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ)
	return 		void
	param		eyeX				GLdouble				in  value
	param		eyeY				GLdouble				in  value
	param		eyeZ				GLdouble				in  value
	param		centerX				GLdouble				in  value
	param		centerY				GLdouble				in  value
	param		centerZ				GLdouble				in  value
	param		upX					GLdouble				in  value
	param		upY					GLdouble				in  value
	param		upZ					GLdouble				in  value
	category	glu

QueryRendererInfo(display_mask, rend, nrend)
	return 		CGLError
	param		display_mask		GLuint					in  value
	param		rend				CGLRendererInfoObj		out reference
	param		nrend				GLint					out reference
	category	cgl

ReleaseContext(ctx)
	return 		void
	param		ctx					CGLContextObj			in  value
	category	cgl

ReleasePixelFormat(pix)
	return 		void
	param		pix					CGLPixelFormatObj       in  value
	category	cgl

RetainContext(ctx)
	return 		CGLContextObj
	param		ctx					CGLContextObj			in  value
	category	cgl

RetainPBuffer(pbuffer)
	return 		CGLPBufferObj
	param		pbuffer				CGLPBufferObj			in  value
	category	cgl

RetainPixelFormat(pix)
	return 		CGLPixelFormatObj
	param		pix					CGLPixelFormatObj       in  value
	category	cgl

SetCurrentContext(ctx)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	category	cgl

SetFullScreen(ctx)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	category	cgl

SetGlobalOption(pname, params)
	return 		CGLError
	param		pname				CGLGlobalOption			in  value
	param		params				GLint					out reference
	category	cgl

SetOffScreen(ctx, width, height, rowbytes, baseaddr)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		width				GLsizei					in  value
	param		height				GLsizei					in  value
	param		rowbytes			GLint					in  value
	param		baseaddr			void					out reference
	category	cgl

SetOption(pname, param)
	return 		CGLError
	param		pname				CGLGlobalOption			in  value
	param		param				GLint					in  value
	category	cgl

SetParameter(ctx, pname, params)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		pname				CGLContextParameter		in  value
	param		params				GLint					in	reference
	category	cgl

SetPBuffer(ctx, pbuffer, face, level, screen)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		obj					CGLPBufferObj			in  value
	param		face				GLenum					in  value
	param		level				GLint					in  value
	param		screen				GLint					in  value
	category	cgl

SetSurface(ctx, cid, wid, sid)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		cid					CGSConnectionID			in  value
	param		wid					CGSWindowID				in  value
	param		sid					CGSSurfaceID			in  value
	category	cgl

SetVirtualScreen(ctx, screen)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		screen				GLint					in	value
	category	cgl

TexImageIOSurface2D(ctx, target, internal_format, width, height, format, type, ioSurface, plane)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		target				GLenum					in  value
	param		internal_format		GLenum					in  value
	param		width				GLsizei					in  value
	param		height				GLsizei					in  value
	param		format				GLenum					in  value
	param		type				GLenum					in  value
	param		ioSurface			IOSurfaceRef			in  value
	param		plane				GLuint					in  value
	category	cgl

TexImagePBuffer(ctx, pbuffer, source)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	param		pbuffer				CGLPBufferObj			in  value
	param		source				GLenum					in  value
	category	cgl

UnlockContext(ctx)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	category	cgl

UpdateContext(ctx)
	return 		CGLError
	param		ctx					CGLContextObj			in  value
	category	cgl



