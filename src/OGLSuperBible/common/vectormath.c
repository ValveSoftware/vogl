// Vector Math Functions
// OpenGL SuperBible gltools library
// Richard S. Wright Jr.
// One note of interest. These REALLY should be inlined. I'm sorry I was too stupid at the time
// to figure out how to get inlining to work on all the platforms and different compilers I use.
// this seemed simpler than a ton of #ifdef's all over the place. Watch the web site as I may
// make frequent improvments to the library.

#include "gltools.h"
#include <math.h>
#include <string.h>

// Adds two vectors together
void gltAddVectors(const GLTVector3 vFirst, const GLTVector3 vSecond, GLTVector3 vResult) {
    vResult[0] = vFirst[0] + vSecond[0];
    vResult[1] = vFirst[1] + vSecond[1];
    vResult[2] = vFirst[2] + vSecond[2];
    }

// Subtract one vector from another
void gltSubtractVectors(const GLTVector3 vFirst, const GLTVector3 vSecond, GLTVector3 vResult) 
    {
    vResult[0] = vFirst[0] - vSecond[0];
    vResult[1] = vFirst[1] - vSecond[1];
    vResult[2] = vFirst[2] - vSecond[2];
    }

// Scales a vector by a scalar
void gltScaleVector(GLTVector3 vVector, const GLfloat fScale)
    { 
    vVector[0] *= fScale; vVector[1] *= fScale; vVector[2] *= fScale; 
    }

// Gets the length of a vector squared
GLfloat gltGetVectorLengthSqrd(const GLTVector3 vVector)
    { 
    return (vVector[0]*vVector[0]) + (vVector[1]*vVector[1]) + (vVector[2]*vVector[2]); 
    }
    
// Gets the length of a vector
GLfloat gltGetVectorLength(const GLTVector3 vVector)
    { 
    return (GLfloat)sqrt(gltGetVectorLengthSqrd(vVector)); 
    }
    
// Scales a vector by it's length - creates a unit vector
void gltNormalizeVector(GLTVector3 vNormal)
    { 
    GLfloat fLength = 1.0f / gltGetVectorLength(vNormal);
    gltScaleVector(vNormal, fLength); 
    }
    
// Copies a vector
void gltCopyVector(const GLTVector3 vSource, GLTVector3 vDest)
    { 
    memcpy(vDest, vSource, sizeof(GLTVector3)); 
    }

// Get the dot product between two vectors
GLfloat gltVectorDotProduct(const GLTVector3 vU, const GLTVector3 vV)
    {
    return vU[0]*vV[0] + vU[1]*vV[1] + vU[2]*vV[2]; 
    }

// Calculate the cross product of two vectors
void gltVectorCrossProduct(const GLTVector3 vU, const GLTVector3 vV, GLTVector3 vResult)
	{
	vResult[0] = vU[1]*vV[2] - vV[1]*vU[2];
	vResult[1] = -vU[0]*vV[2] + vV[0]*vU[2];
	vResult[2] = vU[0]*vV[1] - vV[0]*vU[1];
	}



// Given three points on a plane in counter clockwise order, calculate the unit normal
void gltGetNormalVector(const GLTVector3 vP1, const GLTVector3 vP2, const GLTVector3 vP3, GLTVector3 vNormal)
    {
    GLTVector3 vV1, vV2;
    
    gltSubtractVectors(vP2, vP1, vV1);
    gltSubtractVectors(vP3, vP1, vV2);
    
    gltVectorCrossProduct(vV1, vV2, vNormal);
    gltNormalizeVector(vNormal);
    }



// Transform a point by a 4x4 matrix
void gltTransformPoint(const GLTVector3 vSrcVector, const GLTMatrix mMatrix, GLTVector3 vOut)
    {
    vOut[0] = mMatrix[0] * vSrcVector[0] + mMatrix[4] * vSrcVector[1] + mMatrix[8] *  vSrcVector[2] + mMatrix[12];
    vOut[1] = mMatrix[1] * vSrcVector[0] + mMatrix[5] * vSrcVector[1] + mMatrix[9] *  vSrcVector[2] + mMatrix[13];
    vOut[2] = mMatrix[2] * vSrcVector[0] + mMatrix[6] * vSrcVector[1] + mMatrix[10] * vSrcVector[2] + mMatrix[14];    
    }

// Rotates a vector using a 4x4 matrix. Translation column is ignored
void gltRotateVector(const GLTVector3 vSrcVector, const GLTMatrix mMatrix, GLTVector3 vOut)
    {
    vOut[0] = mMatrix[0] * vSrcVector[0] + mMatrix[4] * vSrcVector[1] + mMatrix[8] *  vSrcVector[2];
    vOut[1] = mMatrix[1] * vSrcVector[0] + mMatrix[5] * vSrcVector[1] + mMatrix[9] *  vSrcVector[2];
    vOut[2] = mMatrix[2] * vSrcVector[0] + mMatrix[6] * vSrcVector[1] + mMatrix[10] * vSrcVector[2];    	
    }


// Gets the three coefficients of a plane equation given three points on the plane.
void gltGetPlaneEquation(GLTVector3 vPoint1, GLTVector3 vPoint2, GLTVector3 vPoint3, GLTVector3 vPlane)
    {
    // Get normal vector from three points. The normal vector is the first three coefficients
    // to the plane equation...
    gltGetNormalVector(vPoint1, vPoint2, vPoint3, vPlane);
    
    // Final coefficient found by back substitution
    vPlane[3] = -(vPlane[0] * vPoint3[0] + vPlane[1] * vPoint3[1] + vPlane[2] * vPoint3[2]);
    }
    
// Determine the distance of a point from a plane, given the point and the
// equation of the plane.
GLfloat gltDistanceToPlane(GLTVector3 vPoint, GLTVector4 vPlane)
    {
    return vPoint[0]*vPlane[0] + vPoint[1]*vPlane[1] + vPoint[2]*vPlane[2] + vPlane[3];
    }
    