// frg_color_table.cpp
// for frg_writer
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
#include "frg_color_table.h"
#include "../color_error_diffuse.h"
#include <queue>

namespace frg{

    const TUInt32 gray_r_coeff=19;//(0.299*(1<<6));
    const TUInt32 gray_g_coeff=37;//(0.587*(1<<6));
    const TUInt32 gray_b_coeff=(1<<6)-gray_r_coeff-gray_g_coeff;//(0.114*(1<<6));
    //颜色距离.
    static inline TUInt32 getColorDistance(const Color24& c0,const Color24& c1){
        return   (TUInt32)sqr(c0.r-c1.r)*gray_r_coeff
                +(TUInt32)sqr(c0.g-c1.g)*gray_g_coeff
                +(TUInt32)sqr(c0.b-c1.b)*gray_b_coeff;
    }
    template<class TCalcColor>
    static inline TUInt32 getColorDistance(const TCalcColor& c0,const TCalcColor& c1){
        return (TUInt32)( (sqr((TInt64)c0.r-c1.r)*gray_r_coeff
                        + sqr((TInt64)c0.g-c1.g)*gray_g_coeff
                        + sqr((TInt64)c0.b-c1.b)*gray_b_coeff) >>(TCalcColor::kIntFloatBit*2) );
    }



    typedef TColorTableZiper::TColorErrorParameter TColorErrorParameter;

    static const int kColorErrorDiffuseCoefficientIntFloatBit=8;

    TUInt32 TColorTableZiper::qualityToMatchColorMask(float colorQuality){
        //质量->有效bit数: 100->8.0  80->7.0   60->6.0  0->4.0
        float gBit_f;
        if (colorQuality>=80)
            gBit_f=7.0f+(colorQuality-80)*((8.0f-7.0f)/(100-80));
        else if (colorQuality>=60)
            gBit_f=6.0f+(colorQuality-60)*((7.0f-6.0f)/(80-60));
        else
            gBit_f=4.0f+(colorQuality-60)*((6.0f-4.0f)/(60-0));

        int gBit=(int)(gBit_f+0.5f);
        int rBit=(int)(8-((8-gBit_f)*1.5f)+0.5f);
        int bBit=(int)(8-((8-gBit_f)*2)+0.5f);
        if (gBit>8) gBit=8; else if (gBit<2) gBit=2;
        if (rBit>8) rBit=8; else if (rBit<2) rBit=2;
        if (bBit>8) bBit=8; else if (bBit<2) bBit=2;
        gBit=8-gBit; rBit=8-rBit; bBit=8-bBit;
        return (0xFF>>bBit<<bBit)|(0xFF>>gBit<<(gBit+8))|(0xFF>>rBit<<(rBit+16));
    }


