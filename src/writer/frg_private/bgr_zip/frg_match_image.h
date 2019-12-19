// frg_match_image.h
// for frg_writer
// 2维图像的帧内匹配.
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
#ifndef __LIBFRG_FRG_MATCH_IMAGE_H_
#define __LIBFRG_FRG_MATCH_IMAGE_H_
#include "frg_color_base.h"

namespace frg{

    class TColorMatch{
    public:
        explicit TColorMatch(const TPixels32Ref& ref,TUInt32 colorMask)
          :m_ref(ref),m_colorMask(0xFFFFFF),m_nodeWidth(0){ initColorMatch(); }
        bool findNodeMatch(int nodeX,int nodeY,int* out_x0,int* out_y0,frg_TMatchType* out_matchType) const;
        bool isMatchAt(int subX0,int subY0,int subWidth,int subHeight,int match_x0,int match_y0,frg_TMatchType* out_matchType)const;
    public:
        typedef TFRG_multimap<TUInt32,TUInt32> TMatchMap;
    private:
        TPixels32Ref            m_ref;
        TUInt32                 m_colorMask;
        TMatchMap               m_matchMap;
        int                     m_nodeWidth;
        std::vector<TUInt32>    m_nodeKeys;
        void initColorMatch();
    };

}//end namespace frg

#endif //__LIBFRG_FRG_MATCH_IMAGE_H_