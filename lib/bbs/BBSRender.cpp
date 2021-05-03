#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <BBSMenu.h>
#include <string>
#include <unicode/brkiter.h>
#include <unicode/ucnv.h>
#include "BBSRender.h"

using namespace icu;
using namespace std;

//#define DISABLE_SCREENRESET

#define CHAR_WIDTH 5
#define CHAR_HEIGHT 3

typedef struct CharStruct
{
  uint8_t Width;
  uint8_t Lines[CHAR_HEIGHT][CHAR_WIDTH];
} CharStruct;

CharStruct HeaderChars[] = {
#include "chars.h"
};

/*
            buffer[CurWidth + 0] = 0xe0 | (val >> 12);
            buffer[CurWidth + 1] = 0x80 | ((val >> 6) & 0x3f);
            buffer[CurWidth + 2] = 0x80 | (val & 0x3f);
*/

const char *unicode_boxes[0x39] = {
//first entry is 0x2550
"\xe2\x95\x90", "\xe2\x95\x91", "\xe2\x95\x92", "\xe2\x95\x93",
"\xe2\x95\x94", "\xe2\x95\x95", "\xe2\x95\x96", "\xe2\x95\x97",
"\xe2\x95\x98", "\xe2\x95\x99", "\xe2\x95\x9a", "\xe2\x95\x9b",
"\xe2\x95\x9c", "\xe2\x95\x9d", "\xe2\x95\x9e", "\xe2\x95\x9f",
"\xe2\x95\xa0", "\xe2\x95\xa1", "\xe2\x95\xa2", "\xe2\x95\xa3",
"\xe2\x95\xa4", "\xe2\x95\xa5", "\xe2\x95\xa6", "\xe2\x95\xa7",
"\xe2\x95\xa8", "\xe2\x95\xa9", "\xe2\x95\xaa", "\xe2\x95\xab",
"\xe2\x95\xac", "\xe2\x95\xad", "\xe2\x95\xae", "\xe2\x95\xaf",
"\xe2\x95\xb0", "\xe2\x95\xb1", "\xe2\x95\xb2", "\xe2\x95\xb3",
"\xe2\x95\xb4", "\xe2\x95\xb5", "\xe2\x95\xb6", "\xe2\x95\xb7",
"\xe2\x95\xb8", "\xe2\x95\xb9", "\xe2\x95\xba", "\xe2\x95\xbb",
"\xe2\x95\xbc", "\xe2\x95\xbd", "\xe2\x95\xbe", "\xe2\x95\xbf",
"\xe2\x96\x80", "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83",
"\xe2\x96\x84", "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87",
"\xe2\x96\x88"
};

const char *fixed_space = " ";

#define ESCAPE '\x1b'
#define PTR_FLAG (1L << 63)
#define PTR_ALLOC_FLAG (1L << 62)
#define StringIsPointer(str) (((size_t)(str) & (PTR_FLAG|PTR_ALLOC_FLAG)) != 0)

BBSRender::BBSRender(ConnectionBase *Conn)
{
	memset(_Display, 0, sizeof(_Display));
	_Color = false;
	Clear();
	_Conn = Conn;
}

BBSRender::~BBSRender()
{
	Clear();
}

void BBSRender::FreeData(char *Data)
{
	if((size_t)Data & PTR_ALLOC_FLAG)
		free((char *)((size_t)Data & ~PTR_ALLOC_FLAG));
}

const char *BBSRender::GetString(CharDetailStruct *entry)
{
	//if either pointer is set then return an appropriate pointer
	if((size_t)entry->Data & (PTR_FLAG | PTR_ALLOC_FLAG))
		return (const char *)((size_t)entry->Data & ~(PTR_FLAG | PTR_ALLOC_FLAG));
	else
	{
		//we have data embedded in the actual field, return a pointer to the struct entry
		return (const char *)(&entry->Data);
	}
}

void BBSRender::SetString(CharDetailStruct *entry, const char *str)
{
	SetString(entry, str, strlen(str));
}

void BBSRender::SetString(CharDetailStruct *entry, const char *str, size_t len)
{
	size_t str_len;

	//if the current entry is a pointer then free it
	FreeData(entry->Data);

	if(len == 0)
		str_len = strlen(str);
	else
		str_len = len;

	//if the string is less than 8 bytes then shove it into the entry directly
	if(str_len < 8)
	{
		//make sure the field is empty
		entry->Data = 0;

		//copy the data
		memcpy(&entry->Data, str, str_len);
	}
	else
	{
		//if the character after us is not a null byte or original length is 0
		//then allocate a buffer and copy to it
		if(!len || str[str_len+1])
		{
			entry->Data = (char *)malloc(str_len + 1);
			memcpy(entry->Data, str, str_len);
			entry->Data[str_len] = 0;

			//set allocated flag
			entry->Data = (char *)((size_t)str | PTR_FLAG | PTR_ALLOC_FLAG);
		}
		else
			entry->Data = (char *)((size_t)str | PTR_FLAG);	//set the pointer flag
	}
}

