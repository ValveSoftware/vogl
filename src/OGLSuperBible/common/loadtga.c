// LoadTGA.c
// Loads a Targa file for OpenGL
// Richard S. Wright Jr.
// OpenGL SuperBible
// This really only works with 24/32/8 bit targas

#include "gltools.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Define targa header.
#pragma pack(1)
typedef struct
    {
    GLbyte	identsize;              // Size of ID field that follows header (0)
    GLbyte	colorMapType;           // 0 = None, 1 = paletted
    GLbyte	imageType;              // 0 = none, 1 = indexed, 2 = rgb, 3 = grey, +8=rle
    unsigned short	colorMapStart;          // First colour map entry
    unsigned short	colorMapLength;         // Number of colors
    unsigned char 	colorMapBits;   // bits per palette entry
    unsigned short	xstart;                 // image x origin
    unsigned short	ystart;                 // image y origin
    unsigned short	width;                  // width in pixels
    unsigned short	height;                 // height in pixels
    GLbyte	bits;                   // bits per pixel (8 16, 24, 32)
    GLbyte	descriptor;             // image descriptor
    } TGAHEADER;
#pragma pack(8)



////////////////////////////////////////////////////////////////////
// Allocate memory and load targa bits. Returns pointer to new buffer,
// height, and width of texture, and the OpenGL format of data.
// Call free() on buffer when finished!
// This only works on pretty vanilla targas... 8, 24, or 32 bit color
// only, no palettes, no RLE encoding.
GLbyte *gltLoadTGA(const char *szFileName, GLint *iWidth, GLint *iHeight, GLint *iComponents, GLenum *eFormat)
    {
    FILE *pFile;			// File pointer
    TGAHEADER tgaHeader;		// TGA file header
    unsigned long lImageSize;		// Size in bytes of image
    short sDepth;			// Pixel depth;
    GLbyte	*pBits = NULL;          // Pointer to bits
    
    // Default/Failed values
    *iWidth = 0;
    *iHeight = 0;
    *eFormat = GL_BGR_EXT;
    *iComponents = GL_RGB8;
    
    // Attempt to open the file
    pFile = fopen(szFileName, "rb");
    if(pFile == NULL)
    {
#ifdef __linux__
        // Assuming the name of our app is 'sphereworld_ch9' and we're trying to load 'grass.tga',
        // check for the filename 'sphereworld_ch9_grass.tga' if we failed to find 'grass.tga'.

        // Get name of exe.
        char buf[1024];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf));
        if (len > 0)
        {
            // Get basename of exe.
            buf[len] = 0;
            char *bname = strrchr(buf, '/');
            if (bname)
            {
                char filename[1024];
                snprintf(filename, sizeof(filename), "%s_%s", bname + 1, szFileName);
                filename[sizeof(filename) - 1] = 0;

                // Try to open this new filename.
                pFile = fopen(filename, "rb");
                if(pFile == NULL)
                    fprintf(stderr, "gltLoadTGA failed to load %s\n", filename);
            }
        }
        if(pFile == NULL)
        {
            fprintf(stderr, "gltLoadTGA failed to load %s\n", szFileName);
            return NULL;
        }
#else
        return NULL;
#endif
    }
        
    // Read in header (binary)
    fread(&tgaHeader, 18/* sizeof(TGAHEADER)*/, 1, pFile);
    
    // Do byte swap for big vs little endian
#ifdef __APPLE__
    BYTE_SWAP(tgaHeader.colorMapStart);
    BYTE_SWAP(tgaHeader.colorMapLength);
    BYTE_SWAP(tgaHeader.xstart);
    BYTE_SWAP(tgaHeader.ystart);
    BYTE_SWAP(tgaHeader.width);
    BYTE_SWAP(tgaHeader.height);
#endif

        
    // Get width, height, and depth of texture
    *iWidth = tgaHeader.width;
    *iHeight = tgaHeader.height;
    sDepth = tgaHeader.bits / 8;
    
    // Put some validity checks here. Very simply, I only understand
    // or care about 8, 24, or 32 bit targa's.
    if(tgaHeader.bits != 8 && tgaHeader.bits != 24 && tgaHeader.bits != 32)
        return NULL;

    // Calculate size of image buffer
    lImageSize = tgaHeader.width * tgaHeader.height * sDepth;
    
    // Allocate memory and check for success
    pBits = malloc(lImageSize * sizeof(GLbyte));
    if(pBits == NULL)
        return NULL;
    
    // Read in the bits
    // Check for read error. This should catch RLE or other 
    // weird formats that I don't want to recognize
    if(fread(pBits, lImageSize, 1, pFile) != 1)
        {
        free(pBits);
        return NULL;
        }
    
    // Set OpenGL format expected
    switch(sDepth)
        {
        case 3:     // Most likely case
            *eFormat = GL_BGR_EXT;
            *iComponents = GL_RGB8;
            break;
        case 4:
            *eFormat = GL_BGRA_EXT;
            *iComponents = GL_RGBA8;
            break;
        case 1:
            *eFormat = GL_LUMINANCE;
            *iComponents = GL_LUMINANCE8;
            break;
        };
        
    
    // Done with File
    fclose(pFile);
        
    // Return pointer to image data
    return pBits;
    }
