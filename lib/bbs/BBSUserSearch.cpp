
#include <string.h>
#include "BBSUserSearch.h"
#include "BBSUser.h"
#include "BBSUtils.h"

BBSMenu::MenuEntryStruct UserSearchMenu[] = {
    {.ID = 1, .Name = "Search By Username", .Help = "Search by username"},
    {.ID = 2, .Name = "Search by ID", .Help = "Search by ID"},
    {.Name = 0}
};

BBSUserSearch::BBSUserSearch(ConnectionBase *Conn, BBSRender *Render) :
    BBSMenu(Conn, Render)
{
    LargeHeader = "USER SEARCH";
    Header = "SEARCH";
    _UserListMenu = 0;
    _SelectionType = SELECTION_NUMERIC;
    _FindUser = 0;
    _SelectedUser = 0;
    memset(_UserMatches, 0, sizeof(_UserMatches));

    HeaderColor = BBSRender::COLOR_CYAN;
    BoxLineColor = BBSRender::COLOR_BLUE;
    OptionColor = BBSRender::COLOR_YELLOW;

    //add ourselves as the menu
    AddMenu(this);

    //display the search menu
    DisplaySearchMenu();
}

BBSUserSearch::~BBSUserSearch()
{
    size_t i;

    //free the buffer
    if(_UserListMenu)
    {
        free(_UserListMenu);
        _UserListMenu = 0;
    }

    //if we have an active find then delete it
    if(_FindUser)
    {
        delete _FindUser;
        _FindUser = 0;
    }

    //delete any found entries
    for(i = 0; i < USER_MATCH_COUNT; i++)
    {
        if(_UserMatches[i])
            delete _UserMatches[i];
    }
    memset(_UserMatches, 0, sizeof(_UserMatches));
}

const uint8_t BBSUserSearch::RequiredPrivilegeLevel()
{
    return 0;
}

BBSUser *BBSUserSearch::GetUser()
{
    //we assume the caller will delete this
    return _SelectedUser;
}

void BBSUserSearch::GenerateFindMenu()
{
    size_t Count;

    //generate the menu of found users
    if(!_UserListMenu)
    {
        _UserListMenu = (BBSMenu::MenuEntryStruct *)calloc(USER_MATCH_COUNT + 1, sizeof(BBSMenu::MenuEntryStruct));
        for(Count = 0; Count < USER_MATCH_COUNT; Count++)
        {
            _UserListMenu[Count].ID = Count + 1;
            _UserListMenu[Count].Color = BBSRender::COLOR_GREEN;
        }
    }

    //make sure we don't have any previous entries left
    for(Count = 0; Count < USER_MATCH_COUNT; Count++)
    {
        if(_UserMatches[Count])
            delete _UserMatches[Count];
        _UserMatches[Count] = 0;
        _UserListMenu[Count].Name = 0;
    }

    //keep adding entries to our list
    for(Count = 0; Count < USER_MATCH_COUNT; Count++)
    {
        //make a copy and add it to the menu
        _UserMatches[Count] = new BBSUser(_FindUser);
        _UserListMenu[Count].Name = _UserMatches[Count]->GetName();

        //find next user
        if(!_FindUser->FindNext())
        {
            _FindUser->FindDone();
            delete _FindUser;
            _FindUser = 0;
            break;
        }
    }

    //update the menu entry
    _Menu = _UserListMenu;

    //set the prompt up
    if(_FindUser)
        Prompt = "%3~[%2~B%3~]%1~ack  %3~[%2~M%3~]%1~ore  Selection>> ";
    else
        Prompt = "%3~[%2~B%3~]%1~ack  Selection>> ";
}

void BBSUserSearch::DisplaySearchMenu()
{
    size_t i;

    //display the search menu
    _Menu = UserSearchMenu;

    //delete any found entries
    for(i = 0; i < USER_MATCH_COUNT; i++)
    {
        if(_UserMatches[i])
            delete _UserMatches[i];

        _UserMatches[i] = 0;
    }

    //if a find is active then get rid of it
    if(_FindUser)
        delete _FindUser;

    _FindUser = 0;
    Prompt = "%3~[%2~B%3~]%1~ack  %3~[%2~1-2%3~]%1~>> ";
}