void BBSRender::Clear()
{
	size_t x, y;

	//remove all memory used
	for(y = 0; y < RENDER_HEIGHT; y++)
	{
		for(x = 0; x < RENDER_WIDTH; x++)
		{
			//if we have a display value and it isn't one of our fixed
			//strings then free it
			FreeData(_Display[x][y].Data);

			//erase everything
			SetString(&_Display[x][y], " ", 1);
			_Display[x][y].ForeColor = COLOR_BLACK;
			_Display[x][y].BackColor = COLOR_BLACK;
		}
	}

	//reset our X/Y
	_X = 0;
	_Y = 0;
}

void BBSRender::DrawHeader(RENDER_COLOR BGColor, RENDER_COLOR FGColor, const char *Header)
{
	size_t HeaderWidth = 0;
	uint8_t CurChar;
	size_t CharCount;
	const char *Temp = Header;

	//get the width of the header itself
	CharCount = 0;
	while(*Temp)
	{
		//get current char
		CurChar = (uint8_t)*Temp | 0x20;

		//now map it
		CurChar -= 'a';
		if(CurChar > 'z')
			CurChar = 'z' - 'a' + 1;
		if(*Temp == ' ')
			CurChar++;

		if((HeaderWidth + HeaderChars[CurChar].Width) > RENDER_WIDTH)
			break;

		HeaderWidth += HeaderChars[CurChar].Width;

		//move to next char
		Temp++;
		CharCount++;
	}

	//start drawing the header based on our width offset
	DrawHeaderAt(BGColor, FGColor, (RENDER_WIDTH - HeaderWidth) / 2, _Y, Header);

	//set our X/Y to be the row after the header
	_X = 0;
	_Y += CHAR_HEIGHT;
}

void BBSRender::DrawHeaderAt(RENDER_COLOR BGColor, RENDER_COLOR FGColor, size_t X, size_t Y, const char *Header)
{
	size_t HeaderWidth = 0;
	uint8_t CurChar;
	size_t CharCount;
	size_t WidthPos;
	size_t WidthCount;
	size_t CurY;
	const char *Temp = Header;

	//get the width of the header itself
	CharCount = 0;
	while(*Temp)
	{
		//get current char
		CurChar = (uint8_t)*Temp | 0x20;

		//now map it
		CurChar -= 'a';
		if(CurChar > 'z')
			CurChar = 'z' - 'a' + 1;
		if(*Temp == ' ')
			CurChar++;

		if((HeaderWidth + HeaderChars[CurChar].Width) > RENDER_WIDTH)
			break;

		HeaderWidth += HeaderChars[CurChar].Width;

		//move to next char
		Temp++;
		CharCount++;
	}

	//start drawing the header
	WidthPos = X;
	while(CharCount)
	{
		//get current char
		CurChar = (uint8_t)*Header | 0x20;

		//now map it
		CurChar -= 'a';
		if(CurChar > 'z')
			CurChar = 'z' - 'a' + 1;
		if(*Header == ' ')
			CurChar++;

		//draw the character
		for(CurY = 0; (CurY < CHAR_HEIGHT) && ((Y + CurY) < RENDER_HEIGHT); CurY++)
		{
			for(WidthCount = 0; WidthCount < HeaderChars[CurChar].Width; WidthCount++)
			{
				if(FGColor != COLOR_NOCHANGE)
					_Display[WidthPos + WidthCount][Y + CurY].ForeColor = FGColor;
				if(BGColor != COLOR_NOCHANGE)
					_Display[WidthPos + WidthCount][Y + CurY].BackColor = BGColor;

				//render the right character
				if(HeaderChars[CurChar].Lines[CurY][WidthCount] == 0xff)
					SetString(&_Display[WidthPos + WidthCount][Y + CurY], " ", 1);
				else if(HeaderChars[CurChar].Lines[CurY][WidthCount] == 0)
					break;
				else
					SetString(&_Display[WidthPos + WidthCount][Y + CurY], unicode_boxes[HeaderChars[CurChar].Lines[CurY][WidthCount] - 0x50], 3);
			}
		}

		//update our locations
		WidthPos += HeaderChars[CurChar].Width;
		Header++;
		CharCount--;
	}
}

size_t BBSRender::GetLineIndex(CharDetailStruct *entry)
{
	size_t Counter;
	const char *str;

	//return the lower portion of 0x25xx for the data entry if we have a match for the border
	//otherwise return 0

	//if a pointer then it isn't a potential entry
	if(StringIsPointer(entry->Data))
		return 0;

	//check against the array of objects that it could be
	str = GetString(entry);
	if(strlen(str) != 3)
		return 0;

	for(Counter = 0; Counter < (sizeof(unicode_boxes) / sizeof(char *)); Counter++)
	{
		if(memcmp(str, unicode_boxes[Counter], 4) == 0)
			return Counter + 0x50;
	}

	return 0;
}

