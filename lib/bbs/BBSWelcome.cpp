#include "BBSWelcome.h"
#include "BBSUser.h"

BBSWelcome::BBSWelcome(ConnectionBase *Conn, BBSRender *Render) :
    BBSMenu(Conn, Render)
{
}

size_t BBSWelcome::DoAction(size_t ID)
{
    //all done, exit
    return 0;
}

void BBSWelcome::RenderHelpMenu()
{
    return;
}

void BBSWelcome::RenderMenu()
{
    BBSMenu *Announcements = 0;
    BBSMenu *Messages = 0;
    BBSMenuPluginInit Init;
    time_t LastAnnouncementTime;
    bool NewSinceLastRead;

    //render our menu and wait for a key press
    _Render->Clear();

    //move Y down a bit to center everything
    _Render->SetY((BBSRender::RENDER_HEIGHT - 3 - 3 - 4) / 2);
    _Render->DrawHeader(BBSRender::COLOR_BLACK, BBSRender::COLOR_GREEN, "welcome to");
    _Render->DrawHeader(BBSRender::COLOR_BLACK, BBSRender::COLOR_YELLOW, "l i g h t n i n g  b b s");
    _Render->OffsetY(-3);
    _Render->SetX(68);
    _Render->PrintF("beta", BBSRender::COLOR_YELLOW_BRIGHT, BBSRender::COLOR_RED);

    //move for message info
    _Render->SetX(0);
    _Render->OffsetY(5);

    //get announcements
	Init = GetBBSMenuPlugin("announcements");
	if(Init)
	{
		Announcements = Init(BBSMENU_PlUGIN_TYPE::TYPE_NO_PARAMS, 0, 0, 0);
        Announcements->GetValue("LastTime", &LastAnnouncementTime, sizeof(LastAnnouncementTime));

        //if there is new announcements highlight our entry
        if(LastAnnouncementTime > GlobalUser->GetPreviousLoginTime())
            _Render->PrintF("New announcements\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_YELLOW);
	}

	Init = GetBBSMenuPlugin("messages");
	if(Init)
	{
		Messages = Init(BBSMENU_PlUGIN_TYPE::TYPE_NO_PARAMS, 0, 0, 0);
        Messages->GetValue("NewSinceLastRead", &NewSinceLastRead, sizeof(NewSinceLastRead));

        if(NewSinceLastRead)
            _Render->PrintF("New messages\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_YELLOW);
        else
            _Render->PrintF("No new messages\n", BBSRender::COLOR_BLACK, TextColor);
    }


    _Render->PrintF("[%2~C%1~]ontinue ", BBSRender::COLOR_BLACK, TextColor);
    _Render->Render(UserDataColor);

    if(Announcements)
        delete Announcements;
    if(Messages)
        delete Messages;
}

BBSMenu *CreateWelcomeMenu(BBSMENU_PlUGIN_TYPE Type, ConnectionBase *Conn, BBSRender *Render, void *Value)
{
    return new BBSWelcome(Conn, Render);
}

__attribute__((constructor)) static void init()
{
    RegisterBBSMenuPlugin(CreateWelcomeMenu, "welcome");
}