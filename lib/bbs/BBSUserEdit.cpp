#include <string>
#include <string.h>
#include "BBSUserEdit.h"
#include "BBSUser.h"
#include "BBSUtils.h"

using namespace std;

void BBSUserEdit::Init(bool IsSysop)
{
    LargeHeader = "USER EDIT";
    Header = "EDIT";
    _UserEditMenu = 0;
    _UserListMenu = 0;
    _SelectionType = SELECTION_NUMERIC;
    _EditedUser = 0;
    _IsSysop = IsSysop;

    HeaderColor = BBSRender::COLOR_CYAN;
    BoxLineColor = BBSRender::COLOR_BLUE;
    OptionColor = BBSRender::COLOR_YELLOW;

    //add ourselves as the menu
    AddMenu(this);
}

BBSUserEdit::BBSUserEdit(ConnectionBase *Conn, BBSRender *Render) :
    BBSMenu(Conn, Render)
{
    Init(false);
    _EditedUser = GlobalUser;

    //generate a new menu
    GenerateMenu();
}

BBSUserEdit::BBSUserEdit(ConnectionBase *Conn, BBSRender *Render, BBSUser *User) :
    BBSMenu(Conn, Render)
{
    Init(true);
    _EditedUser = User;
  
    //generate the menu
    GenerateMenu();
}

BBSUserEdit::~BBSUserEdit()
{
    size_t i;
    if(_UserEditMenu)
    {
        //delete our string objects
        for(i = 0; _UserEditMenu[i].Name; i++)
        {
            if(_UserEditMenu[i].Data)
                delete (string *)_UserEditMenu[i].Data;
        }

        //free the buffer
        free(_UserEditMenu);
        _UserEditMenu = 0;
    }

    //free the buffer
    if(_UserListMenu)
    {
        free(_UserListMenu);
        _UserListMenu = 0;
    }
}

const uint8_t BBSUserEdit::RequiredPrivilegeLevel()
{
    if(_IsSysop)
        return BBSUser::LEVEL_SYSOP;

    return 0;
}

