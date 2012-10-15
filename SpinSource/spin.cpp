//////////////////////////////////////////////////////////////
//                                                          //
// Propeller Spin/PASM Compiler Command Line Tool           //
// (c)2012 Parallax Inc. DBA Parallax Semiconductor.        //
// Adapted from Jeff Martin's Delphi code by Roy Eltham     //
// See end of file for terms of use.                        //
//                                                          //
//////////////////////////////////////////////////////////////
//
// PropCom.cpp
//

//
// define USE_PREPROCESSOR to enable the preprocessor
//
#define USE_PREPROCESSOR

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../PropellerCompiler/PropellerCompiler.h"

#ifdef USE_PREPROCESSOR
#include "preprocess.h"

struct preprocess preproc;
bool s_bPreprocess = false;
#endif

#define DataLimitStr        "64k"
#define ImageLimit          32768   // Max size of Propeller Application image file
#define ObjFileStackLimit   16

#define ListLimit           5000000
#define DocLimit            5000000
#define MaxObjInHeap        256

// {Object heap (compile-time objects)}
struct ObjHeap
{
    char*   ObjFilename;    // {Full filename of object}
    char*   Obj;            // {Object binary}
    int     ObjSize;        // {Size of object}
};

ObjHeap s_ObjHeap[MaxObjInHeap];
int     s_nObjHeapIndex = 0;
int     s_nObjStackPtr = 0;

CompilerData* s_pCompilerData = NULL;

bool CompileRecursively(char* pFilename);
void ComposeRAM(unsigned char** ppBuffer, int& bufferSize, bool bDATonly);

//
// code for handling directory paths (used with -I option)
//

#define PATH_MAX 256
#if defined(WIN32)
#define DIR_SEP     '\\'
#define DIR_SEP_STR "\\"
#else
#define DIR_SEP     '/'
#define DIR_SEP_STR "/"
#endif

struct PathEntry
{
    PathEntry *next;
    char path[1];
};

static PathEntry *path = NULL;
static PathEntry **pNextPathEntry = &path;

static PathEntry *define = NULL;
static PathEntry **pNextDefineEntry = &define;

static const char *MakePath(PathEntry *entry, const char *name)
{
    static char fullpath[PATH_MAX];
    sprintf(fullpath, "%s%c%s", entry->path, DIR_SEP, name);
    return fullpath;
}

bool AddPath(const char *path)
{
    PathEntry* entry = (PathEntry*)new char[(sizeof(PathEntry) + strlen(path))];
    if (!(entry))
    {
        return false;
    }
    strcpy(entry->path, path);
    *pNextPathEntry = entry;
    pNextPathEntry = &entry->next;
    entry->next = NULL;
    return true;
}

bool AddFilePath(const char *name)
{
    const char* end = strrchr(name, DIR_SEP);
    if (!end)
    {
        return false;
    }
    int len = (int)(end - name);
    PathEntry *entry = (PathEntry*)new char[(sizeof(PathEntry) + len)];
    if (!entry)
    {
        return false;
    }
    strncpy(entry->path, name, len);
    entry->path[len] = '\0';
    *pNextPathEntry = entry;
    pNextPathEntry = &entry->next;
    entry->next = NULL;

    return true;
}

FILE* OpenFileInPath(const char *name, const char *mode)
{
    FILE* file = fopen(name, mode);
    if (!file)
    {
        for (PathEntry* entry = path; entry != NULL; entry = entry->next)
        {
            file = fopen(MakePath(entry, name), mode);
            if (file != NULL)
            {
                break;
            }
        }
    }
    return file;
}

bool AddDefine(const char *define)
{
    PathEntry* entry = (PathEntry*)new char[(sizeof(PathEntry) + strlen(define))];
    if (!(entry))
    {
        return false;
    }
    strcpy(entry->path, define);
    *pNextDefineEntry = entry;
    pNextDefineEntry = &entry->next;
    entry->next = NULL;
    return true;
}

static void Banner(void)
{
    fprintf(stdout, "Propeller Spin/PASM Compiler (c)2012 Parallax Inc. DBA Parallax Semiconductor.\n");
    fprintf(stdout, "Compiled on %s\n",__DATE__);
}

