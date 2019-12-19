// frg_color_zip.cpp
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
#include "frg_color_zip.h"
#include "../frg_color_tools.h"
#include "frg_color_table.h"
#include "frg_match_image.h"
//#include "frg_match_index.h"
#include "frg_match_table.h"

namespace frg{

    struct TClipNode{
        TPixels32Ref    colors;         //本块像素.
        int             subX0;          //本块在源图片中的位置x
        int             subY0;          //本块在源图片中的位置y
        std::vector<Color24> sub_table; //本块的局部调色板.
        frg_TClipType   type;           //本块当前判定的类型.
        frg_TMatchType  matchType;      //帧内匹配类型.
        TByte           matchTableBit;  //匹配的调色板的bit数.
        TByte           directColorTypeData; //无损压缩的类型数据.
        TInt            matchIndex;     //帧内匹配的位置数据的索引.
        TUInt           forwardLength;  //调色板向前匹配位置.

        inline bool isSingleColorType()const{//单色.
            return (type==kFrg_ClipType_single_bgr)||(type==kFrg_ClipType_single_bgra_w8);
        }
        inline bool isDirectColorType()const{//无损压缩.
            return (type==kFrg_ClipType_directColor);
        }
        inline bool isMatchTableType()const{//颜色表匹配.
            return (type==kFrg_ClipType_match_table)||(type==kFrg_ClipType_match_table_single_a_w8);
        }
        inline bool isIndexType()const{//使用序号和调色板.
            return (type==kFrg_ClipType_index)||(type==kFrg_ClipType_index_single_a_w8);
        }

        TByte packNodeInfo()const{
            switch (type) {
                case kFrg_ClipType_index:
                case kFrg_ClipType_index_single_a_w8:
                    return (type<<4)|((TByte)(sub_table.size()-1));
                    break;
                case kFrg_ClipType_single_bgr:
                case kFrg_ClipType_single_bgra_w8:
                    return (type<<4)|(TByte)forwardLength;
                    break;
                case kFrg_ClipType_directColor:
                    return (type<<4)| directColorTypeData;
                    break;
                case kFrg_ClipType_match_table_single_a_w8:
                case kFrg_ClipType_match_table:
                    return (type<<4)|(matchTableBit-1);
                    break;
                case kFrg_ClipType_match_image:
                    return (type<<4)|(matchType);
                    break;
            }
            assert(false);
            return 0;
        }
    };

    class TColorZip{
    public:
        explicit TColorZip(float colorQuality,bool isMustFitColorTable,TUInt* out_bgrColorTableLength)
            :m_colorQuality(colorQuality),m_isMustFitColorTable(isMustFitColorTable),
            m_out_bgrColorTableLength(out_bgrColorTableLength) { }
        
        void saveTo(std::vector<TByte>& out_buf,const TPixels32Ref& ref){
            if (ref.getIsEmpty()) return;
            m_srcRef=ref;
            m_nodeWidth=(m_srcRef.width+kFrg_ClipWidth-1)/kFrg_ClipWidth;
            m_nodeHeight=(m_srcRef.height+kFrg_ClipHeight-1)/kFrg_ClipHeight;

            createClipNodes();
            TColorTableZiper tableZiper(m_colorQuality,m_isMustFitColorTable);
            createSubColorTable(tableZiper);
            
            TUInt32 matchColorMask=TColorTableZiper::qualityToMatchColorMask(m_colorQuality);
            createMatchs(matchColorMask);
            createPublicColorTable(matchColorMask);
            tableZiper.setImageSize(m_srcRef.width,m_srcRef.height);
            createIndexList(tableZiper);
            writeOut(out_buf);
        }