    static void qualityToMinColorError(TColorErrorParameter* out_errorParameter,float& colorQuality,bool isMustFitColorTable){
        if (colorQuality>100) colorQuality=100;
        else if (colorQuality<0) colorQuality=0;

        const TInt32 kDiffuseCoeff0=(1<<kColorErrorDiffuseCoefficientIntFloatBit)*87/100;
        const TInt32 kDiffuseCoeff100=(1<<kColorErrorDiffuseCoefficientIntFloatBit)*97/100;
        out_errorParameter->errorDiffuse_coefficient=(TInt32)(0.5f+kDiffuseCoeff0+(kDiffuseCoeff100-kDiffuseCoeff0)*colorQuality/100);

        const TInt32 kMaxDiffuseValue0=(1<<kColorErrorDiffuseCoefficientIntFloatBit)*(1<<5);
        const TInt32 kMaxDiffuseValue100=(1<<kColorErrorDiffuseCoefficientIntFloatBit)*(1<<2);
        out_errorParameter->maxErrorDiffuseValue=(TInt32)(0.5f+kMaxDiffuseValue0+(kMaxDiffuseValue100-kMaxDiffuseValue0)*colorQuality/100);

        out_errorParameter->isMustFitColorTable=isMustFitColorTable;//colorQuality<=80;

        static TInt32 kSingleColorError0=getColorDistance(Color24(150,150,150),Color24(0,0,0));
        static TInt32 kSingleColorError60=getColorDistance(Color24((1<<5)-1,(1<<5)-1,(1<<6)-1),Color24(0,0,0));
        static TInt32 kSingleColorError80=getColorDistance(Color24((1<<3)-1,(1<<3)-1,(1<<3)-1),Color24(0,0,0));
        static TInt32 kSingleColorError100=0;

        static TInt32 kTableSize0=2;
        static TInt32 kTableSize60=12;
        static TInt32 kTableSize80=16;

        if ((80<=colorQuality)&&(colorQuality<=100)){
            out_errorParameter->minColorError=(TInt32)(0.5f+(colorQuality-80)*(kSingleColorError100-kSingleColorError80)/(100-80)+kSingleColorError80);
            out_errorParameter->minColorError_optimize=(out_errorParameter->minColorError)>>1;
            out_errorParameter->maxTableSize=16;
            return;
        }else if ((60<=colorQuality)&&(colorQuality<80)){
            out_errorParameter->minColorError=(TInt32)(0.5f+(colorQuality-60)*(kSingleColorError80-kSingleColorError60)/(80-60)+kSingleColorError60);
            out_errorParameter->minColorError_optimize=(out_errorParameter->minColorError)>>1;
            out_errorParameter->maxTableSize=(TInt32)(0.5f+(colorQuality-60)*(kTableSize80-kTableSize60)/(80-60)+kTableSize60);
            return;
        }else if ((0<=colorQuality)&&(colorQuality<60)){
            out_errorParameter->minColorError=(TInt32)(0.5f+(colorQuality-0)*(kSingleColorError60-kSingleColorError0)/(60-0)+kSingleColorError0);
            out_errorParameter->minColorError_optimize=(out_errorParameter->minColorError)>>1;
            out_errorParameter->maxTableSize=(TInt32)(0.5f+(colorQuality-0)*(kTableSize60-kTableSize0)/(60-0)+kTableSize0);
            return;
        }
        assert(false);
    }


    //////////////////

    void TColorTableZiper::TColorNode::uniteColor(const TColorNode& n){
        m_sumColor.add(n.m_sumColor);
        m_count+=n.m_count;
        TColor bestColor=m_sumColor; bestColor.divUseRound(m_count);
        if (getColorDistance(bestColor,n.m_color)<getColorDistance(bestColor,m_color))
            m_color=n.m_color;
    }

    TColorTableZiper::TColorTableZiper(float colorQuality,bool isMustFitColorTable):m_colorQuality(colorQuality){
        qualityToMinColorError(&m_errorParameter,m_colorQuality,isMustFitColorTable);
    }


    static void clearErrorColors(const TPixelsRefBase<TColorTableZiper::TErrorColor>& dst){
        const int w=dst.width;
        TColorTableZiper::TErrorColor* pline=dst.pColor;
        for (int y=0;y<dst.height;++y){
            for (int x=0;x<w;++x){
                pline[x].clear();
            }
            pline=dst.nextLine(pline);
        }
    }

    void TColorTableZiper::setImageSize(int imageWidth,int imageHeight){// null   ,boader,boader,boader,...
        m_errorBuffer.resizeFast(1+1+imageWidth+1,1+kFrg_ClipHeight);   // HErrCur,boader,..........,boader
        clearErrorColors(m_errorBuffer.getRef());                       // HErrCur,boader,..........,boader
    }                                                                   //...
    
    //int _temp_nums[64+1]={0};
    bool TColorTableZiper::getBestColorTable(std::vector<Color24>& out_table,const TPixels32Ref& colors,const int maxTableSize)const{
        std::vector<TColorNode> colorSet; //color<-->count
        getColorSet(&colorSet,colors);
        //++_temp_nums[colorSet.size()];
        
        if (m_errorParameter.minColorError==0){
            if (colorSet.size()<=(TUInt)maxTableSize){
                //ok
                sortColorArrayForOut(colorSet);
                writeTable(out_table,colorSet);
                return true;
            }else{
                out_table.clear();
                return false;
            }
        }

        //delete color
        deleteColor(colorSet,maxTableSize,m_errorParameter);
        if (colorSet.size()>(TUInt)maxTableSize)
            return false; //fail

        sortColorArrayForOut(colorSet);
        writeTable(out_table,colorSet);//ok
        return true;
    }

