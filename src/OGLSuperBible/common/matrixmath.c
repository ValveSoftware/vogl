// MatrixMath.c
// Collection of matrix routines that are less suitable for inlining
// glTools Library
// OpenGL SuperBible
// Richard S. Wright Jr.
// Benjamin Lipchak

#include "gltools.h"
#include <math.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// Load a matrix with the Idenity matrix
void gltLoadIdentityMatrix(GLTMatrix m)
	{
	static GLTMatrix identity = { 1.0f, 0.0f, 0.0f, 0.0f,
                                     0.0f, 1.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, 1.0f, 0.0f,
                                     0.0f, 0.0f, 0.0f, 1.0f };

	memcpy(m, identity, sizeof(GLTMatrix));
	}


///////////////////////////////////////////////////////////////////////////////
// Multiply two 4x4 matricies. Assumes normal OpenGL column major ordering
void gltMultiplyMatrix(const GLTMatrix m1, const GLTMatrix m2, GLTMatrix mProduct )
    {
    mProduct[0]     = m1[0] * m2[0] + m1[4] * m2[1] + m1[8] * m2[2] + m1[12] * m2[3];
    mProduct[4]     = m1[0] * m2[4] + m1[4] * m2[5] + m1[8] * m2[6] + m1[12] * m2[7];
    mProduct[8]     = m1[0] * m2[8] + m1[4] * m2[9] + m1[8] * m2[10] + m1[12] * m2[11];
    mProduct[12]    = m1[0] * m2[12] + m1[4] * m2[13] + m1[8] * m2[14] + m1[12] * m2[15];
        
    mProduct[1]     = m1[1] * m2[0] + m1[5] * m2[1] + m1[9] * m2[2] + m1[13] * m2[3];
    mProduct[5]     = m1[1] * m2[4] + m1[5] * m2[5] + m1[9] * m2[6] + m1[13] * m2[7];
    mProduct[9]     = m1[1] * m2[8] + m1[5] * m2[9] + m1[9] * m2[10] + m1[13] * m2[11];
    mProduct[13]    = m1[1] * m2[12] + m1[5] * m2[13] + m1[9] * m2[14] + m1[13] * m2[15];
        
    mProduct[2]     = m1[2] * m2[0] + m1[6] * m2[1] + m1[10] * m2[2] + m1[14] * m2[3];
    mProduct[6]     = m1[2] * m2[4] + m1[6] * m2[5] + m1[10] * m2[6] + m1[14] * m2[7];
    mProduct[10]    = m1[2] * m2[8] + m1[6] * m2[9] + m1[10] * m2[10] + m1[14] * m2[11];
    mProduct[14]    = m1[2] * m2[12] + m1[6] * m2[13] + m1[10] * m2[14] + m1[14] * m2[15];
        
    mProduct[3]     = m1[3] * m2[0] + m1[7] * m2[1] + m1[11] * m2[2] + m1[15] * m2[3];
    mProduct[7]     = m1[3] * m2[4] + m1[7] * m2[5] + m1[11] * m2[6] + m1[15] * m2[7];
    mProduct[11]    = m1[3] * m2[8] + m1[7] * m2[9] + m1[11] * m2[10] + m1[15] * m2[11];
    mProduct[15]    = m1[3] * m2[12] + m1[7] * m2[13] + m1[11] * m2[14] + m1[15] * m2[15];     
    }

//////////////////////////////////////////////////////////////////////////////
// Create a translation matrix
void gltTranslationMatrix(GLfloat x, GLfloat y, GLfloat z, GLTMatrix mTranslate)
    {
    gltLoadIdentityMatrix(mTranslate);
    mTranslate[12] = x;
    mTranslate[13] = y;
    mTranslate[14] = z;
    }
    
///////////////////////////////////////////////////////////////////////////////
// Create a scaling matrix
void gltScalingMatrix(GLfloat x, GLfloat y, GLfloat z, GLTMatrix mScale)
    {
    gltLoadIdentityMatrix(mScale);
    mScale[0] = x;
    mScale[5] = y;
    mScale[11] = z;
    }

///////////////////////////////////////////////////////////////////////////////
// Creates a 4x4 rotation matrix, takes radians NOT degrees
void gltRotationMatrix(float angle, float x, float y, float z, GLTMatrix mMatrix)
    {
    float vecLength, sinSave, cosSave, oneMinusCos;
    float xx, yy, zz, xy, yz, zx, xs, ys, zs;

    // If NULL vector passed in, this will blow up...
    if(x == 0.0f && y == 0.0f && z == 0.0f)
        {
        gltLoadIdentityMatrix(mMatrix);
        return;
        }

    // Scale vector
    vecLength = (float)sqrt( x*x + y*y + z*z );

    // Rotation matrix is normalized
    x /= vecLength;
    y /= vecLength;
    z /= vecLength;
        
    sinSave = (float)sin(angle);
    cosSave = (float)cos(angle);
    oneMinusCos = 1.0f - cosSave;

    xx = x * x;
    yy = y * y;
    zz = z * z;
    xy = x * y;
    yz = y * z;
    zx = z * x;
    xs = x * sinSave;
    ys = y * sinSave;
    zs = z * sinSave;

    mMatrix[0] = (oneMinusCos * xx) + cosSave;
    mMatrix[4] = (oneMinusCos * xy) - zs;
    mMatrix[8] = (oneMinusCos * zx) + ys;
    mMatrix[12] = 0.0f;

    mMatrix[1] = (oneMinusCos * xy) + zs;
    mMatrix[5] = (oneMinusCos * yy) + cosSave;
    mMatrix[9] = (oneMinusCos * yz) - xs;
    mMatrix[13] = 0.0f;

    mMatrix[2] = (oneMinusCos * zx) - ys;
    mMatrix[6] = (oneMinusCos * yz) + xs;
    mMatrix[10] = (oneMinusCos * zz) + cosSave;
    mMatrix[14] = 0.0f;

    mMatrix[3] = 0.0f;
    mMatrix[7] = 0.0f;
    mMatrix[11] = 0.0f;
    mMatrix[15] = 1.0f;
    }