        void writeOut(std::vector<TByte>& out_buf){
            const TUInt32 nodeCount=SafeToUInt32(m_clipNodeList.size(),"TColorZip::writeOut() m_clipNodeList.size() over 32bit.");
            packUInt(out_buf,nodeCount);
            const TUInt32 tableSize=SafeToUInt32(m_colorTable.size(),"TColorZip::writeOut() m_colorTable.size() over 32bit.");
            packUInt(out_buf,tableSize);
            const TUInt32 index2Size=SafeToUInt32(m_indexList.size(),"TColorZip::writeOut() m_indexList.size() over 32bit.");
            packUInt(out_buf,index2Size);
            const TUInt32 matchCount=SafeToUInt32(m_matchXYList.size(),"TColorZip::writeOut() m_matchXYList.size() over 32bit.");
            packUInt(out_buf,matchCount);

            for (TUInt32 i=0;i<nodeCount;++i){
                const TClipNode& node=m_clipNodeList[i];
                out_buf.push_back(node.packNodeInfo());
            }

            *m_out_bgrColorTableLength=tableSize;
            for (TUInt32 i=0;i<tableSize;++i) {
                out_buf.push_back(m_colorTable[i].b);
                out_buf.push_back(m_colorTable[i].g);
                out_buf.push_back(m_colorTable[i].r);
            }

            out_buf.insert(out_buf.end(), m_indexList.begin(),m_indexList.end());

            for (TUInt32 i=0; i<matchCount; ++i) {
                writeUInt32(out_buf,m_matchXYList[i]);
            }
        }

        void createClipNodes(){
            //分割块.
            const TUInt nodeCount=(TUInt)m_nodeWidth*(TUInt)m_nodeHeight;
            m_clipNodeList.resize(nodeCount);
            TClipNode* cur_node=&m_clipNodeList[0];
            for (int ny=0; ny<m_nodeHeight;++ny) {
                for (int nx=0; nx<m_nodeWidth;++nx,++cur_node) {
                    cur_node->subX0=nx*kFrg_ClipWidth;
                    cur_node->subY0=ny*kFrg_ClipHeight;
                    cur_node->colors.pColor=&m_srcRef.pixels(cur_node->subX0,cur_node->subY0);
                    cur_node->colors.byte_width=m_srcRef.byte_width;
                    cur_node->colors.width=kFrg_ClipWidth;
                    if (cur_node->subX0+kFrg_ClipWidth>m_srcRef.width)
                        cur_node->colors.width=m_srcRef.width-cur_node->subX0;
                    cur_node->colors.height=kFrg_ClipHeight;
                    if (cur_node->subY0+kFrg_ClipHeight>m_srcRef.height)
                        cur_node->colors.height=m_srcRef.height-cur_node->subY0;
                }
            }
        }

        void createSubColorTable(const TColorTableZiper& tableZiper){
            //计算出局部调色板.
            TClipNode* cur_node=&m_clipNodeList[0];
            for (TUInt i=0; i<m_clipNodeList.size();++i,++cur_node){
                tableZiper.getBestColorTable(cur_node->sub_table,cur_node->colors,kFrg_MaxSubTableSize);
                bool isSingleColor=(cur_node->sub_table.size()==1);
                TByte _tmpAlpha;
                bool isW8AndSangleAlpha=(cur_node->colors.width==kFrg_ClipWidth)
                                        &&getIsSigleAlphaColor(cur_node->colors,&_tmpAlpha);
                if (isSingleColor){
                    if (isW8AndSangleAlpha)
                        cur_node->type=kFrg_ClipType_single_bgra_w8;
                    else
                        cur_node->type=kFrg_ClipType_single_bgr;
                } else if (cur_node->sub_table.empty()){//表示获取调色板失败,没有其他办法时需要无损压缩.
                    cur_node->type=kFrg_ClipType_directColor;
                    if (isW8AndSangleAlpha)
                        cur_node->directColorTypeData=1;
                    else
                        cur_node->directColorTypeData=0;
                }else{
                    if (isW8AndSangleAlpha)
                        cur_node->type=kFrg_ClipType_index_single_a_w8;
                    else
                        cur_node->type=kFrg_ClipType_index;
                }
            }
        }