void BBSRender::DrawBox(size_t x1, size_t y1, size_t x2, size_t y2, RENDER_COLOR BorderColor, RENDER_COLOR BorderBackgroundColor, RENDER_COLOR BoxBackground)
{
	size_t CurX, CurY;
	size_t LineIndex;
	uint8_t CharIndex;

	//create the border of the box, if our border overlaps another then add the appopriate join pieces
	if(x2 >= RENDER_WIDTH)
		x2 = RENDER_WIDTH - 1;
	if(y2 >= RENDER_HEIGHT)
		y2 = RENDER_HEIGHT - 1;

	if(x1 >= x2)
		return;
	if(y1 >= y2)
		return;

	//draw the top line of the box
	for(CurX = x1; CurX <= x2; CurX++)
	{
		//if we already have a border of some form then adjust our output based on it
		if(CurX == x1)
			CharIndex = 0x54;
		else if(CurX == x2)
			CharIndex = 0x57;
		else
			CharIndex = 0x50;

		//see if we already have a line in this spot
		LineIndex = GetLineIndex(&_Display[CurX][y1]);
		if(LineIndex)
		{
			//we got a hit, swap the character we are outputting for a different one
			switch(LineIndex)
			{
				case 0x50:	//vertical line
				case 0x66: //T
					if((CurX == x1) || (CurX == x2))
						CharIndex = 0x66;
					break;

				case 0x51:	//horizonal line
					if(CurX == x1)
						CharIndex = 0x60;
					else if(CurX == x2)
						CharIndex = 0x63;
					else
						CharIndex = 0x69;
					break;

				case 0x54:	//left top corner
					if(CurX == x2)
						CharIndex = 0x66;
					break;

				case 0x57:	//right top corner
					if(CurX == x1)
						CharIndex = 0x66;
					break;

				case 0x5A:	//left bottom corner
				case 0x60: //T right
					if(CurX == x1)
						CharIndex = 0x60;
					else if(CurX == x2)
						CharIndex = 0x6c;
					else
						CharIndex = 0x69;
					break;

				case 0x5D:	//right bottom corner
				case 0x63: //T left
					if(CurX == x1)
						CharIndex = 0x6c;
					else if(CurX == x2)
						CharIndex = 0x63;
					else
						CharIndex = 0x69;
					break;

				case 0x69:	//upside down T
				case 0x6c:	//plus
					if((CurX == x1) || (CurX == x2))
						CharIndex = 0x6c;
					else
						CharIndex = 0x69;
					break;
			}
		}

		//write our line char
		_Display[CurX][y1].ForeColor = BorderColor;
		_Display[CurX][y1].BackColor = BorderBackgroundColor;
		SetString(&_Display[CurX][y1], unicode_boxes[CharIndex - 0x50], 3);
	}

	//draw sides
	for(CurY = (y1 + 1); CurY < y2; CurY++)
	{
		//each step down requires checking both sides of the box
		CurX = x1;
		do
		{
			//up and down line
			CharIndex = 0x51;

			//see if we already have a line in this spot
			LineIndex = GetLineIndex(&_Display[CurX][CurY]);
			if(LineIndex)
			{
				//we got a hit, swap the character we are outputting for a different one
				switch(LineIndex)
				{
					case 0x50:	//vertical line
					case 0x66:	//T
					case 0x69:	//upside down T
					case 0x6c:	//plus
						if(CurX == x1)
							CharIndex = 0x63;
						else
							CharIndex = 0x60;
						break;

					case 0x54:	//left top corner
					case 0x5A:	//left bottom corner
					case 0x60: 	//T right
						if(CurX == x2)
							CharIndex = 0x60;
						break;

					case 0x57:	//right top corner
					case 0x5D:	//right bottom corner
					case 0x63: 	//T left
						if(CurX == x1)	//first entry, add T
							CharIndex = 0x63;
						break;
				}
			}

			//write our line char
			_Display[CurX][CurY].ForeColor = BorderColor;
			_Display[CurX][CurY].BackColor = BorderBackgroundColor;
			SetString(&_Display[CurX][CurY], unicode_boxes[CharIndex - 0x50], 3);

			//if the left side then check the right side otherwise exit
			if(CurX == x1)
				CurX = x2;
			else
				break;
		} while(1);
	}

	//draw the bottom line of the box
	for(CurX = x1; CurX <= x2; CurX++)
	{
		//if we already have a border of some form then adjust our output based on it
		if(CurX == x1)
			CharIndex = 0x5A;
		else if(CurX == x2)
			CharIndex = 0x5D;
		else
			CharIndex = 0x50;

		//see if we already have a line in this spot
		LineIndex = GetLineIndex(&_Display[CurX][y2]);
		if(LineIndex)
		{
			//we got a hit, swap the character we are outputting for a different one
			switch(LineIndex)
			{
				case 0x51:	//horizonal line
					if(CurX == x1)
						CharIndex = 0x60;
					else if(CurX == x2)
						CharIndex = 0x63;
					else
						CharIndex = 0x66;
					break;

				case 0x54:	//left top corner
				case 0x60: //T right
					if(CurX == x1)
						CharIndex = 0x60;
					else if(CurX == x2)
						CharIndex = 0x6c;
					else
						CharIndex = 0x66;
					break;

				case 0x57:	//right top corner
				case 0x63: //T left
					if(CurX == x1)
						CharIndex = 0x6c;
					else if(CurX == x2)
						CharIndex = 0x63;
					else
						CharIndex = 0x66;
					break;

				case 0x5A:	//left bottom corner
					if(CurX == x2)
						CharIndex = 0x69;
					break;

				case 0x5D:	//right bottom corner
					if(CurX == x1)
						CharIndex = 0x69;
					break;

				case 0x66: //T
				case 0x6c:	//plus
					if((CurX == x1) || (CurX == x2))
						CharIndex = 0x6c;
					else
						CharIndex = 0x66;
					break;

				case 0x69:	//upside down T
					if((CurX == x1) || (CurX == x2))
						CharIndex = 0x69;
					break;
			}
		}

		//write our line char
		_Display[CurX][y2].ForeColor = BorderColor;
		_Display[CurX][y2].BackColor = BorderBackgroundColor;
		SetString(&_Display[CurX][y2], unicode_boxes[CharIndex - 0x50], 3);
	}

	//erase the inside of the box
	for(CurY = y1 + 1; CurY < y2; CurY++)
	{
		for(CurX = x1 + 1; CurX < x2; CurX++)
		{
			//set as a space to erase it
			SetString(&_Display[CurX][CurY], " ", 1);

			//set the background color if need be
			if(BoxBackground != COLOR_NOCHANGE)
				_Display[CurX][CurY].BackColor = BoxBackground;
		}
	}
}

