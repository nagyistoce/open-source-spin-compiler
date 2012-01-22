//////////////////////////////////////////////////////////////
//                                                          //
// Propeller Spin/PASM Compiler                             //
// (c)2012 Parallax Inc. DBA Parallax Semiconductor.        //
// Adapted from Chip Gracey's x86 asm code by Roy Eltham    //
// See end of file for terms of use.                        //
//                                                          //
//////////////////////////////////////////////////////////////
//
// DistillObjects.cpp
//
// called Object Distiller in the asm code
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Utilities.h"
#include "PropellerCompilerInternal.h"
#include "SymbolEngine.h"
#include "Elementizer.h"
#include "ErrorStrings.h"

bool DistillSetup_Enter(short value)
{
    if (g_pCompilerData->dis_ptr == distiller_limit)
    {
        g_pCompilerData->error = true;
        g_pCompilerData->error_msg = g_pErrorStrings[error_odo];
        return false;
    }
    g_pCompilerData->dis[g_pCompilerData->dis_ptr++] = value;
    return true;
}

bool DistillSetup_Record(short id, short offset, short& subObjectId)
{
    if (!DistillSetup_Enter(id))
    {
        return false;
    }
    if (!DistillSetup_Enter(offset))
    {
        return false;
    }
    short numSubObjects = (short)(g_pCompilerData->obj[offset+3]);
    if (!DistillSetup_Enter(numSubObjects))
    {
        return false;
    }
    if (numSubObjects > 0)
    {
        short startingSubObjectId = subObjectId;
        for (short i = 0; i < numSubObjects; i++)
        {
            if (!DistillSetup_Enter(subObjectId++))
            {
                return false;
            }
        }

        short nextSubObjects = (short)(g_pCompilerData->obj[offset+2]);
        for (short i = 0; i < numSubObjects; i++)
        {
            short offsetAdjust = *((short*)&(g_pCompilerData->obj[offset + ((nextSubObjects + i) * 4)]));
            if (!DistillSetup_Record(startingSubObjectId + i, offset + offsetAdjust, subObjectId))
            {
                return false;
            }
        }
    }

    return true;
}

bool DistillSetup()
{
    g_pCompilerData->dis_ptr = 0;
    short subObjectId = 1;
    if (!DistillSetup_Record(0, 0, subObjectId))
    {
        return false;
    }

    short disPtr = 0;
    while (disPtr < g_pCompilerData->dis_ptr)
    {
        // do we have sub objects?
        short numSubObjects = g_pCompilerData->dis[disPtr + 2];
        if (numSubObjects > 0) 
        {
            short offset = g_pCompilerData->dis[disPtr + 1];

            disPtr += numSubObjects; 

            short offsetAdjust = (short)(g_pCompilerData->obj[offset + 2]);
            unsigned char* pObj = &(g_pCompilerData->obj[offset + (offsetAdjust * 4)]);
            for (int i = 0; i < numSubObjects; i++)
            {
                *((short*)&pObj[0]) = 0;
                pObj += 4;
            }
        }

        disPtr += 3;
    }

    return true;
}

void DistillEliminate_Update(short objectId, int newDisPtr)
{
    int disPtr = 0;

    while (disPtr < g_pCompilerData->dis_ptr)
    {
        disPtr += 3;
        short numSubObjects = g_pCompilerData->dis[disPtr - 1];
        if (numSubObjects > 0)
        {
            for (int i = 0; i < numSubObjects; i++)
            {
                if ((g_pCompilerData->dis[disPtr] & 0x7FFF) == objectId)
                {
                    g_pCompilerData->dis[disPtr] = (g_pCompilerData->dis[newDisPtr] | 0x8000);
                }
                disPtr++;
            }
        }
    }
}