        void createMatchs(TUInt32 matchColorMask){
            TColorMatch colorMatcher(m_srcRef,matchColorMask);
            
            //处理单色和帧内匹配.
            m_matchXYList.clear();
            TClipNode* cur_node=&m_clipNodeList[0];
            for (int ny=0; ny<m_nodeHeight;++ny) {
                for (int nx=0; nx<m_nodeWidth;++nx,++cur_node) {
                    if (cur_node->isSingleColorType())
                        continue;
                    //查找匹配位置.
                    frg_TMatchType matchType;
                    int  matchX0,matchY0;
                    if (findMatchImage(colorMatcher,nx,ny,&matchX0,&matchY0,&matchType)){
                        //match ok
                        cur_node->type=kFrg_ClipType_match_image;
                        cur_node->sub_table.clear();
                        cur_node->matchType=matchType;
                        cur_node->matchIndex=addAMatch(matchX0, matchY0);
                    }
                }
            }
        }

        void createPublicColorTable(TUInt32 matchColorMask){
            m_colorTable.clear();
            TTableMatch tableMatcher(m_colorTable,matchColorMask);
            
            //创建公共调色板，并处理颜色表共享匹配.
            TClipNode* cur_node=&m_clipNodeList[0];
            for (TUInt ni=0; ni<m_clipNodeList.size();++ni,++cur_node) {
               std::vector<Color24>& sub_table=cur_node->sub_table;
                if (cur_node->type==kFrg_ClipType_match_image)
                    continue; //next node;

                if (cur_node->isDirectColorType()){//无损.
                    addNodeColorsToTable(cur_node->colors);
                    continue; //next node;
                }

                bool isSingleColor=cur_node->isSingleColorType();

                TUInt forwardLength=0;
                int matchTableBit;
                TInt tableMatchIndex=tableMatcher.findMatch(sub_table,&matchTableBit); //颜色表匹配.
                if (tableMatchIndex>=0){
                    forwardLength=(TUInt)(m_colorTable.size()-tableMatchIndex);
                    //note!  mybe forwardLength < sub_table.size()
                    if (isSingleColor){
                        if (forwardLength>(TUInt)kFrg_MaxShortForwardLength) //singleColor时只支持短匹配.
                            tableMatchIndex=-1;
                    }
                    if (((TUInt32)forwardLength)!=forwardLength){ //限制长度.
                        tableMatchIndex=-1;
                    }
                }

                /* //check findMatch result, must saved bytes !
                if (tableMatchIndex>=0){
                    int curBestSavedBytes=0;
                    const int indexBitOld=kFrg_SubTableSize_to_indexBit[sub_table.size()];
                    if (isSingleColor)
                        curBestSavedBytes=1*sizeof(Color24);
                    else
                        curBestSavedBytes=(int)sub_table.size()*sizeof(Color24)-((cur_node->colors.width*cur_node->colors.height*(matchTableBit-indexBitOld)+7)>>3);
                    if (forwardLength>kFrg_MaxShortForwardLength)
                        curBestSavedBytes-=packUIntOutSize((TUInt32)forwardLength);
                    assert(curBestSavedBytes>0);
                    if (curBestSavedBytes<=0){
                        tableMatchIndex=-1;
                    }
                }
                //*/

                if (tableMatchIndex>=0){
                     cur_node->matchTableBit=matchTableBit;
                     if (!isSingleColor){
                         TByte _tmpAlpha;
                         if ((cur_node->colors.width==kFrg_ClipWidth)&&getIsSigleAlphaColor(cur_node->colors,&_tmpAlpha))
                             cur_node->type=kFrg_ClipType_match_table_single_a_w8;
                         else
                             cur_node->type=kFrg_ClipType_match_table;
                         cur_node->forwardLength=forwardLength;
                         cur_node->sub_table.clear();
                     }else{
                         assert(sub_table.size()==1);
                         cur_node->forwardLength=forwardLength;
                     }
                }else{
                    cur_node->forwardLength=0;
                    m_colorTable.insert(m_colorTable.end(), sub_table.begin(),sub_table.end());
                }
            }
        }