/* Usage - display a usage message and exit */
static void Usage(void)
{
    Banner();
#ifdef USE_PREPROCESSOR
    fprintf(stderr, "\
usage: spin\n\
         [ -I <path> ]     add a directory to the include path\n\
         [ -o <path> ]     output filename\n\
         [ -c ]            output only DAT sections\n\
         [ -d ]            dump out doc mode\n\
         [ -q ]            quiet mode (suppress banner and non-error text)\n\
         [ -v ]            verbose output\n\
         [ -p ]            use preprocessor\n\
         [ -D <define> ]   add a define (must have -p before any of these)\n\
         <name.spin>       spin file to compile\n\
\n");
#else
    fprintf(stderr, "\
usage: spin\n\
         [ -I <path> ]     add a directory to the include path\n\
         [ -o <path> ]     output filename\n\
         [ -c ]            output only DAT sections\n\
         [ -d ]            dump out doc mode\n\
         [ -q ]            quiet mode (suppress banner and non-error text)\n\
         [ -v ]            verbose output\n\
         <name.spin>       spin file to compile\n\
\n");
#endif
}

int main(int argc, char* argv[])
{
    char* infile = NULL;
    char* outfile = NULL;
    char* p = NULL;
    bool bVerbose = false;
    bool bQuiet = false;
    bool bDocMode = false;
    bool bDATonly = false;
    s_bPreprocess = false;

    // get the arguments
    for(int i = 1; i < argc; i++)
    {
        // handle switches
        if(argv[i][0] == '-')
        {
            switch(argv[i][1])
            {
            case 'I':
                if(argv[i][2])
                {
                    p = &argv[i][2];
                }
                else if(++i < argc)
                {
                    p = argv[i];
                }
                else
                {
                    Usage();
                    return 1;
                }
                AddPath(p);
                break;

            case 'o':
                if(argv[i][2])
                {
                    outfile = &argv[i][2];
                }
                else if(++i < argc)
                {
                    outfile = argv[i];
                }
                else
                {
                    Usage();
                    return 1;
                }
                break;

#ifdef USE_PREPROCESSOR
            case 'p':
                s_bPreprocess = true;
                break;

            case 'D':
                if (s_bPreprocess)
                {
                    if (argv[i][2])
                    {
                        p = &argv[i][2];
                    }
                    else if(++i < argc)
                    {
                        p = argv[i];
                    }
                    else
                    {
                        Usage();
                        return 1;
                    }
                    AddDefine(p);
                }
                else
                {
                    Usage();
                    return 1;
                }
                break;
#endif
            case 'c':
                bDATonly = true;
                break;

            case 'd':
                bDocMode = true;
                break;

            case 'q':
                bQuiet = true;
                break;

            case 'v':
                bVerbose = true;
                break;

            default:
                Usage();
                return 1;
                break;
            }
        }
        else // handle the input filename
        {
            if (infile)
            {
                Usage();
                return 1;
            }
            infile = argv[i];
        }
    }

#ifdef USE_PREPROCESSOR
    if (s_bPreprocess)
    {
        pp_init(&preproc);

        // add any predefined symbols here
        for (PathEntry* entry = define; entry != NULL; entry = entry->next)
        {
            pp_define(&preproc, entry->path, "1");
        }

        pp_define(&preproc, "__SPIN__", "1");
        pp_define(&preproc, "__TARGET__", "P1");


        pp_setcomments(&preproc, "\'", "{", "}");

    }
#endif

    // must have input file
    if (!infile)
    {
        Usage();
        return 1;
    }

    // finish the include path
    AddFilePath(infile);

    char outputFilename[256];
    if (!outfile)
    {
        // create *.binary filename from user passed in spin filename
        strcpy(&outputFilename[0], infile);
        const char* pTemp = strstr(&outputFilename[0], ".spin");
        if (pTemp == 0)
        {
            printf("ERROR: spinfile must have .spin extension. You passed in: %s\n", infile);
            Usage();
            return 1;
        }
        else
        {
            int offset = pTemp - &outputFilename[0];
            outputFilename[offset+1] = 0;
            if (bDATonly)
            {
                strcat(&outputFilename[0], "dat");
            }
            else
            {
                strcat(&outputFilename[0], "binary");
            }
        }
    }
    else // use filename specified with -o
    {
        strcpy(outputFilename, outfile);
    }

    if (!bQuiet)
    {
        Banner();
        printf("Compiling %s...\n", infile);
    }

    s_pCompilerData = InitStruct();

    s_pCompilerData->list = new char[ListLimit];
    s_pCompilerData->list_limit = ListLimit;
    memset(s_pCompilerData->list, 0, ListLimit);
    s_pCompilerData->doc = new char[DocLimit];
    s_pCompilerData->doc_limit = DocLimit;
    memset(s_pCompilerData->doc, 0, DocLimit);

    s_pCompilerData->bDATonly = bDATonly;

    // copy filename into obj_title, and chop off the .spin
    strcpy(s_pCompilerData->obj_title, infile);
    char* pExtension = strstr(&s_pCompilerData->obj_title[0], ".spin");
    if (pExtension != 0)
    {
        *pExtension = 0;
    }

    if (!CompileRecursively(infile))
    {
        return 1;
    }

    if (!bQuiet)
    {
        printf("Done.\n");
    }

    unsigned char* pBuffer = NULL;
    int bufferSize = 0;
    ComposeRAM(&pBuffer, bufferSize, bDATonly);
    FILE* pFile = fopen(outputFilename, "wb");
    if (pFile)
    {
        fwrite(pBuffer, bufferSize, 1, pFile);
        fclose(pFile);
    }

    if (!bQuiet)
    {
        printf("Program size is %d bytes\n", ((unsigned short*)pBuffer)[7]);
    }

    delete [] pBuffer;

    if (bVerbose && !bQuiet && !bDATonly)
    {
        // do stuff with list and/or doc here
        int listOffset = 0;
        while (listOffset < s_pCompilerData->list_length)
        {
            char* pTemp = strstr(&(s_pCompilerData->list[listOffset]), "\r");
            if (pTemp)
            {
                *pTemp = 0;
            }
            printf("%s\n", &(s_pCompilerData->list[listOffset]));
            if (pTemp)
            {
                *pTemp = 0x0D;
                listOffset += (pTemp - &(s_pCompilerData->list[listOffset])) + 1;
            }
            else
            {
                listOffset += strlen(&(s_pCompilerData->list[listOffset]));
            }
        }
    }

    if (bDocMode && !bQuiet && !bDATonly)
    {
        // do stuff with list and/or doc here
        int docOffset = 0;
        while (docOffset < s_pCompilerData->doc_length)
        {
            char* pTemp = strstr(&(s_pCompilerData->doc[docOffset]), "\r");
            if (pTemp)
            {
                *pTemp = 0;
            }
            printf("%s\n", &(s_pCompilerData->doc[docOffset]));
            if (pTemp)
            {
                *pTemp = 0x0D;
                docOffset += (pTemp - &(s_pCompilerData->doc[docOffset])) + 1;
            }
            else
            {
                docOffset += strlen(&(s_pCompilerData->doc[docOffset]));
            }
        }
    }

    // cleanup
    delete [] s_pCompilerData->list;
    delete [] s_pCompilerData->doc;

    return 0;
}