void BBSUserEdit::GenerateMenu()
{
    size_t i;
    size_t Count;
    size_t AmountN[2];
    char Marker;
    string *NewStr;
    char Buffer[100];
    BBSGlobalStats::TrafficStatsStruct Stats;

    if(!_UserEditMenu)
    {
        Count = 8;
        if(_IsSysop)
            Count += 1;
        _UserEditMenu = (BBSMenu::MenuEntryStruct *)calloc(Count, sizeof(BBSMenu::MenuEntryStruct));
        for(i = 0; i < Count; i++)
        {
            _UserEditMenu[i].ID = i+1;
            _UserEditMenu[i].Color = BBSRender::COLOR_GREEN;
        }
    }

    //fill in our info that won't change between menu generations
    if(_UserEditMenu[0].Data)
        NewStr = (string *)_UserEditMenu[0].Data;
    else
        NewStr = new string();
    NewStr->clear();
    NewStr->append("ID:                  ");
    snprintf(Buffer, sizeof(Buffer) - 1, "%ld", _EditedUser->GetID());
    NewStr->append(Buffer);
    _UserEditMenu[0].Name = NewStr->c_str();
    _UserEditMenu[0].Data = NewStr;
    _UserEditMenu[0].Help = "Unique user ID for the account";
    _UserEditMenu[0].ColorOnColon = true;

    if(_UserEditMenu[1].Data)
        NewStr = (string *)_UserEditMenu[1].Data;
    else
        NewStr = new string();
    NewStr->clear();
    NewStr->append("Name:                ");
    NewStr->append(_EditedUser->GetName());
    _UserEditMenu[1].Name = NewStr->c_str();
    _UserEditMenu[1].Data = NewStr;
    _UserEditMenu[1].Help = "Your username";
    _UserEditMenu[1].ColorOnColon = true;

    if(_UserEditMenu[2].Data)
        NewStr = (string *)_UserEditMenu[2].Data;
    else
        NewStr = new string();
    NewStr->clear();
    NewStr->append("Privilege Level:     ");
    snprintf(Buffer, sizeof(Buffer) - 1, "%d", _EditedUser->GetPrivilegeLevel());
    NewStr->append(Buffer);
    _UserEditMenu[2].Name = NewStr->c_str();
    _UserEditMenu[2].Data = NewStr;
    _UserEditMenu[2].Help = "The current privilege level";
    if(_IsSysop)
        _UserEditMenu[2].Color = BBSRender::COLOR_RED;
    _UserEditMenu[2].ColorOnColon = true;

    _UserEditMenu[3].Help = "Determine if the welcome screen is displayed";
    _UserEditMenu[3].Color = BBSRender::COLOR_RED;
    _UserEditMenu[3].ColorOnColon = true;

    _UserEditMenu[4].Help = "Change if menu color is displayed";
    _UserEditMenu[4].Color = BBSRender::COLOR_RED;
    _UserEditMenu[4].ColorOnColon = true;

    _EditedUser->GetTrafficStats(&Stats);
    GetAmountAndMarker(Stats.DownloadSize, &Marker, AmountN);

    if(_UserEditMenu[5].Data)
        NewStr = (string *)_UserEditMenu[5].Data;
    else
        NewStr = new string();
    NewStr->clear();
    NewStr->append("Files Downloaded: ");
    snprintf(Buffer, sizeof(Buffer) - 1, "%4d (%ld.%02ld %cb)", Stats.DownloadCount, AmountN[0], AmountN[1], Marker);
    NewStr->append(Buffer);
    _UserEditMenu[5].Name = NewStr->c_str();
    _UserEditMenu[5].Data = NewStr;
    _UserEditMenu[5].Help = "Download statistics";
    _UserEditMenu[5].ColorOnColon = true;

    GetAmountAndMarker(Stats.UploadSize, &Marker, AmountN);
    if(_UserEditMenu[6].Data)
        NewStr = (string *)_UserEditMenu[6].Data;
    else
        NewStr = new string();
    NewStr->clear();
    NewStr->append("Files Uploaded:   ");
    snprintf(Buffer, sizeof(Buffer) - 1, "%4d (%ld.%02ld %cb)", Stats.UploadCount, AmountN[0], AmountN[1], Marker);
    NewStr->append(Buffer);
    _UserEditMenu[6].Name = NewStr->c_str();
    _UserEditMenu[6].Data = NewStr;
    _UserEditMenu[6].Help = "Upload statistics";
    _UserEditMenu[6].ColorOnColon = true;

    if(_IsSysop)
    {
        _UserEditMenu[7].Help = "Is a user account locked";
        _UserEditMenu[7].ColorOnColon = true;
    }

    //users can alter these
    if(_EditedUser->GetShowWelcome())
        _UserEditMenu[3].Name = "Welcome Screen:      Shown";
    else
        _UserEditMenu[3].Name = "Welcome Screen:      Hidden";

    if(_EditedUser->GetColorChoice() == BBSUser::COLOR_UNSET)
        _UserEditMenu[4].Name = "Display Color Unset";
    else if(_EditedUser->GetColorChoice() == BBSUser::COLOR_YES)
        _UserEditMenu[4].Name = "Color:               Turned On";
    else
        _UserEditMenu[4].Name = "Color:               Turned Off";

    if(_IsSysop)
    {
        if(_EditedUser->IsLocked())
        {
            _UserEditMenu[7].Name = "Account Locked:      Yes";
            _UserEditMenu[7].Color = BBSRender::COLOR_RED;
        }
        else
        {
            _UserEditMenu[7].Name = "Account Locked:      No";
            _UserEditMenu[7].Color = BBSRender::COLOR_BLUE;
        }
    }

    //set the menu up
    _Menu = _UserEditMenu;
    if(_IsSysop)
        Prompt = "%3~[%2~B%3~]%1~ack  %3~[%2~C%3~]%1~hange Password  %3~[%2~R%3~]%1~eset Stats  %3~[%2~1-8%3~]%1~>> ";
    else
        Prompt = "%3~[%2~B%3~]%1~ack  %3~[%2~C%3~]%1~hange Password  %3~[%2~1-7%3~]%1~>> ";
}

