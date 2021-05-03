#ifndef NEKISAHLOTH_LIB_BBS_RENDER
#define NEKISAHLOTH_LIB_BBS_RENDER

#include <unistd.h>
#include <stdint.h>
#include <ConnectionBase.h>
#include <stdarg.h>

class BBSRender
{
	public:
		static const size_t RENDER_WIDTH = 80;
		static const size_t RENDER_HEIGHT = 25;

		typedef enum RENDER_COLOR : uint8_t
		{
			COLOR_BLACK = 30,
			COLOR_RED,
			COLOR_GREEN,
			COLOR_YELLOW,
			COLOR_BLUE,
			COLOR_MAGENTA,
			COLOR_CYAN,
			COLOR_WHITE,
			COLOR_GREY = 90,
			COLOR_RED_BRIGHT,
			COLOR_GREEN_BRIGHT,
			COLOR_YELLOW_BRIGHT,
			COLOR_BLUE_BRIGHT,
			COLOR_MAGENTA_BRIGHT,
			COLOR_CYAN_BRIGHT,
			COLOR_WHITE_BRIGHT,
			COLOR_NOCHANGE = 0
		} RENDER_COLOR;

		BBSRender(ConnectionBase *Conn);
		~BBSRender();

		void Clear();
		void DrawHeader(RENDER_COLOR BGColor, RENDER_COLOR FGColor, const char *Header);
		void DrawHeaderAt(RENDER_COLOR BGColor, RENDER_COLOR FGColor, size_t x, size_t y, const char *Header);
		void DrawBox(size_t x1, size_t y1, size_t x2, size_t y2, RENDER_COLOR BorderColor, RENDER_COLOR BorderBackgroundColor, RENDER_COLOR BoxBackground);
		size_t PrintF(size_t x, size_t y, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, ...);
		size_t PrintF(size_t x, size_t y, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, ...);
		size_t PrintF(const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, ...);
		size_t PrintF(const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, ...);
		size_t PrintF(const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, ...);
		size_t PrintF(const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, ...);
		size_t PrintF(size_t x, size_t y, size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, ...);
		size_t PrintF(size_t x, size_t y, size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, ...);
		size_t PrintF(size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, ...);
		size_t PrintF(size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, ...);
		size_t PrintF(size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, ...);
		size_t PrintF(size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, ...);
		void Render(RENDER_COLOR UserColor);
		void Render(RENDER_COLOR UserColor, bool Conceal);

		size_t GetX();
		size_t GetY();
		size_t SetX(size_t X);
		size_t SetY(size_t Y);
		size_t OffsetX(ssize_t X);
		size_t OffsetY(ssize_t Y);

		size_t GetStringLen(const char *str);
		void DrawColors(bool Color);

	private:
		ConnectionBase *_Conn;
		size_t _X, _Y;				//current X/Y location as we write things
		bool _Color;
		bool _LastConceal;

		typedef struct CharDetailStruct
		{
			RENDER_COLOR ForeColor;
			RENDER_COLOR BackColor;
			char *Data;
		} CharDetailStruct;

		CharDetailStruct _Display[RENDER_WIDTH][RENDER_HEIGHT];

		void FreeData(char *Data);
		const char *GetString(CharDetailStruct *entry);
		void SetString(CharDetailStruct *entry, const char *str);
		void SetString(CharDetailStruct *entry, const char *str, size_t len);
		size_t GetLineIndex(CharDetailStruct *entry);
		size_t PrintF(size_t *x, size_t *y, size_t max_x, const char *data, RENDER_COLOR BGColor, RENDER_COLOR Color1, RENDER_COLOR Color2, RENDER_COLOR Color3, RENDER_COLOR Color4, va_list args);
};

#endif