void ComposeRAM(unsigned char** ppBuffer, int& bufferSize, bool bDATonly)
{
    if (!bDATonly)
    {
        unsigned short varsize = *((unsigned short*)&(s_pCompilerData->obj[0]));                      // variable size (in bytes)
        unsigned short codsize = *((unsigned short*)&(s_pCompilerData->obj[2]));                      // code size (in bytes)
        unsigned short pubaddr = *((unsigned short*)&(s_pCompilerData->obj[8]));                      // address of first public method
        unsigned short publocs = *((unsigned short*)&(s_pCompilerData->obj[10]));                     // number of stack variables (locals), in bytes, for the first public method
        unsigned short pbase = 0x0010;                                                                // base of object code
        unsigned short vbase = pbase + codsize;                                                       // variable base = object base + code size
        unsigned short dbase = vbase + varsize + 8;                                                   // data base = variable base + variable size + 8
        unsigned short pcurr = pbase + pubaddr;                                                       // Current program start = object base + public address (first public method)
        unsigned short dcurr = dbase + 4 + (s_pCompilerData->first_pub_parameters << 2) + publocs;    // current data stack pointer = data base + 4 + FirstParams*4 + publocs

        // reset ram
        *ppBuffer = new unsigned char[vbase];
        memset(*ppBuffer, 0, vbase);
        bufferSize = vbase;

        // set clock frequency and clock mode
        *((int*)&((*ppBuffer)[0])) = s_pCompilerData->clkfreq;
        (*ppBuffer)[4] = s_pCompilerData->clkmode;

        // set interpreter parameters
        ((unsigned short*)&((*ppBuffer)[4]))[1] = pbase;         // always 0x0010
        ((unsigned short*)&((*ppBuffer)[4]))[2] = vbase;
        ((unsigned short*)&((*ppBuffer)[4]))[3] = dbase;
        ((unsigned short*)&((*ppBuffer)[4]))[4] = pcurr;
        ((unsigned short*)&((*ppBuffer)[4]))[5] = dcurr;

        // set code
        memcpy(&((*ppBuffer)[pbase]), &(s_pCompilerData->obj[4]), codsize);

        // install ram checksum byte
        unsigned char sum = 0;
        for (int i = 0; i < vbase; i++)
        {
          sum = sum + (*ppBuffer)[i];
        }
        (*ppBuffer)[5] = (unsigned char)((-(sum+2028)) );
    }
    else
    {
        int size = (int)(*((unsigned short*)&(s_pCompilerData->obj[2]))) - 12;
        *ppBuffer = new unsigned char[size];
        bufferSize = size;
        memcpy(&((*ppBuffer)[0]), &(s_pCompilerData->obj[12]), size);
    }
}