void BBSUserEdit::ChangePassword()
{
    char OldPassword[100];
    char NewPassword[100];

    OldPassword[0] = 0;
    if(!_IsSysop)
    {
        //ask for current password
        _Render->Clear();
        _Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, "PASSWORD CHANGE");
        _Render->SetX(0);
        _Render->OffsetY(1);
        _Render->PrintF("Current Password: ", BBSRender::COLOR_BLACK, TextColor);
        _Render->Render(BBSRender::COLOR_BLACK, true);

        //get password
        memset(OldPassword, 0, sizeof(OldPassword));
        _Conn->Readline(OldPassword, sizeof(OldPassword) - 1);
    }

    //ask for new password
    _Render->SetX(0);
    _Render->OffsetY(1);
    _Render->PrintF("New Password: ", BBSRender::COLOR_BLACK, TextColor);
    _Render->Render(BBSRender::COLOR_BLACK, true);

    memset(NewPassword, 0, sizeof(NewPassword));
    _Conn->Readline(NewPassword, sizeof(NewPassword) - 1);

    //see if it works
    _Render->SetX(0);
    _Render->OffsetY(1);
    if(_EditedUser->SetPassword(OldPassword, NewPassword))
        _Render->PrintF("Password change %2~SUCCESSFUL\n", BBSRender::COLOR_BLACK, OptionColor, BBSRender::COLOR_GREEN);
    else
        _Render->PrintF("Password change %2~FAILED\n", BBSRender::COLOR_BLACK, OptionColor, BBSRender::COLOR_RED);

    _Render->OffsetY(1);
    _Render->SetX(0);
    _Render->PrintF("Press enter to continue", BBSRender::COLOR_BLACK, TextColor);
    _Render->Render(UserDataColor);

    _Conn->Readline(OldPassword, 1);
}

void BBSUserEdit::ChangePrivilegeLevel()
{
    char Buffer[100];
    size_t NewPrivLevel;
    char *EndPtr;

    //ask for new privilege level to look up
    _Render->Clear();
    _Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, "PRIVILEGE LEVEL");
    _Render->SetX(0);
    _Render->OffsetY(1);
    _Render->PrintF("Current Privilege Level: %2`d\n", BBSRender::COLOR_BLACK, TextColor, OptionColor, _EditedUser->GetPrivilegeLevel());
    _Render->PrintF("New Privilege Level: ", BBSRender::COLOR_BLACK, TextColor);
    _Render->Render(UserDataColor);

    memset(Buffer, 0, sizeof(Buffer));
    _Conn->Readline(Buffer, sizeof(Buffer) - 1);

    //confirm it is a full number
    NewPrivLevel = strtoul(Buffer, &EndPtr, 10);
    if(*EndPtr || (EndPtr == Buffer))
        return;

    //make sure it isn't too large
    if(NewPrivLevel > BBSUser::LEVEL_SYSOP)
        NewPrivLevel = BBSUser::LEVEL_SYSOP;

    _EditedUser->SetPrivilegeLevel(NewPrivLevel);
}

size_t BBSUserEdit::DoAction(size_t ID)
{
    switch(ID)
    {
        case 3: //privilege level
            if(_IsSysop)
                ChangePrivilegeLevel();
            GenerateMenu();
            break;

        case 4: //change welcome display
            _EditedUser->SetShowWelcome(!_EditedUser->GetShowWelcome());
            GenerateMenu();
            break;

        case 5: //change color selection
            if(_EditedUser->GetColorChoice() == BBSUser::COLOR_YES)
            {
                //turn colors off
                _EditedUser->SetColorChoice(BBSUser::COLOR_NO);
                _Render->DrawColors(false);
            }
            else
            {
                //turn colors on
                _EditedUser->SetColorChoice(BBSUser::COLOR_YES);
                _Render->DrawColors(true);
            }
            GenerateMenu();
            break;

        case 8: //lock/unlock account
            if(_IsSysop)
                _EditedUser->SetLocked(!_EditedUser->IsLocked());

            GenerateMenu();
            break;

        case CharActionToInt('b'): //back
            return 0;

        case CharActionToInt('c'): //change password
            ChangePassword();
            break;

        case CharActionToInt('r'): //reset statistics
            if(_IsSysop)
                _EditedUser->ResetStats();

            GenerateMenu();
            break;
    };

    return 1;
}
