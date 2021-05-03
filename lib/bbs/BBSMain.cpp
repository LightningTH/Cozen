#include <stdio.h>
#include <malloc.h>
#include <byteswap.h>
#include <stdarg.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include "BBSMain.h"
#include "BBSFiles.h"
#include "BBSUser.h"
#include "BBSMessages.h"

BBSMain::BBSMain(ConnectionBase *Conn, BBSRender *Render) :
    BBSMenu(Conn, Render)
{
    Header = "SYSTEM";

    SystemMenu = 0;
    _Menu = 0;
}

void BBSMain::InitMain()
{
    size_t CurID = 0;

    //create our system menu based on what is available
    SystemMenu = (BBSMenu::MenuEntryStruct *)malloc(sizeof(BBSMenu::MenuEntryStruct));
    AnnouncementID = SIZE_MAX;
    FileID = SIZE_MAX;
    MessageID = SIZE_MAX;

    //if we have announcements then create a menu entry
    if(GetBBSMenuPlugin("announcements"))
    {
        AnnouncementID = CurID;
        CurID++;

        SystemMenu = (BBSMenu::MenuEntryStruct *)realloc(SystemMenu, sizeof(BBSMenu::MenuEntryStruct) * (CurID + 1));
        memset(&SystemMenu[AnnouncementID], 0, sizeof(BBSMenu::MenuEntryStruct));
        SystemMenu[AnnouncementID].ID = AnnouncementID;
        SystemMenu[AnnouncementID].Name = "Announcements";
        SystemMenu[AnnouncementID].Help = "System announcements";
    }

    if(GetBBSMenuPlugin("files"))
    {
        FileID = CurID;
        CurID++;

        SystemMenu = (BBSMenu::MenuEntryStruct *)realloc(SystemMenu, sizeof(BBSMenu::MenuEntryStruct) * (CurID + 1));
        memset(&SystemMenu[FileID], 0, sizeof(BBSMenu::MenuEntryStruct));
        SystemMenu[FileID].ID = FileID;
        SystemMenu[FileID].Name = "Files Area";
        SystemMenu[FileID].Help = "Files area of system";
    }

    if(GetBBSMenuPlugin("messages"))
    {
        MessageID = CurID;

        //2 entries for messages
        CurID+=2;

        SystemMenu = (BBSMenu::MenuEntryStruct *)realloc(SystemMenu, sizeof(BBSMenu::MenuEntryStruct) * (CurID + 1));
        memset(&SystemMenu[MessageID], 0, sizeof(BBSMenu::MenuEntryStruct) * 2);
        SystemMenu[MessageID].ID = MessageID;
        SystemMenu[MessageID].Name = "Message Area";
        SystemMenu[MessageID].Help = "Message area for texting";

        SystemMenu[MessageID + 1].ID = MessageID + 1;
        SystemMenu[MessageID + 1].Name = "New Messages";
        SystemMenu[MessageID + 1].Help = "New messages since last check";
    }

    //make sure the last entry is empty
    memset(&SystemMenu[CurID], 0, sizeof(BBSMenu::MenuEntryStruct));
    _Menu = SystemMenu;
}

void BBSMain::Refresh()
{
    BBSMenu *temp_menu;
    BBSMenuPluginInit Init;
    time_t LastAnnouncementTime;
    bool NewSinceLastRead;

    if(!SystemMenu)
        InitMain();

    //get announcements
	Init = GetBBSMenuPlugin("announcements");
	if(Init)
	{
		temp_menu = Init(BBSMENU_PlUGIN_TYPE::TYPE_NO_PARAMS, 0, 0, 0);
        temp_menu->GetValue("LastTime", &LastAnnouncementTime, sizeof(LastAnnouncementTime));
		delete temp_menu;

        //if there is new announcements highlight our entry
        if(LastAnnouncementTime > GlobalUser->GetPreviousLoginTime())
            SystemMenu[AnnouncementID].Color = BBSRender::COLOR_YELLOW;
	}

	Init = GetBBSMenuPlugin("messages");
	if(Init)
	{
		temp_menu = Init(BBSMENU_PlUGIN_TYPE::TYPE_NO_PARAMS, 0, 0, 0);
        temp_menu->GetValue("NewSinceLastRead", &NewSinceLastRead, sizeof(NewSinceLastRead));
        delete temp_menu;

        if(NewSinceLastRead)
            SystemMenu[MessageID + 1].Color = BBSRender::COLOR_GREEN;
        else
            SystemMenu[MessageID + 1].Color = BBSRender::COLOR_NOCHANGE;
    }
}

size_t BBSMain::DoAction(size_t ID)
{
    BBSMenu *menu;
    BBSMenuPluginInit Init;

    if(ID == SIZE_MAX)
        return 1;

    if(ID == AnnouncementID)
    {
        //get announcements
        Init = GetBBSMenuPlugin("announcements");
        if(Init)
        {
            menu = Init(BBSMENU_PlUGIN_TYPE::TYPE_NORMAL, _Conn, _Render, 0);
            menu->HandleMenu();
            delete menu;
        }
    }
    else if(ID == FileID)
    {
        //get files
        Init = GetBBSMenuPlugin("files");
        if(Init)
        {
            menu = Init(BBSMENU_PlUGIN_TYPE::TYPE_NORMAL, _Conn, _Render, 0);
            menu->HandleMenu();
            delete menu;
        }
    }
    else if((ID == MessageID) || (ID == (MessageID + 1)))
    {
        //get messages
        Init = GetBBSMenuPlugin("messages");
        if(Init)
        {
            menu = Init(BBSMENU_PlUGIN_TYPE::TYPE_NORMAL_WITH_VALUE, _Conn, _Render, (void *)(ID - MessageID));
            menu->HandleMenu();
            delete menu;
        }
    }

    return 1;
}

const uint8_t BBSMain::RequiredPrivilege()
{
    return 0;
}

BBSMenu *RegisterBBSMain(ConnectionBase *Connection, BBSRender *Render)
{
    return new BBSMain(Connection, Render);
}

__attribute__((constructor)) static void init()
{
    RegisterBBSMenu(RegisterBBSMain);
}