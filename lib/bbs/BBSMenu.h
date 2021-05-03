#ifndef NEKISAHLOTH_LIB_BBS_MENU
#define NEKISAHLOTH_LIB_BBS_MENU

#include <unistd.h>
#include <stdint.h>
#include <ConnectionBase.h>
#include <BBSRender.h>


typedef enum BBSMENU_PlUGIN_TYPE : size_t
{
	TYPE_NORMAL,
	TYPE_NORMAL_WITHFLAG,
	TYPE_NO_PARAMS,
	TYPE_NORMAL_WITH_VALUE
} BBSMENU_PlUGIN_TYPE;

class BBSMenu;
typedef BBSMenu *(*BBSMenuClassInit)(ConnectionBase *Connection, BBSRender *Render);
typedef BBSMenu *(*BBSMenuPluginInit)(BBSMENU_PlUGIN_TYPE Type, ConnectionBase *Connection, BBSRender *Render, void *Value);

#define CharActionToInt(c) (c - 'a' + 0x100)

class BBSMenu
{
	public:
		typedef struct MenuEntryStruct
		{
			size_t ID;
			const char *Name;
			const char *Help;
			uint8_t Level;
			BBSRender::RENDER_COLOR Color;
			bool ColorOnColon;
			void *Data;			//data specific to a class for this entry
		} MenuEntryStruct;

		typedef enum SelectionTypeEnum : uint8_t
		{
			SELECTION_ALPHA,
			SELECTION_NUMERIC,
			SELECTION_NONE,
		} SelectionTypeEnum;

		BBSMenu(ConnectionBase *Conn, BBSRender *Render);
		virtual ~BBSMenu();
		virtual size_t DoAction(size_t ID);
		virtual const uint8_t RequiredPrivilegeLevel();			//required level to see this menu, 255 = SYSOP
		const MenuEntryStruct *GetEntryList();
		void AddMenu(BBSMenu *NewMenu);
		void AddMenu(BBSMenuClassInit MenuInit);
		virtual void RenderMenu();
		virtual void RenderHelpMenu();
		virtual void HandleMenu();
		virtual void Refresh();
		void RefreshGroup();

		void GetAmountAndMarker(size_t Amount, char *Marker, size_t *Result);
		virtual size_t GetValue(const char *Name, void *Data, size_t DataLen);
		virtual void SetValue(const char *Name, void *Data, size_t DataLen);

	protected:
		ConnectionBase *_Conn;
		BBSRender *_Render;
		MenuEntryStruct *_Menu;			//our local menu sent to our parent menu for displaying
		BBSMenu **_MenuGroups;			//the group of menus under us that we are responsible for rendering
		SelectionTypeEnum _SelectionType;		//type of selections
		bool _ShowHelp;					//show help menu

		//various flags for rendering
		uint8_t RenderHeader;
		uint8_t RenderTimeLeft;
		BBSRender::RENDER_COLOR TextColor;				//color for most text
		BBSRender::RENDER_COLOR OptionColor;			//color for a user selection
		BBSRender::RENDER_COLOR UserDataColor;			//color of user controlled data
		BBSRender::RENDER_COLOR BoxLineColor;			//color of lines for the box
		BBSRender::RENDER_COLOR BoxBackgroundColor;		//color of background of box
		BBSRender::RENDER_COLOR HeaderColor;			//color of the header when displayed
		const char *Prompt;								//prompt to display
		const char *Header;								//header to display on boxes
		const char *LargeHeader;						//header to display at the top

};

extern BBSMenu *MainMenu;
void RegisterBBSMenu(BBSMenuClassInit Init);
void RegisterBBSMenuPlugin(BBSMenuPluginInit Init, const char *Name);
BBSMenuPluginInit GetBBSMenuPlugin(const char *Name);

#endif
