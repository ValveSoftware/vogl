# Copyright (c) 1991-2005 Silicon Graphics, Inc. All Rights Reserved.
# Copyright (c) 2006-2010 The Khronos Group, Inc.
#
# This document is licensed under the SGI Free Software B License Version
# 2.0. For details, see http://oss.sgi.com/projects/FreeB/ .
#
# $Revision: 17025 $ on $Date: 2012-03-05 03:01:59 -0800 (Mon, 05 Mar 2012) $

required-props:
param:		retval retained
dlflags:	notlistable handcode nop
glxflags:	client-handcode server-handcode
glxvendorglx:	*
vectorequiv:	*
category:	pixel-rw bgn-end display-list drawing drawing-control feedback framebuf misc modeling pixel-op pixel-rw state-req xform glx glxopcode
glxopcode:		*

###############################################################################
#
# Missing commands
#
###############################################################################
ChooseVisual(dpy, screen, attribList)
	return 		XVisualInfoPointer
	param		dpy		Display in reference
	param		screen	Int32 in value
	param		attribList	Int32 in array[]
	category	glx

GetConfig(dpy, vis, attrib, value)
	return 		Int32
	param		dpy		Display in reference
	param		vis XVisualInfo in reference
	param		attrib Int32 in value
	param		value Int32 out reference 
	category	glx	
	
QueryExtension(dpy, errorBase, eventBase)
	return		Bool
	param 		dpy Display in reference
	param		errorBase Int32 out reference
	param		eventBase Int32 out reference
	category	glx

GetClientString(dpy, name)
	return		String
	param 		dpy Display in reference	
	param		name Int32 in value
	category	glx
	
GetCurrentDrawable(dpy, name)
	return		GLXDrawable
	category	glx


###############################################################################
#
# GLX1.0 commands
#
###############################################################################

Render()
	return		 void
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 1


RenderLarge()
	return		 void
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 2


CreateContext(dpy, vis, shareList, direct)
	return		 GLXContext
	param		 dpy		Display in reference
	param		 vis		XVisualInfo in reference
	param		 shareList		GLXContext in value
	param		 direct Bool in value
	glxflags	 client-handcode server-handcode
	category	 glx
	dlflags		 notlistable
	glxopcode	 3


DestroyContext(dpy, context)
	return		 void
	param		 dpy		Display in reference
	param		 context       GLXContext in value
	glxflags	 client-handcode server-handcode
	category	 glx
	dlflags		 notlistable
	glxopcode	 4


MakeCurrent(dpy, drawable, context)
	return		 Bool
	param		 dpy		Display in reference
	param		 drawable	GLXDrawable in value
	param		 context       GLXContext in value
	glxflags	 client-handcode server-handcode
	category	 glx
	dlflags		 notlistable
	glxopcode	 5


GetCurrentContext()
	return		GLXContext
	category glx

IsDirect(dpy, context)
	return		Bool
	param		dpy		Display in reference
	param		context		GLXContext in value
	glxflags	client-handcode server-handcode
	category	glx
	dlflags		notlistable
	glxopcode	6


QueryVersion(dpy, major, minor)
	return		 Bool
	param		 dpy		Display in reference
	param		 major		Int32 out reference
	param		 minor		Int32 out reference
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 7


WaitGL()
	return		 void
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 8


WaitX()
	return		 void
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 9


CopyContext(dpy, source, dest, mask)
	return		 void
	param		 dpy		Display in reference
	param		 source		GLXContext in value
	param		 dest		GLXContext in value
	param		 mask		ulong in value
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 10


SwapBuffers(dpy, drawable)
	return		 void
	param		 dpy		Display in reference
	param		 drawable	GLXDrawable in value
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 11


UseXFont(font, first, count, list_base)
	return		 void
	param		 font		Font in value
	param		 first		Int32 in value
	param		 count		Int32 in value
	param		 list_base	Int32 in value
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 12


CreateGLXPixmap(dpy, visual, pixmap)
	return		 GLXPixmap
	param		 dpy		Display in reference
	param		 visual		XVisualInfo in reference
	param		 pixmap		Pixmap in value
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 13

GetVisualConfigs()
	return		 void
	category	 glx
	dlflags		 notlistable
	glxflags	 client-handcode server-handcode
	glxopcode	 14


DestroyGLXPixmap(dpy, pixmap)
	return		 void
	param		 dpy		Display in reference
	param		 pixmap		GLXPixmap in value
	glxflags	 client-handcode
	category	 glx
	dlflags		 notlistable
	glxopcode	 15