    /////////////////
    struct TColorDistanceInfo{
    private:
        friend class TColorsDistance;
        int iX;
        int iY;
    };
    
    class TColorsDistance{
    public:
        inline TColorsDistance(std::vector<TColorTableZiper::TColorNode>& colorSet)
            :m_colorSet(colorSet),m_colorCount((int)colorSet.size()),m_isNeedUpdateCache(false){ }
        inline ~TColorsDistance(){ resetSetSize(); }
        
        inline int getColorCount()const{ return m_colorCount; }
        TUInt32 getMinDistanceColor(TColorDistanceInfo* out_cdInfo){
            assert(m_colorCount>=2);
            if (m_distanceCaches.empty())
                initCache();
            else if (m_isNeedUpdateCache)
                updateCache();
            
            while (true) {
                const TDistanceCache& cur=m_distanceCaches.top();
                if ((m_colorSet[cur.iX].getCount()==0)||(m_colorSet[cur.iY].getCount()==0))
                    m_distanceCaches.pop();
                else{
                    out_cdInfo->iX=cur.iX;
                    out_cdInfo->iY=cur.iY;
                    return cur.distance;
                }
            }
        }
        void  deleteMinDistanceColor(const TColorDistanceInfo& cdInfo){
            //assert(cdInfo.iX<cdInfo.iY);
            //assert(m_distanceCaches.top().iX==cdInfo.iX);
            //assert(m_distanceCaches.top().iY==cdInfo.iY);
            int newIndex=(int)m_colorSet.size();
            //del color
            m_colorSet.push_back(m_colorSet[cdInfo.iX]);
            m_colorSet[newIndex].uniteColor(m_colorSet[cdInfo.iY]);
            m_colorSet[cdInfo.iX].setCount(0);
            m_colorSet[cdInfo.iY].setCount(0);
            --m_colorCount;
            
            if ((newIndex+1)>(m_colorCount)*2){
                //clear all chaches
                resetSetSize();
            }else{
                m_isNeedUpdateCache=true;
                m_distanceCaches.pop();
            }
        }
    private:
        struct TDistanceCache{
            short       iX;
            short       iY;
            TUInt32     distance;
            inline TDistanceCache(int _iX,int _iY,TUInt32 _distance):iX(_iX),iY(_iY),distance(_distance){}
            inline bool operator <(const TDistanceCache& y)const{
                return distance>y.distance;
            }
        };
        std::vector<TColorTableZiper::TColorNode>&      m_colorSet;
        int                                             m_colorCount;
        std::priority_queue<TDistanceCache>             m_distanceCaches;
        bool                                            m_isNeedUpdateCache;
        void resetSetSize(){
            m_distanceCaches=std::priority_queue<TDistanceCache>();
            int insertIndex=0;
            for (int i=0;i<(int)m_colorSet.size();++i){
                if (m_colorSet[i].getCount()==0) continue;
                m_colorSet[insertIndex]=m_colorSet[i]; ++insertIndex;
            }
            assert(insertIndex==m_colorCount);
            m_colorSet.resize(insertIndex);
        }
        void initCache(){
            m_distanceCaches=std::priority_queue<TDistanceCache>();
            for (int index0=0;index0<m_colorCount-1;++index0){
                for (int index1=index0+1;index1<m_colorCount;++index1){
                    TUInt32 distance=getNodeColorDistance(m_colorSet[index0],m_colorSet[index1]);
                    m_distanceCaches.push(TDistanceCache(index0,index1,distance));
                }
            }
        }
        void updateCache(){
            int index1=(int)m_colorSet.size()-1;
            for (int index0=0;index0<index1;++index0){
                if (m_colorSet[index0].getCount()==0) continue;
                TUInt32 distance=getNodeColorDistance(m_colorSet[index0],m_colorSet[index1]);
                m_distanceCaches.push(TDistanceCache(index0,index1,distance));
            }
            m_isNeedUpdateCache=false;
        }

        inline static TUInt32 getNodeColorDistance(const TColorTableZiper::TColorNode& na,const TColorTableZiper::TColorNode& nb){
            return getColorDistance(na.getColor(),nb.getColor())*std::min(na.getCount(),nb.getCount());
        }
    };
    ///