        void createIndexList(TColorTableZiper& tableZiper){
            m_indexList.clear();

            //get indexList
            std::vector<TByte> sub_indexList; sub_indexList.reserve(kFrg_ClipWidth*kFrg_ClipHeight);
            const Color24* cur_table=&m_colorTable[0];
            const Color24* table_end=cur_table+m_colorTable.size();
            for (TUInt i=0;i<m_clipNodeList.size();++i){
                TClipNode& cur_node=m_clipNodeList[i];
                switch (cur_node.type) {
                    case kFrg_ClipType_index:
                    case kFrg_ClipType_index_single_a_w8:
                    case kFrg_ClipType_match_table:
                    case kFrg_ClipType_match_table_single_a_w8:{
                        const Color24* subTable;
                        const bool isMatchTable=cur_node.isMatchTableType();
                        if (isMatchTable){
                            subTable=cur_table-cur_node.forwardLength;
                        }else{ //index
                            subTable=cur_table;
                            cur_table+=cur_node.sub_table.size();
                        }

                        sub_indexList.clear();
                        int bit;
                        if (isMatchTable)
                            bit=cur_node.matchTableBit;
                        else
                            bit=kFrg_SubTableSize_to_indexBit[cur_node.sub_table.size()];
                        int subTableSafeSize=(1<<bit);
                        if (subTableSafeSize>(table_end-subTable))
                            subTableSafeSize=(int)(table_end-subTable);
                        tableZiper.getBestColorIndex(sub_indexList, subTable,subTableSafeSize,cur_node.colors,cur_node.subX0,cur_node.subY0);
                        packIndexs(sub_indexList,bit);

                        std::vector<TByte> ctrlData;
                        if (isMatchTable){//match table
                            TUInt tableForwardLength=cur_node.forwardLength;
                            cur_node.forwardLength=0;
                            packUInt(ctrlData,(TUInt32)tableForwardLength);
                        }

                        m_indexList.insert(m_indexList.end(), ctrlData.begin(),ctrlData.end());
                        m_indexList.insert(m_indexList.end(), sub_indexList.begin(),sub_indexList.end());

                    } break;
                    case kFrg_ClipType_directColor:{
                        cur_table+=cur_node.colors.width*cur_node.colors.height;
                    } break;
                    case kFrg_ClipType_single_bgra_w8:
                    case kFrg_ClipType_single_bgr:{
                        if (cur_node.forwardLength==0)
                            cur_table+=1;
                    } break;
                    case kFrg_ClipType_match_image:{
                        //cur_table+=0;
                    } break;
                    default:{
                        assert(false);
                    } break;
                }
            }
        }


        bool tryMatchImageAt(const TColorMatch& colorMatcher,int subX0,int subY0,int subWidth,int subHeight,const TClipNode& referenceNode,
                             int* out_matchX0,int* out_matchY0,frg_TMatchType* out_matchType){
            if (referenceNode.type!=kFrg_ClipType_match_image) return false;
            TUInt32 matchX0=unpackMatchX(m_matchXYList[referenceNode.matchIndex]);
            TUInt32 matchY0=unpackMatchY(m_matchXYList[referenceNode.matchIndex]);
            for (int y=-1; y<=1; ++y) {
                for (int x=-1; x<=1; ++x) {
                    int curMatchX0=matchX0+x*kFrg_ClipWidth;
                    int curMatchY0=matchY0+y*kFrg_ClipHeight;
                    if (colorMatcher.isMatchAt(subX0,subY0,subWidth,subHeight,curMatchX0,curMatchY0,out_matchType)){
                        *out_matchX0=curMatchX0;
                        *out_matchY0=curMatchY0;
                        return true;
                    }
                }
            }
            return false;
        }
        