VendorPrivate()
	return		void
	glxflags	client-handcode server-handcode
	category	glx
	dlflags		notlistable
	glxopcode	16


VendorPrivateWithReply()
	return		void
	glxflags	client-handcode server-handcode
	category	glx
	dlflags		notlistable
	glxopcode	17

###############################################################################
#
# GLX1.1 commands
#
###############################################################################
QueryExtensionsString(dpy, screen)
	return		CharPointer
	param		dpy		Display in reference
	param		screen		Int32 in value
	glxflags	client-handcode server-handcode
	category	glx
	dlflags		notlistable
	glxopcode	18

QueryServerString(dpy, screen, name)
	return		CharPointer
	param		dpy		Display in reference
	param		screen		Int32 in value
	param		name		Int32 in value
	glxflags	client-handcode server-handcode
	category	glx
	dlflags		notlistable
	glxopcode	19

ClientInfo()
	return		void
	glxflags	client-handcode server-handcode
	category	glx
	dlflags		notlistable
	glxopcode	20

###############################################################################
#
# GLX1.3 commands
#
###############################################################################
# defined in glxext.spec
#GetFBConfigs()
#	return		void
#	category	glx
#	dlflags		notlistable
#	glxflags	client-handcode server-handcode
#	glxopcode	21

# defined in glxext.spec
#CreatePixmap(config, pixmap, glxpixmap)
#	return		void
#	param		config		Int32 in value
#	param		pixmap		Int32 in value
#	param		glxpixmap	Int32 in value
#	dlflags		notlistable
#	glxflags	client-handcode server-handcode
#	category	glx
#	glxopcode	22

# defined in glxext.spec
#DestroyPixmap(glxpixmap)
#	return		void
#	param		glxpixmap	Int32 in value
#	dlflags		notlistable
#	glxflags	client-handcode server-handcode
#	category	glx
#	glxopcode	23

# defined in glxext.spec
#CreateNewContext(config, render_type, share_list, direct)
#	return		void
#	param		config		Int32 in value
#	param		render_type	Int32 in value
#	param		share_list	Int32 in value
#	param		direct		Int32 in value
#	dlflags		notlistable
#	glxflags	client-handcode server-handcode
#	category	glx
#	glxopcode	24

# defined in glxext.spec
#QueryContext()
#	return		void
#	dlflags		notlistable
#	glxflags	client-handcode server-handcode
#	category	glx
#	glxopcode	25

# defined in glxext.spec
#MakeContextCurrent(drawable, readdrawable, context)
#	return		void
#	param		drawable	GLXDrawable in value
#	param		readdrawable	GLXDrawable in value
#	param		context		Int32 in value
#	dlflags		notlistable
#	glxflags	client-handcode server-handcode
#	category	glx
#	glxopcode	26

# defined in glxext.spec
#CreatePbuffer(config, pbuffer)
#	return		void
#	param		config		Int32 in value
#	param		pbuffer		Int32 in value
#	dlflags		notlistable
#	glxflags	client-handcode server-handcode
#	category	glx
#	glxopcode	27

# defined in glxext.spec
#DestroyPbuffer(pbuffer)
#	return		void
#	param		pbuffer		Int32 in value
#	dlflags		notlistable
#	glxflags	client-handcode
#	category	glx
#	glxopcode	28

# this func seems dead
GetDrawableAttributes(drawable)
	return		void
	param		drawable	GLXDrawable in value
	dlflags		notlistable
	glxflags	client-handcode server-handcode
	category	glx
	glxopcode	29

# this func seems dead
ChangeDrawableAttributes(drawable)
	return		void
	param		drawable	GLXDrawable in value
	dlflags		notlistable
	glxflags	client-handcode server-handcode
	category	glx
	glxopcode	30

# defined in glxext.spec
#CreateWindow(config, window, glxwindow)
#	return		void
#	param		config		Int32 in value
#	param		window		Int32 in value
#	param		glxwindow	Int32 in value
#	dlflags		notlistable
#	glxflags	client-handcode server-handcode
#	category	glx
#	glxopcode	31

# defined in glxext.spec
#DestroyWindow(glxwindow)
#	return		void
#	param		glxwindow	Int32 in value
#	dlflags		notlistable
#	glxflags	client-handcode server-handcode
#	category	glx
#	glxopcode	32

###############################################################################
#
# EXT_import_context extension commands
#
###############################################################################
# defined in glxext.spec
#QueryContextInfoEXT()
#	return		 void
#	category	 glx
#	dlflags		 notlistable
#	glxflags	 client-handcode server-handcode
#	glxvendorglx	 1024