    void TColorTableZiper::deleteColor(std::vector<TColorNode>& colorSet,int maxTableSize,const TColorErrorParameter& errorParameter){
        maxTableSize=std::min(maxTableSize,errorParameter.maxTableSize);
        TColorsDistance colorsDistance(colorSet);
        TUInt32 curMinColorError=0;
        while (colorsDistance.getColorCount()>maxTableSize) {
            TColorDistanceInfo cdInfo;
            curMinColorError=colorsDistance.getMinDistanceColor(&cdInfo);
            if ((!errorParameter.isMustFitColorTable)&&(curMinColorError>errorParameter.minColorError))
                return; //fail;
            colorsDistance.deleteMinDistanceColor(cdInfo);
        }

        //succeed,continue optimize size

        const int kMinTableSize =2;
        while ((curMinColorError<errorParameter.minColorError_optimize)&&(colorsDistance.getColorCount()>kMinTableSize)){
            TColorDistanceInfo cdInfo;
            curMinColorError=colorsDistance.getMinDistanceColor(&cdInfo);
            if (curMinColorError>errorParameter.minColorError_optimize)
                return; //finish optimize;
            colorsDistance.deleteMinDistanceColor(cdInfo);
        }
    }

    template <class TCalcColor>
    static int getABestColorIndex(const Color24* table,TInt32 tableSize,const TCalcColor& color){
        TUInt32 curMinDistance=~0;
        int curMinIndex=-1;
        for (int index=0;index<tableSize;++index){
            TUInt32 distance=getColorDistance(TCalcColor(Color32(table[index].r,table[index].g,table[index].b)),color);
            if (distance<curMinDistance){
                curMinDistance=distance;
                curMinIndex=index;
            }
        }
        assert(curMinIndex>=0);
        return curMinIndex;
    }

    struct TIndextColors{
    public:
        typedef TColorTableZiper::TErrorColor TErrorColor;
        inline void toBeginLine() { }
        inline void toNextLine()  { }
        
        inline Color32 setColor(const TErrorColor& wantColor,Color32 srcColor) {
            TErrorColor newColor=wantColor; //newColor=f(wantColor,srcColor)
            newColor.sub(srcColor);
            newColor.r=limitError(newColor.r);
            newColor.g=limitError(newColor.g);
            newColor.b=limitError(newColor.b);
            newColor.add(srcColor);
            
            int index=getABestColorIndex(m_table,m_tableSize,newColor);
            m_out_indexList.push_back(index);
            Color24 rt= m_table[index];
            return Color32(rt.r,rt.g,rt.b);
        }

        TErrorColor  optimizeErrorColor(const TErrorColor& eColor) {
            TErrorColor rt;
            rt.r=optimizeEiffuseError(eColor.r);
            rt.g=optimizeEiffuseError(eColor.g);
            rt.b=optimizeEiffuseError(eColor.b);
            return rt;
        }

    public:
       inline TIndextColors(std::vector<TByte>& out_indexList,const Color24* table,int tableSize,
                            const TColorErrorParameter& errorParameter)
            :m_out_indexList(out_indexList),m_table(table),m_tableSize(tableSize),m_errorParameter(errorParameter){
        }
    private:
        std::vector<TByte>&     m_out_indexList;
        const Color24*          m_table;
        int                     m_tableSize;
        TColorErrorParameter    m_errorParameter;
        inline TInt32 optimizeEiffuseError(TInt32 c){
            c=c*m_errorParameter.errorDiffuse_coefficient/(1<<kColorErrorDiffuseCoefficientIntFloatBit);
            if (c<(-m_errorParameter.maxErrorDiffuseValue)){
                return (c-m_errorParameter.maxErrorDiffuseValue)/2;
            }else if (c>(m_errorParameter.maxErrorDiffuseValue)){
                return (c+m_errorParameter.maxErrorDiffuseValue)/2;
            }else{
                return c;
            }
        }
        inline TInt32 limitError(TInt32 c){
            const int limit=m_errorParameter.maxErrorDiffuseValue>>1;
            if (c<(-limit)){
                return -limit;
            }else if (c>limit){
                return limit;
            }else{
                return c;
            }
        }
    };