void DistillEliminate()
{
    int disPtr = 0;

    while (disPtr < g_pCompilerData->dis_ptr)
    {
        short numSubObjects = g_pCompilerData->dis[disPtr + 2];
        if (numSubObjects > 0)
        {
            int i;
            for (i = 0; i < numSubObjects; i++)
            {
                if ((g_pCompilerData->dis[disPtr + 3 + i] & 0x8000) == 0)
                {
                    break;
                }
            }
            if (i < numSubObjects)
            {
                // point to next object record
                disPtr += (3 + numSubObjects);
                continue;
            }
        }

        // search for any matching objects
        int newDisPtr = disPtr;
        // point to next object record
        newDisPtr += (3 + numSubObjects);

        bool bRestart = false;
        while (newDisPtr < g_pCompilerData->dis_ptr)
        {
            short newNumSubObjects = g_pCompilerData->dis[newDisPtr + 2];

            if (numSubObjects != newNumSubObjects)
            {
                // point to next object record
                newDisPtr += (3 + newNumSubObjects);
                continue;
            }
            if (newNumSubObjects > 0)
            {
                int i;
                for (i = 0; i < newNumSubObjects; i++)
                {
                    if (g_pCompilerData->dis[disPtr + 3 + i] != g_pCompilerData->dis[newDisPtr + 3 + i])
                    {
                        break;
                    }
                }
                if (i < newNumSubObjects)
                {
                    // point to next object record
                    newDisPtr += (3 + newNumSubObjects);
                    continue;
                }
            }
            // compare the object binaries
            unsigned char* pObj = &(g_pCompilerData->obj[g_pCompilerData->dis[disPtr+1]]);
            short objLength = *((short*)pObj);
            if (memcmp(pObj, &(g_pCompilerData->obj[g_pCompilerData->dis[newDisPtr+1]]), (size_t)objLength) != 0)
            {
                // point to next object record
                newDisPtr += (3 + newNumSubObjects);
                continue;
            }

            // the objects match, so update all related sub-object id's
            DistillEliminate_Update(g_pCompilerData->dis[disPtr], newDisPtr);
            DistillEliminate_Update(g_pCompilerData->dis[newDisPtr], newDisPtr);

            // remove redundant object record from list
            g_pCompilerData->dis_ptr -= (3 + numSubObjects);
            memmove(&g_pCompilerData->dis[disPtr], &g_pCompilerData->dis[disPtr + (3 + numSubObjects)], (g_pCompilerData->dis_ptr - disPtr) * 2);

            // restart elimination from beginning
            bRestart = true;
        }
        if (bRestart)
        {
            disPtr = 0;
        }
        else
        {
            // point to next object record
            disPtr += (3 + numSubObjects);
            continue;
        }
    }
}

static unsigned char s_rebuildBuffer[obj_limit];
void DistillRebuild()
{
    int disPtr = 0;
    short rebuildPtr = 0;
    while (disPtr < g_pCompilerData->dis_ptr)
    {
        // copy the object from obj into the rebuild buffer
        unsigned char* pObj = &(g_pCompilerData->obj[g_pCompilerData->dis[disPtr + 1]]);
        short objLength = *((short*)pObj);
        memcpy(&(s_rebuildBuffer[rebuildPtr]), pObj, (size_t)objLength);

        // fixup the distiller record
        g_pCompilerData->dis[disPtr+1] = rebuildPtr;

        rebuildPtr += objLength;

        // point to the next object record
        disPtr += (3 + g_pCompilerData->dis[disPtr + 2]);
    }

    // copy the rebuilt data back into obj
    g_pCompilerData->obj_ptr = rebuildPtr;
    memcpy(&g_pCompilerData->obj[0], &s_rebuildBuffer[0], (size_t)rebuildPtr);
}

void DistillReconnect(int disPtr = 0)
{
    short numSubObjects = g_pCompilerData->dis[disPtr + 2];
    if (numSubObjects > 0)
    {
        // this objects offset in the obj
        short objectOffset = g_pCompilerData->dis[disPtr + 1]; 
        // the offset (number of longs) to the sub-object offset list within this obj
        unsigned char subObjectOffsetListPtr = g_pCompilerData->obj[objectOffset + 2];
        // pointer to the sub-object offset list for this obj
        short* pSubObjectOffsetList = (short*)&(g_pCompilerData->obj[objectOffset + (subObjectOffsetListPtr * 4)]);

        for (int i = 0; i < numSubObjects; i++)
        {
            short subObjectId = g_pCompilerData->dis[disPtr + 3 + i] & 0x7FFF;

            // find offset of sub-object
            int scanDisPtr = 0;
            while(1)
            {
                if (g_pCompilerData->dis[scanDisPtr] == subObjectId)
                {
                    break;
                }
                scanDisPtr += (3 + g_pCompilerData->dis[scanDisPtr + 2]);
            }

            // enter relative offset of sub-object
            pSubObjectOffsetList[i*2] = g_pCompilerData->dis[scanDisPtr + 1] - objectOffset;

            // call recursively to reconnect and sub-objects' sub-objects
            DistillReconnect(scanDisPtr);
        }
    }
}

bool DistillObjects()
{
    int saved_obj_ptr = g_pCompilerData->obj_ptr;

    if (!DistillSetup())
    {
        return false;
    }
    DistillEliminate();
    DistillRebuild();
    DistillReconnect();

    g_pCompilerData->distilled_longs = (saved_obj_ptr - g_pCompilerData->obj_ptr) >> 2;

    char tempStr[64];
    sprintf(tempStr, "\rDistilled longs: %d", g_pCompilerData->distilled_longs);
    if (!PrintString(tempStr))
    {
        return false;
    }
    return PrintChr(13);
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