size_t BBSRender::GetStringLen(const char *str)
{
	UErrorCode status = U_ZERO_ERROR;
	BreakIterator *StrIT;
	size_t StrPos;

	//setup the data iterator for our incoming data format
	StrIT = BreakIterator::createCharacterInstance(Locale::getUS(), status);
	StrIT->setText(str);

	StrPos = 0;
	while(StrIT->next() != BreakIterator::DONE)
		StrPos++;

	return StrPos;
}

size_t BBSRender::PrintF(size_t *x, size_t *y, size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, va_list args)
{
	RENDER_COLOR CurColor;
	RENDER_COLOR ParseColor;
	size_t Ret;
	UErrorCode status = U_ZERO_ERROR;
	UnicodeString DataFmt, DataFmtSub;
	string DataCharUTF8;
	BreakIterator *DataIT;
	size_t DataPos;
	UnicodeString StrFmt, StrFmtSub;
	string StrCharUTF8;
	BreakIterator *StrIT;
	size_t StrPos;
	size_t x_start;
	uint8_t ZeroFill, LeftAlign;
	size_t Value;
	size_t ParamVal, ParamVal2;
	char *ParamStr;
	char ValBuffer[2];

	//if x or y is invalid then exit
	if((*x >= RENDER_WIDTH) || (*y >= RENDER_HEIGHT))
		return 0;

	Ret = 0;
	CurColor = Color1;

	//setup the data iterator for our incoming data format
	DataIT = BreakIterator::createCharacterInstance(Locale::getUS(), status);
	DataIT->setText(data);
	DataIT->getText().getText(DataFmt);
	x_start = *x;

	//go through each character, if it is a % then we need to handle it special
	DataPos = 0;
	while(DataIT->next() != BreakIterator::DONE)
	{
		//get the actual data for the character that will be rendered
		//the input can be unicode and some unicode entries are modifiers so the following gives us
		//the full unicode chunk of data needed for a single character render on the screen
		DataFmtSub.truncate(0);
		DataCharUTF8.erase();
		DataFmt.extract(DataPos, DataIT->current() - DataPos, DataFmtSub);
		DataPos = DataIT->current();
		DataFmtSub.toUTF8String(DataCharUTF8);

		//DataCharUTF8 contains a full UTF8 combo of bytes needed for rendering a single character on the screen
		if(!DataCharUTF8.compare("%"))
		{
			ParseColor = CurColor;
			Value = 0;
			ZeroFill = 0;
			LeftAlign = 0;
			while(1)
			{
				//get the next character
				if(DataIT->next() == BreakIterator::DONE)
					break;

				DataFmtSub.truncate(0);
				DataCharUTF8.erase();
				DataFmt.extract(DataPos, DataIT->current() - DataPos, DataFmtSub);
				DataPos = DataIT->current();
				DataFmtSub.toUTF8String(DataCharUTF8);

				//see if the next character is a number
				if((DataCharUTF8.c_str()[0] >= 0x30) && (DataCharUTF8.c_str()[0] <= 0x39))
				{
					Value = Value * 10;
					Value += DataCharUTF8.c_str()[0] - 0x30;

					//if our value is 0 then we know it is zero padded
					if(Value == 0)
						ZeroFill = 1;
				}
				else if(!DataCharUTF8.compare("`") || !DataCharUTF8.compare("~"))
				{
					//special flag, this changes our current selection of color for parsing
					switch(Value)
					{
						case 1:
							ParseColor = Color1;
							break;

						case 2:
							ParseColor = Color2;
							break;

						case 3:
							ParseColor = Color3;
							break;

						case 4:
							ParseColor = Color4;
							break;
						
						default:
							ParseColor = CurColor;
					};

					//reset value and zero fill
					Value = 0;
					ZeroFill = 0;

					//if ~ then stop looking
					if(!DataCharUTF8.compare("~"))
					{
						CurColor = ParseColor;
						break;
					}
				}
				else if(!DataCharUTF8.compare("-"))
				{
					if(LeftAlign)
						break;

					LeftAlign = 1;
				}
				else if(!DataCharUTF8.compare("*"))
				{
					Value = va_arg(args, size_t);
				}
				else
					break;
			}

			//figure out what character to operate on
			switch(DataCharUTF8.c_str()[0])
			{
				case 'c':
					ParamVal = va_arg(args, int);
					ValBuffer[0] = ParamVal & 0xff;
					ValBuffer[1] = 0;

					//fill in our display entry
					if(BGColor != COLOR_NOCHANGE)
						_Display[*x][*y].BackColor = BGColor;
					if(ParseColor != COLOR_NOCHANGE)
						_Display[*x][*y].ForeColor = ParseColor;
					SetString(&_Display[*x][*y], ValBuffer, 1);
					*x += 1;
					break;

				case 'd':
					ParamVal = va_arg(args, size_t);
					ParamVal2 = ParamVal;

					//see how long the number is
					if(ParamVal2)
					{
						StrPos = 0;
						while(ParamVal2)
						{
							ParamVal2 /= 10;
							StrPos++;
						}
					}
					else
						StrPos = 1;

					//add the number of 0's or spaces needed based on Value
					if(!LeftAlign)
					{
						while(StrPos < Value)
						{
							//fill in our display entry
							if(BGColor != COLOR_NOCHANGE)
								_Display[*x][*y].BackColor = BGColor;
							if(ParseColor != COLOR_NOCHANGE)
								_Display[*x][*y].ForeColor = ParseColor;
							if(ZeroFill)
								SetString(&_Display[*x][*y], "0", 1);
							else
								SetString(&_Display[*x][*y], " ", 1);
							Value--;
							*x += 1;
						}
					}

					//adjust x to the end location
					*x += StrPos;

					//now to fill in the value
					StrPos = 1;
					ValBuffer[1] = 0;

					//write out the value
					do
					{
						//fill in our display entry
						if(BGColor != COLOR_NOCHANGE)
							_Display[*x - StrPos][*y].BackColor = BGColor;
						if(ParseColor != COLOR_NOCHANGE)
							_Display[*x - StrPos][*y].ForeColor = ParseColor;
						ValBuffer[0] = 0x30 + (ParamVal % 10);
						SetString(&_Display[*x - StrPos][*y], ValBuffer, 1);

						ParamVal /= 10;
						StrPos++;
					} while(ParamVal);

					//if we have a left align then pad to the right
					if(LeftAlign)
					{
						while(StrPos < Value)
						{
							//fill in our display entry
							if(BGColor != COLOR_NOCHANGE)
								_Display[*x][*y].BackColor = BGColor;
							if(ParseColor != COLOR_NOCHANGE)
								_Display[*x][*y].ForeColor = ParseColor;
							SetString(&_Display[*x][*y], " ", 1);
							Value--;
							*x += 1;
						}
					}
					break;

				case 's':
					ParamStr = va_arg(args, char *);

					//setup the data iterator for our incoming data format
					StrIT = BreakIterator::createCharacterInstance(Locale::getUS(), status);
					StrIT->setText(ParamStr);
					StrIT->getText().getText(StrFmt);

					//if we have a value then see how many characters we will write and if we need
					//to output any whitespace
					if(Value)
					{
						StrPos = 0;
						while(StrIT->next() != BreakIterator::DONE)
							StrPos++;

						//output the needed number of spaces
						if(!LeftAlign)
						{
							while(StrPos < Value)
							{
								//fill in our display entry
								if(BGColor != COLOR_NOCHANGE)
									_Display[*x][*y].BackColor = BGColor;
								if(ParseColor != COLOR_NOCHANGE)
									_Display[*x][*y].ForeColor = ParseColor;
								SetString(&_Display[*x][*y], " ");
								StrPos++;
								*x += 1;

								//write the character to our position if valid
								if((*x >= RENDER_WIDTH) || (*x >= max_x))
									break;
							}
						}

						//reset StrIT so we can write the string
						StrIT->first();
					}

					//write each character and increase x
					StrPos = 0;
					while(StrIT->next() != BreakIterator::DONE)
					{
						//see above comment about why this large block
						StrFmtSub.truncate(0);
						StrCharUTF8.erase();
						StrFmt.extract(StrPos, StrIT->current() - StrPos, StrFmtSub);
						StrPos = StrIT->current();
						StrFmtSub.toUTF8String(StrCharUTF8);

						//if a newline then ajust x/y
						if(StrCharUTF8.c_str()[0] == '\n')
						{
							*x = x_start;
							*y += 1;
							if(*y >= RENDER_HEIGHT)
								break;
						}
						else
						{
							//write the character to our position if valid
							if((*x >= RENDER_WIDTH) || (*x >= max_x))
								break;

							//fill in our display entry
							if(BGColor != COLOR_NOCHANGE)
								_Display[*x][*y].BackColor = BGColor;
							if(ParseColor != COLOR_NOCHANGE)
								_Display[*x][*y].ForeColor = ParseColor;
							SetString(&_Display[*x][*y], StrCharUTF8.c_str());

							//move a spot over
							*x += 1;
							Ret++;
						}
					}

					//output the needed number of spaces if left aligned
					if(LeftAlign && (*x < RENDER_WIDTH) && (*x < max_x))
					{
						while(StrPos < Value)
						{
							//fill in our display entry
							if(BGColor != COLOR_NOCHANGE)
								_Display[*x][*y].BackColor = BGColor;
							if(ParseColor != COLOR_NOCHANGE)
								_Display[*x][*y].ForeColor = ParseColor;
							SetString(&_Display[*x][*y], " ");
							StrPos++;
							*x += 1;

							//write the character to our position if valid
							if((*x >= RENDER_WIDTH) || (*x >= max_x))
								break;
						}
					}

					break;

				case '~':
					//permanent change the current color so just break
					break;

				case '%':
				default:
					//write the character to our position if valid
					if((*x < RENDER_WIDTH) && (*x < max_x))
					{				
						//fill in our display entry
						if(BGColor != COLOR_NOCHANGE)
							_Display[*x][*y].BackColor = BGColor;
						if(ParseColor != COLOR_NOCHANGE)
							_Display[*x][*y].ForeColor = ParseColor;
						SetString(&_Display[*x][*y], DataCharUTF8.c_str());

						//move a spot over
						*x += 1;
						Ret++;
					}					
					break;
			}
		}
		else if(!DataCharUTF8.compare("\n"))
		{
			//move down a line and reset X to what it started as
			*x = x_start;
			*y += 1;

			Ret++;

			//if we fell off the bottom then we are done
			if(*y >= RENDER_HEIGHT)
				break;
		}
		else
		{
			//write the character to our position if valid
			if((*x < RENDER_WIDTH) && (*x < max_x))
			{				
				//fill in our display entry
				if(BGColor != COLOR_NOCHANGE)
					_Display[*x][*y].BackColor = BGColor;
				if(CurColor != COLOR_NOCHANGE)
					_Display[*x][*y].ForeColor = CurColor;
				SetString(&_Display[*x][*y], DataCharUTF8.c_str());

				//move a spot over
				*x += 1;
				Ret++;
			}
		}
	}

	//return how many characters we rendered
	return Ret;
}

