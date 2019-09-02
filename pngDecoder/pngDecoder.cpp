#include <stdlib.h>
#include <stdio.h>


typedef unsigned char u8;
typedef bool b8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;

#define FOURCC(charA,charB) ((charA)[0] == (charB)[0] && (charA)[1] == (charB)[1] && (charA)[2] == (charB)[2] && (charA)[3] == (charB)[3])
#define MAX_BITS 16

struct PNGIHDR
{
	int width;
	int height;
	u8 bitDepth;
	u8 colorType;
	u8 compressionMethod;
	u8 filterMethod;
	u8 interlaceMethod;

};
struct PNGIDATHDR 
{
	u8 CMF;
	u8 FLG;
};
struct PNGIDAT
{
	void * data;
	PNGIDATHDR header;
	int isFirstChunk;
	PNGIDAT* next;
};
struct PNGFILE
{
	PNGIHDR header;
	PNGIDAT data;
};

void endianSwapInt(int *x)
{
	*x = (*x>>24 & 0xff) | (*x>>8 & 0xff00) | (*x<<8&0xff0000) | (*x<<24 & 0xff000000);
}

int main()
{
	FILE * f = fopen("colorPicker.png","r");
	fseek(f,0,SEEK_END);
	int length  = ftell(f);
	fseek(f,0,SEEK_SET);

	u8 * buffer = (u8*)malloc(length);
	fread(buffer,1,length,f);
	fclose(f);
	
	int error = 0;
		u8 tbuf [] = {137,80,78,71,13,10,26,10};
		for(int i = 0;i<8;i++)
		{
			if(tbuf[i] != buffer[i])
			{
				perror("this file doesn't conform to the png spec");				
				//return -1;
			}
		}

		PNGFILE file={0};

		for (int i = 8; i<length;i++)
		{
			if(FOURCC(buffer + i,"IHDR"))
			{
				PNGIHDR *header = (PNGIHDR*)(buffer+i+4);
				u8 * tcbuff = (u8*)&header->width;
				endianSwapInt(&header->width);
				endianSwapInt(&header->height);
				file.header = *header;				
			}
			if(FOURCC(buffer + i,"PLTE"))
				break;
			if(FOURCC(buffer + i,"IDAT"))
			{
				if(file.data.data)
				{
					PNGIDAT **itr= &file.data.next;
					while(*itr)
					{
						itr= &(*itr)->next;
					}
					*itr = (PNGIDAT*)calloc(1,sizeof(PNGIDAT));
					(**itr).data = buffer+i+4;
				}
				else
				{
					file.data.data = buffer+i + 4;
					file.data.isFirstChunk = true;
					PNGIDATHDR hdr = {*(u8*)file.data.data, *((u8*)file.data.data+ 1)};
					
					u8 CM = (hdr.CMF & 0x0f);
					u8 CINFO =  ((hdr.CMF & 0xf0)>>4);

					int widnowSize = 1<<(CINFO + 8);
					u8 FCHECK = hdr.FLG & 0x1f;
					u8 FDICT = (hdr.FLG & 0x20)>>5;
					u8 FLEVEL = hdr.FLG >>6; 
					u16 fcheckTest = ((u16)hdr.CMF<<8 | hdr.FLG) % 31;
					if(fcheckTest != 0)
					{
						error = 1;
						//Stop Operation	
					}
					u8 * startOfCodeLengths = buffer + i + 4 + 2;


					u8 BFINAL =  *startOfCodeLengths & 0x1;  // first bit
					u8 BTYPE   = (*startOfCodeLengths & 0x6)>>1; // second two bits 
					
					if(BTYPE == 2){ // dynamic huffman codes

						u8 HLIT = (*startOfCodeLengths & 0xf8)>>3; // 5 remaining bits
									startOfCodeLengths++; // move over to next byte since the current byte has been consumed
						u8 HDIST = *startOfCodeLengths & 0x1f;
						u16 xt = *(u16*)startOfCodeLengths;
						xt = (xt>>5) & 0x1f;
						u8 HCLEN = (*startOfCodeLengths >>5 | (*(startOfCodeLengths+1) & 1)<<3);
						startOfCodeLengths++;

						u32 literalLengthCodes = HLIT + 257;
						u32 distanceCodes = HDIST + 1;
						u32 codeLengthCodes = HCLEN + 4;
						u8 codeLengthForCodeLength[19]={};
						u8 codeLengthsScrambleIndices[19] = {16, 17, 18,0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
								
						u32 bitsLeftOver = 0;
						u32 bitIndex = 1;
						u16 t = 0;
						u16 codeLengthRead=0;
						for(u32 codeLengthIndex = 0;codeLengthIndex<codeLengthCodes;)
						{
							if(bitsLeftOver!=0)
							{
								t = ((*startOfCodeLengths | (*(startOfCodeLengths+1)<<8)) & ((1<<bitIndex)-1))<<(3-bitsLeftOver);
								codeLengthForCodeLength[codeLengthsScrambleIndices[codeLengthIndex-1]] |= t;									
								bitsLeftOver = 0;
								codeLengthRead++;
							}
							else 
							{
								t = (*startOfCodeLengths | (*(startOfCodeLengths+1)<<8)) >> bitIndex;
								codeLengthForCodeLength[codeLengthsScrambleIndices[codeLengthIndex++]] = t & 0x7;bitIndex +=3;
								codeLengthRead++;
								if(bitIndex == 16)
								{
									bitIndex = 0;
									bitsLeftOver = 0;
									startOfCodeLengths+=2;									
								}
								else if(bitIndex>16)	
								{
									bitsLeftOver = bitIndex - 16;
									bitIndex= bitsLeftOver;
									startOfCodeLengths+=2;
								}										
							}														
						}
						if(bitIndex < 16)
						{
							t >>= 3;
							t &=  (1 <<(16 - bitIndex) - 1);
						}


						u32 bl_count[19]= {};
						u32 maxBits = 0;
						for (u32 j  =0 ;j<codeLengthCodes;j++)
						{	
							if(codeLengthForCodeLength[codeLengthsScrambleIndices[j]] != 0)
							{
								maxBits = maxBits > codeLengthForCodeLength[codeLengthsScrambleIndices[j]] ?
										  maxBits : codeLengthForCodeLength[codeLengthsScrambleIndices[j]];
								bl_count[codeLengthForCodeLength[codeLengthsScrambleIndices[j]]]++;
							}
						}

						// calculate 
						u16 code = 0;
						u8 next_code[32]= {};
						bl_count[0] = 0;
						struct huff_tree{u32 Len;u32 Code;};
						huff_tree tree[1024] = {};
						for (u32 bits = 1; bits <= maxBits;bits++)
						{
							 code = (code + bl_count[bits-1]) <<1;
							 next_code[bits] = code;
							 
							 for(u32 bitsOffset =0 ; bitsOffset<bl_count[bits];bitsOffset++)
							 {
								tree[code+bitsOffset].Len = bits;
							 }
						}

						// assign numerical values to all codes
						u32 len = 0;

						u32 max_code = 1<<maxBits;
						for (u32 n = 0;  n <= max_code; n++) 
						{
							len = tree[n].Len;
							if (len != 0) 
							{
								tree[n].Code = next_code[len];
								next_code[len]++;
							}
						}


					}

					
				}

			}
			if(FOURCC(buffer + i,"IEND"))
				break;
		}

	


	printf("%d/n",length);
	return 0; 
}