// returns NULL if the file failed to open or is 0 length
char* LoadFile(char* pFilename, int* pnLength)
{
    char* pBuffer = 0;

    FILE* pFile = OpenFileInPath(pFilename, "rb");
    if (pFile != NULL)
    {
#ifdef USE_PREPROCESSOR
        if (s_bPreprocess)
        {
            pp_push_file_struct(&preproc, pFile, pFilename);
            pp_run(&preproc);
            pBuffer = pp_finish(&preproc);
            *pnLength = strlen(pBuffer);
        }
        else
#endif
        {
            // get the length of the file by seeking to the end and using ftell
            fseek(pFile, 0, SEEK_END);
            *pnLength = ftell(pFile);

            if (*pnLength > 0)
            {
                pBuffer = new char[*pnLength+1]; // allocate a buffer that is the size of the file plus one char
                pBuffer[*pnLength] = 0; // set the end of the buffer to 0 (null)

                // seek back to the beginning of the file and read it in
                fseek(pFile, 0, SEEK_SET);
                fread(pBuffer, 1, *pnLength, pFile);
            }
        }
        fclose(pFile);
    }

    return pBuffer;
}

bool AddObjectToHeap(char* name)
{
    if (s_nObjHeapIndex < MaxObjInHeap)
    {
        int nNameBufferLength = (int)strlen(name)+1;
        s_ObjHeap[s_nObjHeapIndex].ObjFilename = new char[nNameBufferLength];
        strcpy(s_ObjHeap[s_nObjHeapIndex].ObjFilename, name);
        s_ObjHeap[s_nObjHeapIndex].ObjSize = s_pCompilerData->obj_ptr;
        s_ObjHeap[s_nObjHeapIndex].Obj = new char[s_pCompilerData->obj_ptr];
        memcpy(s_ObjHeap[s_nObjHeapIndex].Obj, &(s_pCompilerData->obj[0]), s_pCompilerData->obj_ptr);
        s_nObjHeapIndex++;
        return true;
    }
    return false;
}