void BBSUserSearch::SearchByID()
{
    char Buffer[100];
    size_t UserID;
    char *EndPtr;

    //ask for ID to look up
    _Render->Clear();
    _Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, "SEARCH BY ID");
    _Render->SetX(0);
    _Render->OffsetY(1);
    _Render->PrintF("User ID: ", BBSRender::COLOR_BLACK, TextColor);
    _Render->Render(UserDataColor);

    memset(Buffer, 0, sizeof(Buffer));
    _Conn->Readline(Buffer, sizeof(Buffer) - 1);

    //convert the ID, if the end pointer is not null then
    //we had invalid data to conver
    UserID = strtoul(Buffer, &EndPtr, 10);
    if(*EndPtr || (EndPtr == Buffer))
        return;

    //find the user
    _FindUser = new BBSUser();
    if(_FindUser->Find(UserID))
    {
        _SelectedUser = new BBSUser(_FindUser);
    }
    else
    {
        //put the name onto the screen
        _Render->PrintF("%s\n", BBSRender::COLOR_BLACK, UserDataColor, Buffer);

        _Render->OffsetY(2);
        _Render->SetX(0);
        _Render->PrintF("No users matched", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED);
    }

    delete _FindUser;
    _FindUser = 0;
}

void BBSUserSearch::SearchByName()
{
    char Buffer[257];

    //ask for ID to look up
    _Render->Clear();
    _Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, "SEARCH BY NAME");
    _Render->SetX(0);
    _Render->OffsetY(1);
    _Render->PrintF("Partial Username to Match: ", BBSRender::COLOR_BLACK, TextColor);
    _Render->Render(UserDataColor);

    memset(Buffer, 0, sizeof(Buffer));
    _Conn->Readline(Buffer, sizeof(Buffer) - 1);

    //find the user
    _FindUser = new BBSUser();
    if(_FindUser->Find(Buffer))
        GenerateFindMenu();
    else
    {
        //put the name onto the screen
        _Render->PrintF("%s\n", BBSRender::COLOR_BLACK, UserDataColor, Buffer);

        _Render->OffsetY(2);
        _Render->SetX(0);
        _Render->PrintF("No users matched. Press enter to continue", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED);
        _Render->Render(UserDataColor);
        _Conn->Readline(Buffer, 1);

        delete _FindUser;
        _FindUser = 0;
    }
}

size_t BBSUserSearch::DoAction(size_t ID)
{
    size_t Count;

    if(_Menu == UserSearchMenu)
    {
        //in search menu, handle it
        switch(ID)
        {
            case 1: //search by name
                SearchByName();
                break;

            case 2: //search by ID
                SearchByID();
                break;

            case CharActionToInt('b'): //back
                return 0;
        };
    }
    else if(_Menu == _UserListMenu)
    {
        //handle selecting a user from the list
        if(ID && (ID < (USER_MATCH_COUNT + 1)))
        {
            ID--;
            if(_UserMatches[ID])
            {
                //set our user to edit
                _SelectedUser = _UserMatches[ID];
                _UserMatches[ID] = 0;

                //delete all of the other entries
                for(Count = 0; Count < USER_MATCH_COUNT; Count++)
                {
                    if(_UserMatches[ID])
                        delete _UserMatches[ID];

                    _UserMatches[ID] = 0;
                }

                //close our find entry
                if(_FindUser)
                    delete _FindUser;
                _FindUser = 0;
            }
        }
        else if(ID == CharActionToInt('b'))
            DisplaySearchMenu();
        else if(ID == CharActionToInt('m')) //more
        {
            //if there is more to find then find it
            if(_FindUser)
                GenerateFindMenu();
        }
    }

    //if a user was selected then return 0 indicating we are done
    if(_SelectedUser)
        return 0;

    return 1;
}
