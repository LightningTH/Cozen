#include <string.h>
#include "BBSSysop.h"
#include "BBSUser.h"
#include "BBSUserEdit.h"
#include "BBSUserSearch.h"

BBSMenu::MenuEntryStruct SysopsMenu[] = {
    {.Name = 0}
};

BBSSysop::BBSSysop(ConnectionBase *Conn, BBSRender *Render) :
    BBSMenu(Conn, Render)
{
    Header = "SYSOP";
    InitSysop();
}


void BBSSysop::InitSysop()
{
    BBSMenu::MenuEntryStruct *SysopsMenu;
    size_t CurID = 0;

    //create our system menu based on what is available
    SysopsMenu = (BBSMenu::MenuEntryStruct *)calloc(2, sizeof(BBSMenu::MenuEntryStruct));
    AnnouncementID = SIZE_MAX;
    FileID = SIZE_MAX;

    //setup the user editor
    SysopsMenu[CurID].ID = 0;
    SysopsMenu[CurID].Name = "User Editor";
    SysopsMenu[CurID].Help = "Allow user editing";
    CurID++;

    //if we have announcements then create a menu entry
    if(GetBBSMenuPlugin("announcements"))
    {
        AnnouncementID = CurID;
        CurID++;

        SysopsMenu = (BBSMenu::MenuEntryStruct *)realloc(SysopsMenu, sizeof(BBSMenu::MenuEntryStruct) * (CurID + 1));
        memset(&SysopsMenu[AnnouncementID], 0, sizeof(BBSMenu::MenuEntryStruct));
        SysopsMenu[AnnouncementID].ID = AnnouncementID;
        SysopsMenu[AnnouncementID].Name = "Edit Announcements";
        SysopsMenu[AnnouncementID].Help = "Edit system announcements";
    }

    if(GetBBSMenuPlugin("files"))
    {
        FileID = CurID;
        CurID++;

        SysopsMenu = (BBSMenu::MenuEntryStruct *)realloc(SysopsMenu, sizeof(BBSMenu::MenuEntryStruct) * (CurID + 1));
        memset(&SysopsMenu[FileID], 0, sizeof(BBSMenu::MenuEntryStruct));
        SysopsMenu[FileID].ID = FileID;
        SysopsMenu[FileID].Name = "File Management";
        SysopsMenu[FileID].Help = "Manage files on the system";
    }

    //make sure the last entry is empty
    memset(&SysopsMenu[CurID], 0, sizeof(BBSMenu::MenuEntryStruct));
    _Menu = SysopsMenu;
}

size_t BBSSysop::DoAction(size_t ID)
{
    BBSMenu *menu;
    BBSUserSearch *Search;
    BBSUser *User;
    BBSMenuPluginInit Init;

    if(ID == SIZE_MAX)
        return 1;

    if(ID == 0)
    {
        //give a search option
        while(1)
        {
            //find a user to edit
            Search = new BBSUserSearch(_Conn, _Render);
            Search->HandleMenu();
            User = Search->GetUser();
            delete Search;

            //if we don't have a user then exit the loop
            if(!User)
                break;

            //edit the user
            menu = new BBSUserEdit(_Conn, _Render, User);
            menu->HandleMenu();
            delete menu;
            delete User;
        }
    }
    else if(ID == AnnouncementID)
    {
        //get announcements
        Init = GetBBSMenuPlugin("announcements");
        if(Init)
        {
            menu = Init(BBSMENU_PlUGIN_TYPE::TYPE_NORMAL_WITHFLAG, _Conn, _Render, 0);
            menu->HandleMenu();
            delete menu;
        }
    }
    else if(ID == FileID)
    {
    }

    return 1;
}

const uint8_t BBSSysop::RequiredPrivilegeLevel()
{
    return BBSUser::LEVEL_SYSOP;
}

BBSMenu *RegisterBBSSysop(ConnectionBase *Connection, BBSRender *Render)
{
    return new BBSSysop(Connection, Render);
}

__attribute__((constructor)) static void init()
{
    RegisterBBSMenu(RegisterBBSSysop);
}