        bool findMatchImage(const TColorMatch& colorMatcher,int cur_nx,int cur_ny,
                            int* out_matchX0,int* out_matchY0,frg_TMatchType* out_matchType){
            const TInt curNodeIndex=cur_ny*m_nodeWidth+cur_nx;
            const TClipNode& _curNode=m_clipNodeList[curNodeIndex];
            const int subWidth=_curNode.colors.width;
            const int subHeight=_curNode.colors.height;
            
            if ((subWidth==kFrg_ClipWidth)&&(subHeight==kFrg_ClipHeight)){
                return colorMatcher.findNodeMatch(cur_nx,cur_ny,out_matchX0,out_matchY0,out_matchType);
            }else{
                //m_colorMatcher.findMatch 只支持匹配标准大小的node, 边界的node做了一些额外匹配.
                const int subX0=cur_nx*kFrg_ClipWidth;
                const int subY0=cur_ny*kFrg_ClipHeight;
                if (cur_nx>0){
                    const TClipNode& curNodeLeft=m_clipNodeList[curNodeIndex-1];
                    if (tryMatchImageAt(colorMatcher,subX0,subY0,subWidth,subHeight,curNodeLeft,out_matchX0,out_matchY0,out_matchType))
                        return true;
                }
                if (cur_ny>0){
                    const TClipNode& curNodeTop=m_clipNodeList[curNodeIndex-m_nodeWidth];
                    if (tryMatchImageAt(colorMatcher,subX0,subY0,subWidth,subHeight,curNodeTop,out_matchX0,out_matchY0,out_matchType))
                        return true;
                }
                return false;
            }
        }

        void addNodeColorsToTable(const TPixels32Ref& node_colors){
            const Color32* pline=node_colors.pColor;
            for (int y=0;y<node_colors.height;++y){
                for (int x=0;x<node_colors.width;++x){
                    m_colorTable.push_back(Color24(pline[x].r,pline[x].g,pline[x].b));
                }
                pline=node_colors.nextLine(pline);
            }
        }

        static void packIndexs(std::vector<TByte>& sub_indexList,int ibit){
            int size=(int)sub_indexList.size();
            for (int i=0; i<size; ++i) assert(sub_indexList[i]<(1<<ibit));
            switch (ibit) {
                case 4:
                case 3:
                case 2:
                case 1:{
                    int newSize=0;
                    int curValue=0;
                    int curBit=0;
                    for (int i=0;i<size;++i){
                        curValue|=(sub_indexList[i]<<curBit);
                        curBit+=ibit;
                        if (curBit>=8){
                            sub_indexList[newSize]=curValue;
                            ++newSize;
                            curValue>>=8;
                            curBit-=8;
                        }
                    }
                    if (curBit>0){
                        sub_indexList[newSize]=curValue;
                        ++newSize;
                    }
                    sub_indexList.resize(newSize);
                } break;
                case 0:{
                    sub_indexList.resize(0);
                } break;
                default:{
                    assert(false);
                } break;
            }
        }

        TInt addAMatch(TUInt32 matchX0,TUInt32 matchY0){
            TUInt32 matchPosXY=packMatchXY(matchX0, matchY0);
            TInt matchIndex=(TInt)m_matchXYList.size();
            m_matchXYList.push_back(matchPosXY);
            return matchIndex;
        }

    private:
        float                   m_colorQuality;
        bool                    m_isMustFitColorTable;
        TPixels32Ref            m_srcRef;
        int                     m_nodeWidth;
        int                     m_nodeHeight;
        std::vector<TClipNode>  m_clipNodeList;
        std::vector<Color24>    m_colorTable;       //公共调色板.
        std::vector<TByte>      m_indexList;        //索引.
        std::vector<TUInt32>    m_matchXYList;      //帧内匹配列表.
        TUInt*                  m_out_bgrColorTableLength;
    };

    ///

    void bgrZiper_save(std::vector<TByte>& out_buf,const TPixels32Ref& ref,float colorQuality,bool isMustFitColorTable,TUInt* out_bgrColorTableLength){
        *out_bgrColorTableLength=0;
        if (ref.getIsEmpty()) return;
        TColorZip colorZip(colorQuality,isMustFitColorTable,out_bgrColorTableLength);
        colorZip.saveTo(out_buf,ref);
    }

}//end namespace frg