size_t BBSRender::PrintF(size_t x, size_t y, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color4);
    Ret = PrintF(&x, &y, RENDER_WIDTH, data, BGColor, Color1, Color2, Color3, Color4, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(size_t x, size_t y, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color3);
    Ret = PrintF(&x, &y, RENDER_WIDTH, data, BGColor, Color1, Color2, Color3, Color1, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color4);
    Ret = PrintF(&_X, &_Y, RENDER_WIDTH, data, BGColor, Color1, Color2, Color3, Color4, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color3);
    Ret = PrintF(&_X, &_Y, RENDER_WIDTH, data, BGColor, Color1, Color2, Color3, Color1, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color2);
    Ret = PrintF(&_X, &_Y, RENDER_WIDTH, data, BGColor, Color1, Color2, Color1, Color1, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color1);
    Ret = PrintF(&_X, &_Y, RENDER_WIDTH, data, BGColor, Color1, Color1, Color1, Color1, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(size_t x, size_t y, size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color4);
    Ret = PrintF(&x, &y, max_x, data, BGColor, Color1, Color2, Color3, Color4, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(size_t x, size_t y, size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color3);
    Ret = PrintF(&x, &y, max_x, data, BGColor, Color1, Color2, Color3, Color1, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color4);
    Ret = PrintF(&_X, &_Y, max_x, data, BGColor, Color1, Color2, Color3, Color4, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color3);
    Ret = PrintF(&_X, &_Y, max_x, data, BGColor, Color1, Color2, Color3, Color1, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color2);
    Ret = PrintF(&_X, &_Y, max_x, data, BGColor, Color1, Color2, Color1, Color1, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::PrintF(size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, ...)
{
	va_list argp;
	size_t Ret;

    va_start(argp, Color1);
    Ret = PrintF(&_X, &_Y, max_x, data, BGColor, Color1, Color1, Color1, Color1, argp);
    va_end(argp);
	return Ret;
}

size_t BBSRender::GetX()
{
	return _X;
}

size_t BBSRender::GetY()
{
	return _Y;
}

size_t BBSRender::SetX(size_t X)
{
	if(X >= RENDER_WIDTH)
		_X = RENDER_WIDTH - 1;
	else
		_X = X;
	return _X;
}

size_t BBSRender::SetY(size_t Y)
{
	if(Y >= RENDER_HEIGHT)
		_Y = RENDER_HEIGHT - 1;
	else
		_Y = Y;
	return _Y;
}

size_t BBSRender::OffsetX(ssize_t Offset)
{
	if(((ssize_t)_X + Offset) < 0)
		_X = 0;
	else if((_X + Offset) >= RENDER_WIDTH)
		_X = RENDER_WIDTH - 1;
	else
		_X += (size_t)Offset;
	return _X;
}

size_t BBSRender::OffsetY(ssize_t Offset)
{
	if(((ssize_t)_Y + Offset) < 0)
		_Y = 0;
	else if((_Y + Offset) >= RENDER_HEIGHT)
		_Y = RENDER_HEIGHT - 1;
	else
		_Y += (size_t)Offset;
	return _Y;
}

void BBSRender::Render(RENDER_COLOR UserColor)
{
	Render(UserColor, false);
}

void BBSRender::Render(RENDER_COLOR UserColor, bool Conceal)
{
	char *Buffer;
	size_t BufferSize, BufferPos;
	size_t CurX, CurY;
	RENDER_COLOR CurFG, CurBG;

	//first need to calculate the size of our buffer along with the required escape codes to set colors as they change
	//without being wasteful

	//set CurFG and CurBG to invalid values
	CurFG = COLOR_NOCHANGE;
	CurBG = COLOR_NOCHANGE;

	BufferSize = 0;

	if(_LastConceal)
		BufferSize = 5;

#ifndef DISABLE_SCREENRESET
	//erase display, \x1b[28m\x1b[1;1H\x1b[J
	BufferSize += 9;
#endif

	//loop through display
	for(CurY = 0; CurY < RENDER_HEIGHT; CurY++)
	{
		for(CurX = 0; CurX < RENDER_WIDTH; CurX++)
		{
			//if the fg or bg don't match then add the required escape code sizes
			if(_Color && (CurFG != _Display[CurX][CurY].ForeColor))
			{
				//\x1b[xxm
				BufferSize += 5;
				CurFG = _Display[CurX][CurY].ForeColor;

				//if the background also doesn't match then add space for ;yy
				if(CurBG != _Display[CurX][CurY].BackColor)
				{
					BufferSize += 3;
					CurBG = _Display[CurX][CurY].BackColor;
					if(CurBG > 90)
						BufferSize += 1;	//account for 3 digit value
				}
			}
			else if(_Color && (CurBG != _Display[CurX][CurY].BackColor))
			{
				//\x1b[xxm
				BufferSize += 5;
				CurBG = _Display[CurX][CurY].BackColor;
				if(CurBG > 90)
					BufferSize += 1;	//account for 3 digit value
			}

			//add the actual length of this field
			BufferSize += strlen(GetString(&_Display[CurX][CurY]));
		}

		//move to next line, \x1b[1E
		BufferSize += 4;
	}

	//add in the user color at the end for where the cursor ends up
	if(_Color)
		BufferSize += 5;

	//add in code for setting the X/Y for the user input cursor
	//\x1b[xx;yyH
	BufferSize += 8;

	//conceal if need be
	if(Conceal)
		BufferSize += 4;

	//allocate our buffer
	Buffer = (char *)malloc(BufferSize);

	//now fill the buffer
	BufferPos = 0;

	if(_LastConceal)
	{
		memcpy(Buffer, "\x1b[28m", 5);
		BufferPos += 5;
		_LastConceal = false;
	}

#ifndef DISABLE_SCREENRESET
	memcpy(&Buffer[BufferPos], "\x1b[1;1H\x1b[J", 9);
	BufferPos += 9;
#endif

	//set CurFG and CurBG to invalid values
	CurFG = COLOR_NOCHANGE;
	CurBG = COLOR_NOCHANGE;

	//loop through display and add it
	for(CurY = 0; CurY < RENDER_HEIGHT; CurY++)
	{
		for(CurX = 0; CurX < RENDER_WIDTH; CurX++)
		{
			//if the fg or bg don't match then add the required escape code sizes
			if(_Color && (CurFG != _Display[CurX][CurY].ForeColor))
			{
				CurFG = _Display[CurX][CurY].ForeColor;

				Buffer[BufferPos] = ESCAPE;
				Buffer[BufferPos+1] = '[';
				Buffer[BufferPos+2] = 0x30 + (CurFG / 10);
				Buffer[BufferPos+3] = 0x30 + (CurFG % 10);
				BufferPos += 4;

				//if the background also doesn't match then add space for ;yy
				if(CurBG != _Display[CurX][CurY].BackColor)
				{
					CurBG = _Display[CurX][CurY].BackColor;
					Buffer[BufferPos] = ';';
					if(CurBG >= 90)
					{
						Buffer[BufferPos+1] = 0x31;
						Buffer[BufferPos+2] = 0x30;
						Buffer[BufferPos+3] = 0x30 + (CurBG % 10);
						BufferPos += 4;
					}
					else
					{
						Buffer[BufferPos+1] = 0x30 + (CurBG / 10) + 1;
						Buffer[BufferPos+2] = 0x30 + (CurBG % 10);
						BufferPos += 3;
					}
				}
				Buffer[BufferPos] = 'm';
				BufferPos++;
			}
			else if(_Color && (CurBG != _Display[CurX][CurY].BackColor))
			{
				CurBG = _Display[CurX][CurY].BackColor;

				//\x1b[xxm
				Buffer[BufferPos] = ESCAPE;
				Buffer[BufferPos+1] = '[';
				if(CurBG >= 90)
				{
					Buffer[BufferPos+2] = 0x31;
					Buffer[BufferPos+3] = 0x30;
					Buffer[BufferPos+4] = 0x30 + (CurBG % 10);
					BufferPos += 5;
				}
				else
				{
					Buffer[BufferPos+2] = 0x30 + (CurBG / 10) + 1;
					Buffer[BufferPos+3] = 0x30 + (CurBG % 10);
					BufferPos += 4;
				}
				Buffer[BufferPos] = 'm';
				BufferPos++;
			}

			//add the actual data of this field
			memcpy(&Buffer[BufferPos], GetString(&_Display[CurX][CurY]), strlen(GetString(&_Display[CurX][CurY])));
			BufferPos += strlen(GetString(&_Display[CurX][CurY]));
		}

		//move to next line, \x1b[1E
		Buffer[BufferPos] = ESCAPE;
		Buffer[BufferPos+1] = '[';
		Buffer[BufferPos+2] = '1';
		Buffer[BufferPos+3] = 'E';
		BufferPos += 4;
	}

	//add our user data color
	if(_Color)
	{
		Buffer[BufferPos] = ESCAPE;
		Buffer[BufferPos+1] = '[';
		Buffer[BufferPos+2] = 0x30 + (UserColor / 10);
		Buffer[BufferPos+3] = 0x30 + (UserColor % 10);
		Buffer[BufferPos+4] = 'm';
		BufferPos += 5;
	}

	//set final X/Y location for the user pointer
	Buffer[BufferPos] = ESCAPE;
	Buffer[BufferPos+1] = '[';
	Buffer[BufferPos+2] = 0x30 + ((_Y+1) / 10);
	Buffer[BufferPos+3] = 0x30 + ((_Y+1) % 10);
	Buffer[BufferPos+4] = ';';
	Buffer[BufferPos+5] = 0x30 + ((_X+1) / 10);
	Buffer[BufferPos+6] = 0x30 + ((_X+1) % 10);
	Buffer[BufferPos+7] = 'H';
	BufferPos += 8;

	//if conceal then send the coneal escape code
	if(Conceal)
	{
		Buffer[BufferPos] = ESCAPE;
		Buffer[BufferPos+1] = '[';
		Buffer[BufferPos+2] = '8';
		Buffer[BufferPos+3] = 'm';
		BufferPos += 4;
		_LastConceal = true;
	}

	//if our buffer pos != our length then we have a major issue
	if(BufferPos != BufferSize)
	{
		close(0);
		close(1);
		close(2);
		_exit(0);
	}

	//write it out
	_Conn->Write(Buffer, BufferSize);

	//free
	free(Buffer);
}

void BBSRender::DrawColors(bool Color)
{
	_Color = Color;
}
