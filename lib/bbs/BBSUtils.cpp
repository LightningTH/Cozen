#include "BBSUtils.h"
#include "BBSUser.h"
#include "BBSUserEdit.h"
#include "BBSStats.h"

BBSMenu::MenuEntryStruct UtilsMenu[] = {
    {.ID = 0, .Name = "Who's Online", .Help = "Show who all is online"},
    {.ID = 1, .Name = "Stats", .Help = "Statistics for the BBS"},
    {.ID = 2, .Name = "My Settings", .Help = "Your profile settings"},
    {.ID = 3, .Name = "Quit", .Help = "Quit/Logoff BBS"},
    {.Name = 0}
};

BBSUtils::BBSUtils(ConnectionBase *Conn, BBSRender *Render) :
    BBSMenu(Conn, Render)
{
    Header = "UTILITIES";
    _Menu = UtilsMenu;
}

size_t BBSUtils::DoAction(size_t ID)
{
    BBSMenu *menu;
    uint8_t Buffer[2];

    switch(ID)
    {
        case 0: //who's online
            _Render->Clear();
            _Render->PrintF("Feature not created yet in this version\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED_BRIGHT);
            _Render->PrintF("[%2~C%1~]ontinue ", BBSRender::COLOR_BLACK, TextColor, OptionColor);
            _Render->Render(UserDataColor);

            _Conn->Readline((char *)Buffer, 2);
            Buffer[1] = 0;
            break;

        case 1: //system and user stats
            menu = new BBSStats(_Conn, _Render);
            menu->HandleMenu();
            delete menu;
            break;

        case 2: //user settings
            menu = new BBSUserEdit(_Conn, _Render);
            menu->HandleMenu();
            delete menu;
            break;

        case 3: //quit
            close(0);
            close(1);
            close(2);
            _exit(0);
            break;
    };

    return 1;
}

BBSMenu *RegisterBBSUtils(ConnectionBase *Connection, BBSRender *Render)
{
    return new BBSUtils(Connection, Render);
}

__attribute__((constructor)) static void init()
{
    RegisterBBSMenu(RegisterBBSUtils);
}