//{Returns index of object of Name in Object Heap.  Returns -1 if not found}
int IndexOfObjectInHeap(char* name)
{
    for (int i = s_nObjHeapIndex-1; i >= 0; i--)
    {
        if (_stricmp(s_ObjHeap[i].ObjFilename, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

void CleanObjectHeap()
{
    for (int i = 0; i < s_nObjHeapIndex; i++)
    {
        delete [] s_ObjHeap[i].ObjFilename;
        s_ObjHeap[i].ObjFilename = NULL;
        delete [] s_ObjHeap[i].Obj;
        s_ObjHeap[i].Obj = NULL;
        s_ObjHeap[i].ObjSize = 0;
    }
}

bool CompileSubObject(char* pFileName)
{
    // need to do locate file here
    return CompileRecursively(pFileName);
}

int GetData(unsigned char* pDest, char* pFileName, int nMaxSize)
{
    int nBytesRead = 0;
    FILE* pFile = OpenFileInPath(pFileName, "rb");

    if (pFile)
    {
        if (pDest)
        {
            nBytesRead = (int)fread(pDest, 1, nMaxSize, pFile);
        }
        fclose(pFile);
    }
    else
    {
        printf("Cannot find/open dat file: %s \n", pFileName);
        return -1;
    }

    return nBytesRead;
}

unsigned int DecodeUtf8(const char* pBuffer, int& nCharSize)
{
    unsigned int nChar = (unsigned int)((unsigned char)(*pBuffer));
    if (nChar < 128)
    {
        nCharSize = 1;
    }
    else if (nChar >= 192 && nChar <= 223)
    {
        // character is two bytes
        nCharSize = 2;
        nChar = ((nChar & 31) << 6) | ((unsigned int)pBuffer[1] & 63);
    }
    else if (nChar >= 224 && nChar <= 239)
    {
        // character is three bytes
        nCharSize = 3;
        nChar = ((nChar & 15) << 12) | (((unsigned int)pBuffer[1] & 63) << 6) | ((unsigned int)pBuffer[2] & 63);
    }
    else if (nChar >= 240 && nChar <= 247)
    {
        // character is four bytes
        nCharSize = 4;
        nChar = ((nChar & 7) << 18) | (((unsigned int)pBuffer[1] & 63) << 12) | (((unsigned int)pBuffer[2] & 63) << 6) | ((unsigned int)pBuffer[3] & 63);
    }
    else if (nChar >= 248 && nChar <= 251)
    {
        // character is five bytes
        nCharSize = 5;
        nChar = ((nChar & 3) << 24) | (((unsigned int)pBuffer[1] & 63) << 18) | (((unsigned int)pBuffer[2] & 63) << 12) | (((unsigned int)pBuffer[3] & 63) << 6) | ((unsigned int)pBuffer[4] & 63);
    }
    else if (nChar >= 252 && nChar <= 253)
    {
        // character is six bytes
        nCharSize = 6;
        nChar = ((nChar & 1) << 30) | (((unsigned int)pBuffer[1] & 63) << 24) | (((unsigned int)pBuffer[2] & 63) << 18) | (((unsigned int)pBuffer[3] & 63) << 12) | (((unsigned int)pBuffer[4] & 63) << 6) | ((unsigned int)pBuffer[5] & 63);
    }

    return nChar;
}

void UnicodeToPASCII(char* pBuffer, int nBufferLength, char* pPASCIIBuffer)
{
    // Translate Unicode source to PASCII (Parallax ASCII) source

    /*
     The Parallax font v1.0 utilizes the following Unicode addresses (grouped here into similar ranges):
         $0020- $007E, $00A1, $00A3, $00A5, $00B0- $00B3, $00B5, $00B9, $00BF- $00FE,
         $0394, $03A3, $03A9, $03C0,
         $2022, $2023, $20AC,
         $2190- $2193,
         $221A, $221E, $2248,
         $25B6, $2500, $2502, $250C, $2510, $2514, $2518, $251C, $2523, $2524, $252B, $252C, $2533, $2534, $253B, $253C, $254B, $25C0,
         $F000, $F001, $F008- $F00D, $F016- $F01F, $F07F- $F08F, $F0A0, $F0A2, $F0A6- $F0AF, $F0B4, $F0B6- $F0B8, $F0BA- $F0BE
     Propeller source files are sometimes Unicode-encoded (UTF-16) and can contain only the supported Parallax font characters with the following
     exceptions:  1) Run-time only characters ($F000, $F001, $F008-$F00D) are not valid
                  2) Some control code characters ($0009, $0010, $000D) are allowed.
     Any invalid characters will be translated to the PASCII infinity character ($00FF).
     All others will be mapped to their corresponding PASCII character.
     NOTE: The speediest translation would be a simple lookup table, but the natural size would be impractical.
     A little math solves this problem:
         ANDing the unicode character value with $07FF yields a range of $0000 to $07FF but causes the 20xx characters to inappropriately
         collide with other valid values.  ANDing with $07FF then ORing with ((CharVal >> 5) AND NOT(CharVal >> 4) AND $0100) corrects
         this by mapping 20xx into 21xx space and 22xx characters into 23xx space.  This allows for a practical translation table
         (of 2K bytes) to be used.
    */
    unsigned char aCharTxMap[0x800] = {
     // 0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07  0x08  0x09  0x0A  0x0B  0x0C  0x0D  0x0E  0x0F  0x10  0x11  0x12  0x13  0x14  0x15  0x16  0x17  0x18  0x19  0x1A  0x1B  0x1C  0x1D  0x1E  0x1F
/*000*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x09, 0x0A, 0xff, 0xff, 0x0D, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
/*020*/ 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
/*040*/ 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
/*060*/ 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
/*080*/ 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*0A0*/ 0xA0, 0xA1, 0xA2, 0xA3, 0xff, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
/*0C0*/ 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
/*0E0*/ 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xff,
/*100*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*120*/ 0xff, 0xff, 0x0F, 0x0E, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*140*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*160*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*180*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02, 0x04, 0x03, 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*1A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xA4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*1C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*1E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*200*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*220*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*240*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*260*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*280*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*2A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*2C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*2E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*300*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x15, 0xff, 0xff, 0xff, 0xFF, 0xff,
/*320*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*340*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x14, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*360*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*380*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*3A0*/ 0xff, 0xff, 0xff, 0x12, 0xff, 0xff, 0xff, 0xff, 0xff, 0x13, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*3C0*/ 0x11, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*3E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*400*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*420*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*440*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*460*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*480*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*4A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*4C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*4E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*500*/ 0x90, 0xff, 0x91, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9F, 0xff, 0xff, 0xff, 0x9E, 0xff, 0xff, 0xff, 0x9D, 0xff, 0xff, 0xff, 0x9C, 0xff, 0xff, 0xff, 0x95, 0xff, 0xff, 0xff,
/*520*/ 0xff, 0xff, 0xff, 0x99, 0x94, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x98, 0x97, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9B, 0x96, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9A, 0x92, 0xff, 0xff, 0xff,
/*540*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x93, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*560*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*580*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*5A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*5C0*/ 0x06, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*5E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*600*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*620*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*640*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*660*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*680*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*6A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*6C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*6E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*700*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*720*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*740*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*760*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*780*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*7A0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*7C0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
/*7E0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    bool bUnicode = false;
    bool bUtf8 = false;
#ifdef USE_PREPROCESSOR
    if (s_bPreprocess)
    {
        // preprocessor outputs utf8 encoded data, so just force it
        bUnicode = true;
        bUtf8 = true;
    }
    else
#endif
    // detect Unicode or ASCII form
    if (*((short*)(&pBuffer[0])) == -257) // -257 == 0xFEFF which is the UTF-16 BOM character for Little Endian
    {
        bUnicode = true;
    }
    else if (*((short*)(&pBuffer[0])) == -2) // -2 == 0xFFFE which is the UTF-16 BOM character for Big Endian
    {
        // we don't handle this format yet
        printf("Input in UTF-16 in big endian format is not supported yet!\n");
        exit(-1);
    }
    else if (nBufferLength > 2 && pBuffer[1] == 0) // this attempts tp handle the case of a UTF-16 encoded file without a BOM character. It assumes the first character is < 255, thus the second byte is 0
    {
        bUnicode = true;
    }
    else if (nBufferLength > 2 && (unsigned char)pBuffer[0] == 239 && (unsigned char)pBuffer[1] == 187 && (unsigned char)pBuffer[2] == 191) // handle the UTF-8 BOM sequence case
    {
        bUnicode = true;
        bUtf8 = true;
    }

    // probably need to add a way to detect utf-8 without a BOM character (need to scan file and see if it's all valid utf-8 form

    // the compiler code expects 0x0D line endings, this code will strip out 0x0A from the line endings and put in 0x0D if they are not there.
    if (bUnicode)
    {
        // unicode, copy over translating line endings
        int nSourceOffset = 0;
        int nDestOffset = 0;
        unsigned short nPrevChar = 0;
        while (nSourceOffset < nBufferLength)
        {
            int nCharSize = 2;
            unsigned short nChar =  (bUtf8 == true) ? (unsigned short)DecodeUtf8(&pBuffer[nSourceOffset], nCharSize) : *((unsigned short*)(&pBuffer[nSourceOffset]));
            if (nChar != 0x000A && nChar != 0xFEFF) // -257 == 0xFEFF
            {
                pPASCIIBuffer[nDestOffset] = aCharTxMap[(nChar | ((nChar >> 5) & ~(nChar >> 4) & 0x0100)) & 0x07FF];
                nDestOffset++;
            }
            else if (nChar == 0x000A)
            {
                if (nSourceOffset == 0 || nPrevChar != 0x000D)
                {
                    pPASCIIBuffer[nDestOffset] = 0x0D;
                    nDestOffset++;
                }
            }
            nSourceOffset += nCharSize;
            nPrevChar = nChar;
        }
        pPASCIIBuffer[nDestOffset] = 0;
    }
    else
    {
        // ascii, copy over translating line endings
        int nSourceOffset = 0;
        int nDestOffset = 0;
        while (nSourceOffset < nBufferLength)
        {
            char nChar = pBuffer[nSourceOffset];
            if (nChar != 0x0A)
            {
                pPASCIIBuffer[nDestOffset] = nChar;
                nDestOffset++;
            }
            else
            {
                if (nSourceOffset == 0 || pBuffer[nSourceOffset-1] != 0x0D)
                {
                    pPASCIIBuffer[nDestOffset] = 0x0D;
                    nDestOffset++;
                }
            }
            nSourceOffset++;
        }
        pPASCIIBuffer[nDestOffset] = 0;
    }
}

void GetPASCIISource(char* pFilename)
{
    // read in file to temp buffer
    // convert to PASCII and assign to s_pCompilerData->source
    int nLength = 0;
    char* pBuffer = LoadFile(pFilename, &nLength);
    if (pBuffer)
    {
        char* pPASCIIBuffer = new char[nLength+1];
        UnicodeToPASCII(pBuffer, nLength, pPASCIIBuffer);
        s_pCompilerData->source = pPASCIIBuffer;

        delete [] pBuffer;
    }
    else
    {
        s_pCompilerData->source = NULL;
    }
}

bool CompileRecursively(char* pFilename)
{
    s_nObjStackPtr++;
    if (s_nObjStackPtr > ObjFileStackLimit)
    {
        printf("%s : error : Object nesting exceeds limit of %d levels.\n", pFilename, ObjFileStackLimit);
        return false;
    }

#ifdef USE_PREPROCESSOR
    void *definestate = 0;
    if (s_bPreprocess)
    {
        definestate = pp_get_define_state(&preproc);
    }
#endif
    GetPASCIISource(pFilename);

    if (s_pCompilerData->source == NULL)
    {
        printf("%s : error : Can not find/open file.\n", pFilename);
        return false;
    }

    // first pass on object
    const char* pErrorString = Compile1();
    if (pErrorString != 0)
    {
        int lineNumber = 1;
        int column = 1;
        GetErrorInfo(lineNumber, column);
        printf("%s(%d:%d) : error : %s\n", pFilename, lineNumber, column, pErrorString);
        return false;
    }

    if (s_pCompilerData->obj_files > 0)
    {
        char filenames[file_limit*256];
        //int filename_start[file_limit];
        //int filename_finish[file_limit];
        //int instances[file_limit];

        int numObjects = s_pCompilerData->obj_files;
        for (int i = 0; i < numObjects; i++)
        {
            // copy the obj filename appending .spin if it doesn't have it.
            strcpy(&filenames[i<<8], &(s_pCompilerData->obj_filenames[i<<8]));
            if (strstr(&filenames[i<<8], ".spin") == NULL)
            {
                strcat(&filenames[i<<8], ".spin");
            }
            //filename_start[i] = s_pCompilerData->obj_name_start[i];
            //filename_finish[i] = s_pCompilerData->obj_name_finish[i];
            //instances[i] = s_pCompilerData->obj_instances[i];
        }

        for (int i = 0; i < numObjects; i++)
        {
            if (!CompileSubObject(&filenames[i<<8]))
            {
                return false;
            }
        }

        // redo first pass on object
#ifdef USE_PREPROCESSOR
        if (s_bPreprocess)
        {
            // undo any defines in sub-objects
            pp_restore_define_state(&preproc, definestate);
        }
#endif
        GetPASCIISource(pFilename);
        pErrorString = Compile1();
        if (pErrorString != 0)
        {
            int lineNumber = 1;
            int column = 1;
            GetErrorInfo(lineNumber, column);
            printf("%s(%d:%d) : error : %s\n", pFilename, lineNumber, column, pErrorString);
            return false;
        }

        //{Load sub-objects' obj data}
        int p = 0;
        for (int i = 0; i < s_pCompilerData->obj_files; i++)
        {
            int nObjIdx = IndexOfObjectInHeap(&filenames[i<<8]);
            if (p + s_ObjHeap[nObjIdx].ObjSize > data_limit)
            {
                printf("%s : error : Object files exceed 64k.\n", pFilename);
                return false;
            }
            memcpy(&s_pCompilerData->obj_data[p], s_ObjHeap[nObjIdx].Obj, s_ObjHeap[nObjIdx].ObjSize);
            s_pCompilerData->obj_offsets[i] = p;
            s_pCompilerData->obj_lengths[i] = s_ObjHeap[nObjIdx].ObjSize;
            p += s_ObjHeap[nObjIdx].ObjSize;
        }
    }

    //{Load all DAT files}
    if (s_pCompilerData->dat_files > 0)
    {
        int p = 0;
        for (int i = 0; i < s_pCompilerData->dat_files; i++)
        {
            // Get DAT's Files

            // Get name information
            char filename[256];
            strcpy(&filename[0], &(s_pCompilerData->dat_filenames[i<<8]));

            // Load file and add to dat_data buffer
            s_pCompilerData->dat_lengths[i] = GetData(&(s_pCompilerData->dat_data[p]), &filename[0], data_limit - p);
            if (s_pCompilerData->dat_lengths[i] == -1)
            {
                s_pCompilerData->dat_lengths[i] = 0;
                return false;
            }
            if (p + s_pCompilerData->dat_lengths[i] > data_limit)
            {
                printf("%s : error : Object files exceed 64k.\n", pFilename);
                return false;
            }
            s_pCompilerData->dat_offsets[i] = p;
            p += s_pCompilerData->dat_lengths[i];
        }
    }

    // second pass of object
    pErrorString = Compile2();
    if (pErrorString != 0)
    {
        int lineNumber = 1;
        int column = 1;
        GetErrorInfo(lineNumber, column);
        printf("%s(%d:%d) : error : %s\n", pFilename, lineNumber, column, pErrorString);
        return false;
    }

    // Check to make sure object fits into 32k
    int i = 0x10 + *((short*)&s_pCompilerData->obj[0]) + *((short*)&s_pCompilerData->obj[2]) + (s_pCompilerData->stack_requirement << 2);
    if (!s_pCompilerData->compile_mode && (i > 0x8000))
    {
        printf("%s : error : Object exceeds runtime memory limit by %d longs.\n", pFilename, (i - 0x8000) >> 2);
        return false;
    }

    if (!AddObjectToHeap(pFilename))
    {
        printf("%s : error : Object Heap Overflow.\n", pFilename);
        return false;
    }
    s_nObjStackPtr--;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
//                           TERMS OF USE: MIT License                                   //
///////////////////////////////////////////////////////////////////////////////////////////
// Permission is hereby granted, free of charge, to any person obtaining a copy of this  //
// software and associated documentation files (the "Software"), to deal in the Software //
// without restriction, including without limitation the rights to use, copy, modify,    //
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    //
// permit persons to whom the Software is furnished to do so, subject to the following   //
// conditions:                                                                           //
//                                                                                       //
// The above copyright notice and this permission notice shall be included in all copies //
// or substantial portions of the Software.                                              //
//                                                                                       //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   //
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         //
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    //
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION     //
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE        //
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                //
///////////////////////////////////////////////////////////////////////////////////////////