// Creates a shadow projection matrix out of the plane equation
// coefficients and the position of the light. The return value is stored
// in destMat
void gltMakeShadowMatrix(GLTVector3 vPoints[3], GLTVector4 vLightPos, GLTMatrix destMat)
    {
    GLTVector4 vPlaneEquation;
    GLfloat dot;

    gltGetPlaneEquation(vPoints[0], vPoints[1], vPoints[2], vPlaneEquation);
  
    // Dot product of plane and light position
    dot =   vPlaneEquation[0]*vLightPos[0] + 
            vPlaneEquation[1]*vLightPos[1] + 
            vPlaneEquation[2]*vLightPos[2] + 
            vPlaneEquation[3]*vLightPos[3];

    
    // Now do the projection
    // First column
    destMat[0] = dot - vLightPos[0] * vPlaneEquation[0];
    destMat[4] = 0.0f - vLightPos[0] * vPlaneEquation[1];
    destMat[8] = 0.0f - vLightPos[0] * vPlaneEquation[2];
    destMat[12] = 0.0f - vLightPos[0] * vPlaneEquation[3];

    // Second column
    destMat[1] = 0.0f - vLightPos[1] * vPlaneEquation[0];
    destMat[5] = dot - vLightPos[1] * vPlaneEquation[1];
    destMat[9] = 0.0f - vLightPos[1] * vPlaneEquation[2];
    destMat[13] = 0.0f - vLightPos[1] * vPlaneEquation[3];

    // Third Column
    destMat[2] = 0.0f - vLightPos[2] * vPlaneEquation[0];
    destMat[6] = 0.0f - vLightPos[2] * vPlaneEquation[1];
    destMat[10] = dot - vLightPos[2] * vPlaneEquation[2];
    destMat[14] = 0.0f - vLightPos[2] * vPlaneEquation[3];

    // Fourth Column
    destMat[3] = 0.0f - vLightPos[3] * vPlaneEquation[0];
    destMat[7] = 0.0f - vLightPos[3] * vPlaneEquation[1];
    destMat[11] = 0.0f - vLightPos[3] * vPlaneEquation[2];
    destMat[15] = dot - vLightPos[3] * vPlaneEquation[3];
    }
    
////////////////////////////////////////////////////////////////////////////
///
// Transpose the matrix in place
void gltTransposeMatrix(GLTMatrix mTranspose)
    {
    GLfloat temp;

    temp           = mTranspose[ 1];
    mTranspose[ 1] = mTranspose[ 4];
    mTranspose[ 4] = temp;

    temp           = mTranspose[ 2];
    mTranspose[ 2] = mTranspose[ 8];
    mTranspose[ 8] = temp;

    temp           = mTranspose[ 3];
    mTranspose[ 3] = mTranspose[12];
    mTranspose[12] = temp;

    temp           = mTranspose[ 6];
    mTranspose[ 6] = mTranspose[ 9];
    mTranspose[ 9] = temp;

    temp           = mTranspose[ 7];
    mTranspose[ 7] = mTranspose[13];
    mTranspose[13] = temp;

    temp           = mTranspose[11];
    mTranspose[11] = mTranspose[14];
    mTranspose[14] = temp;
    }

////////////////////////////////////////////////////////////////////////////
/// This function is not exported by library, just for this modules use only
// 3x3 determinant
static float DetIJ(const GLTMatrix m, int i, int j)
    {
    int x, y, ii, jj;
    float ret, mat[3][3];

    x = 0;
    for (ii = 0; ii < 4; ii++)
        {
        if (ii == i) continue;
        y = 0;
        for (jj = 0; jj < 4; jj++)
            {
            if (jj == j) continue;
            mat[x][y] = m[(ii*4)+jj];
            y++;
            }
        x++;
        }

    ret =  mat[0][0]*(mat[1][1]*mat[2][2]-mat[2][1]*mat[1][2]);
    ret -= mat[0][1]*(mat[1][0]*mat[2][2]-mat[2][0]*mat[1][2]);
    ret += mat[0][2]*(mat[1][0]*mat[2][1]-mat[2][0]*mat[1][1]);

    return ret;
    }

////////////////////////////////////////////////////////////////////////////
///
// Invert matrix
void gltInvertMatrix(const GLTMatrix m, GLTMatrix mInverse)
    {
    int i, j;
    float det, detij;

    // calculate 4x4 determinant
    det = 0.0f;
    for (i = 0; i < 4; i++)
        {
        det += (i & 0x1) ? (-m[i] * DetIJ(m, 0, i)) : (m[i] * DetIJ(m, 0,i));
        }
    det = 1.0f / det;

    // calculate inverse
    for (i = 0; i < 4; i++)
        {
        for (j = 0; j < 4; j++)
            {
            detij = DetIJ(m, j, i);
            mInverse[(i*4)+j] = ((i+j) & 0x1) ? (-detij * det) : (detij *det); 
            }
        }
    }

