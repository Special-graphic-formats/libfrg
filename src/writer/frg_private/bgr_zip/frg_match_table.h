// frg_match_table.h
//  for frg_write
// 局部调色板匹配.
/*
 This is the frg copyright.
 
 Copyright (c) 2012-2013 HouSisong All Rights Reserved.
 (The MIT License)
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef __LIBFRG_FRG_MATCH_TABLE_H_
#define __LIBFRG_FRG_MATCH_TABLE_H_
#include "frg_color_base.h"

namespace frg{

    class TTableMatch{
    public:
        explicit TTableMatch(const std::vector<Color24>&  colorTable,TUInt32 colorMask)
            :m_colorTable(colorTable),m_subColorSets((1<<24)/8,0),m_oldColorTableSize(0),m_colorMask(colorMask){ }
        
        int findMatch(const std::vector<Color24>& subTable,int* out_matchTableBit); //return -1 is not find
    private:
        const std::vector<Color24>&  m_colorTable;
        std::vector<TByte>  m_subColorSets;
        std::vector<TUInt>  m_fastMatch4bitCache;
        TInt                m_oldColorTableSize;
        TUInt32             m_colorMask;
        int _findMatch(const std::vector<Color24>& subTable,int windowTableSize);
    };

}//end namespace frg

#endif //__LIBFRG_FRG_MATCH_TABLE_H_