    void TColorTableZiper::getBestColorIndex(std::vector<TByte>& out_indexList,const Color24* table,int tableSize,const TPixels32Ref& subColors,int subX0,int subY0){
        out_indexList.clear();
        TIndextColors dstIndexs(out_indexList,table,tableSize,m_errorParameter);
        
        //error diffuse
        assert(subColors.width<=kFrg_ClipWidth);
        assert(subColors.height<=kFrg_ClipHeight);
        assert(subX0%kFrg_ClipWidth==0);
        assert(subY0%kFrg_ClipHeight==0);
        if (subX0==0){
            TPixelsRefBase<TErrorColor> ref=m_errorBuffer.getRef();
            for (int x=1;x<ref.width;++x)
                ref.pixels(x,0)=ref.pixels(x, ref.height-1);
            ref.pColor=ref.nextLine(ref.pColor);
            --ref.height;
            clearErrorColors(ref);
        }
        
        TPixelsRefBase<TErrorColor> errRef=m_errorBuffer.getRef();
        TBGRA32* pSrc=subColors.pColor;
        TErrorColor*  PrevHLineErr =errRef.pColor;
        TErrorColor*  NextHLineErr =errRef.nextLine(PrevHLineErr);
        dstIndexs.toBeginLine();
        for  (int y=0; y<subColors.height; ++y){
            TErrorDiffuse<TPixelsRefBase<TIndextColors::TErrorColor>,TIndextColors,TPixels32Ref>::errorDiffuse_Line(
                dstIndexs,pSrc,subColors.width,PrevHLineErr+1+1+subX0,NextHLineErr+1+1,&NextHLineErr[0]);
            PrevHLineErr=NextHLineErr; NextHLineErr=errRef.nextLine(PrevHLineErr);
            pSrc= subColors.nextLine(pSrc);
            dstIndexs.toNextLine();
        }

        /* not uses error diffuse; for test
        out_indexList.resize(subColors.width*subColors.height);
        const Color32* pline=subColors.pColor;
        TByte* out_index=&out_indexList[0];
        for (int y=0;y<subColors.height;++y){
            for (int x=0;x<subColors.width;++x){
                out_index[x]=getABestColorIndex(table,tableSize,TErrorColor(pline[x]));
            }
            out_index+=subColors.width;
            pline=subColors.nextLine(pline);
        }
        //*/
    }

    ////////

    void TColorTableZiper::getColorSet(std::vector<TColorNode>* out_colorSet,const TPixels32Ref& colors){
        TFRG_map<TUInt32,TUInt32> colorSet;
        const Color32* pline=colors.pColor;
        for (int y=0;y<colors.height;++y){
            for (int x=0; x<colors.width; ++x) {
                TUInt32 RGB=pline[x].getBGR();
                if (pline[x].a!=0)
                    ++colorSet[RGB];
            }
            pline=colors.nextLine(pline);
        }
        pline=colors.pColor;
        for (int y=0;y<colors.height;++y){
            for (int x=0; x<colors.width; ++x) {
                TUInt32 RGB=pline[x].getBGR();
                if (colorSet.find(RGB)==colorSet.end()) {
                    colorSet[RGB]=1;
                }
            }
            pline=colors.nextLine(pline);
        }

        TUInt colorCountsSize=colorSet.size();
        out_colorSet->resize(colorCountsSize);
        TFRG_map<TUInt32,TUInt32>::const_iterator it(colorSet.begin());
        for (TUInt i=0;i<colorCountsSize;++i,++it){
            Color24 color; color.setBGR(it->first);
            (*out_colorSet)[i].setColor(color,it->second);
        }
    }
    
    inline static bool color_sort_by(const TColorTableZiper::TColorNode& na,const TColorTableZiper::TColorNode& nb){
        return (na.getColor().b+na.getColor().g+na.getColor().r)<(nb.getColor().b+nb.getColor().g+nb.getColor().r);
    }
    void TColorTableZiper::sortColorArrayForOut(std::vector<TColorNode>& colorSet){
        std::sort(colorSet.begin(),colorSet.end(),color_sort_by);
    }

    void TColorTableZiper::writeTable(std::vector<Color24>& out_table,std::vector<TColorNode>& colorSet){
        TUInt colorSetSize=colorSet.size();
        out_table.resize(colorSetSize);
        for (TUInt i=0;i<colorSetSize; ++i) {
            out_table[i]=colorSet[i].asColor24();
        }
    }

}//end namespace frg
