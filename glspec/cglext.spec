# Hand generated
required-props:
param:			retval retained
dlflags:		notlistable handcode nop
aglflags:		client-handcode server-handcode
vectorequiv:	*
category:		gl

DeleteFencesAPPLE(n, fences)
	return 		void
	param		n					GLint					in  value
	param		fences				GLuint					in  reference
	category	gl

FinishFenceAPPLE(fence)
	return 		void
	param		fence				GLuint					in  value
	category	gl

FlushRenderAPPLE()
	return 		void
	category	gl

GenFencesAPPLE(n, fences)
	return 		void
	param		n					GLint					in  value
	param		fences				GLuint					out reference
	category	gl

SetFenceAPPLE(fence)
	return 		void
	param		fence				GLuint					in  value
	category	gl

TestFenceAPPLE(fence)
	return 		GLboolean
	param		fence				GLuint					in  value
	category	gl



