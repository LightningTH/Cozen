#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <string>
#include "BBSMenu.h"
#include <BBSUser.h>

using namespace std;

BBSMenu *MainMenu = 0;

#define HelpMenuStr "Command Help"

typedef struct MenuSelectionStruct
{
    BBSMenu *Menu;
    const BBSMenu::MenuEntryStruct *EntryDetails;
} MenuSelectionStruct;

struct MenuPluginStruct {
    struct MenuPluginStruct *Next;
    BBSMenuPluginInit Init;
    const char *Name;
};

MenuPluginStruct *MenuPluginEntries = 0;

//26 characters
MenuSelectionStruct Selections[26];

BBSMenu::BBSMenu(ConnectionBase *Conn, BBSRender *Render)
{
    _Conn = Conn;
    _Render = Render;
    _Menu = 0;
    _MenuGroups = (BBSMenu **)malloc(sizeof(BBSMenu *));
    _MenuGroups[0] = 0;
    _SelectionType = SELECTION_ALPHA;
    _ShowHelp = true;

    memset(Selections, 0, sizeof(Selections));

    //default settings
    RenderHeader = 1;
    RenderTimeLeft = 1;
    TextColor = BBSRender::COLOR_CYAN;				//color for most text
    OptionColor = BBSRender::COLOR_RED;			//color for a user selection
    UserDataColor = BBSRender::COLOR_WHITE;		//color of user controlled data
    BoxLineColor = BBSRender::COLOR_GREEN;			//color of lines for the box
    BoxBackgroundColor = BBSRender::COLOR_BLACK;	//color of background of box
    HeaderColor = BBSRender::COLOR_GREY;	    //color of the header when displayed
    Prompt = "Command >>";
    LargeHeader = "main menu";
    Header = "missing";
}

BBSMenu::~BBSMenu()
{
    if(_MenuGroups)
        free(_MenuGroups);
}

size_t BBSMenu::DoAction(size_t ID)
{
    return 0;
}

const BBSMenu::MenuEntryStruct *BBSMenu::GetEntryList()
{
    return _Menu;
}

const uint8_t BBSMenu::RequiredPrivilegeLevel()
{
    return 0;
}

void BBSMenu::Refresh()
{
}

void BBSMenu::RefreshGroup()
{
    size_t Count;

    //go through each menu group and call their refresh
    for(Count = 0; _MenuGroups[Count]; Count++)
        _MenuGroups[Count]->Refresh();
}

void BBSMenu::AddMenu(BBSMenu *NewMenu)
{
    //get a count of how many we have
    size_t Count, CurPos;
    for(Count = 0; _MenuGroups[Count]; Count++) {}

    //add 1 for the null entry and add 1 for the new entry
    _MenuGroups = (BBSMenu **)realloc(_MenuGroups, sizeof(BBSMenu *) * (Count + 2));

    //set the null entry
    _MenuGroups[Count + 1] = 0;

    //figure out where to add, higher privileges first then name order
    for(CurPos = 0; CurPos < Count; CurPos++)
    {
        if(_MenuGroups[CurPos]->RequiredPrivilegeLevel() < NewMenu->RequiredPrivilegeLevel())
            break;

        //if the level is the same check the name
        if(_MenuGroups[CurPos]->RequiredPrivilegeLevel() == NewMenu->RequiredPrivilegeLevel())
        {
            if(strcmp(_MenuGroups[CurPos]->Header, NewMenu->Header) > 0)
                break;
        }
    }
    
    //move everything after current position to the end over a slot
    memmove(&_MenuGroups[CurPos + 1], &_MenuGroups[CurPos], (Count - CurPos) * sizeof(BBSMenu *));

    //add the new entry to the list
    _MenuGroups[CurPos] = NewMenu;
}

void BBSMenu::AddMenu(BBSMenuClassInit MenuInit)
{
    AddMenu(MenuInit(_Conn, _Render));
}

void BBSMenu::HandleMenu()
{
    char Buffer[32];
    size_t ID;
    char *endptr;

    while(1)
    {
        RenderMenu();
        _Conn->Readline((char *)Buffer, sizeof(Buffer) - 1);
        Buffer[sizeof(Buffer) - 1] = 0;

        //if ? then print our help menu
        if((_SelectionType == SELECTION_ALPHA) && _ShowHelp && (Buffer[0] == '?'))
            RenderHelpMenu();
        else if(_SelectionType == SELECTION_NONE)
        {
            if(DoAction(0) == 0)
                break;
        }
        else if(_SelectionType == SELECTION_NUMERIC)
        {
            //convert ID and call DoAction directly if we fully converted the number
            ID = strtol(Buffer, &endptr, 10);
            if(*endptr == 0)
            {
                if(DoAction(ID) == 0)
                    break;
            }
            else
            {
                //if end is the very first entry then it is likely a character, do a few checks
                if(endptr == Buffer)
                {
                    //find the right entry and handle the action
                    ID = (Buffer[0] | 0x20) - 'a';

                    //if the value is valid and we have an entry for it then call the doaction
                    //after altering the ID to indicate it is a character
                    if(ID < 26)
                    {
                        if(DoAction(ID | 0x100) == 0)
                            break;
                    }
                }
            }
        }
        else
        {
            //find the right entry and handle the action
            ID = (Buffer[0] | 0x20) - 'a';

            //if the value is valid and we have an entry for it then call the doaction
            if(ID < 26)
            {
                //if we have menu groups then look through the selection otherwise just call doaction with the value
                if(_MenuGroups && _MenuGroups[0])
                {
                    //if the return is 0 then we need to exit this handler
                    if(Selections[ID].Menu && Selections[ID].EntryDetails && (Selections[ID].Menu->DoAction(Selections[ID].EntryDetails->ID) == 0))
                        break;
                }
                else
                {
                    if(DoAction(ID) == 0)
                        break;
                }
            }
        }

        //make sure we allow them to refresh their input
        RefreshGroup();
    }
}

void BBSMenu::RenderHelpMenu()
{
    size_t CurSelection;
    size_t DisplayCount;
    size_t SelectionStart;
    size_t MaxNameWidth;
    size_t MaxHelpWidth;
    size_t x1, y1, x2, y2;
    uint8_t Buffer[2];

    //25 lines tall, 3 for header, 1 for prompt, 2 for border
    #define MAX_LINES (BBSRender::RENDER_HEIGHT-3-1-2)

    //if we don't have any menus then ignore help request
    if(!_MenuGroups || !_MenuGroups[0])
        return;

    //if not in alpha mode then ignore
    if(_SelectionType != SELECTION_ALPHA)
        return;

    SelectionStart = 0;
    while(1)
    {
        //go through and render our menu as needed
        _Render->Clear();

        _Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, "help menu");

        //count how many lines we need to draw
        DisplayCount = 0;
        MaxNameWidth = 0;
        MaxHelpWidth = 0;
        for(CurSelection = SelectionStart; (CurSelection < 26) && (DisplayCount < MAX_LINES); CurSelection++)
        {
            //if we have an entry then increment our count
            if(Selections[CurSelection].EntryDetails)
            {
                DisplayCount++;

                //see how long this line will be
                if(_Render->GetStringLen(Selections[CurSelection].EntryDetails->Name) > MaxNameWidth)
                    MaxNameWidth = _Render->GetStringLen(Selections[CurSelection].EntryDetails->Name);
                if(_Render->GetStringLen(Selections[CurSelection].EntryDetails->Help) > MaxHelpWidth)
                    MaxHelpWidth = _Render->GetStringLen(Selections[CurSelection].EntryDetails->Help);
            }
        }

        //setup box corners
        y1 = 4;

        //add to our max width the letter and other formatting
        //X <name>  <description>
        x2 = MaxNameWidth + MaxHelpWidth + 8;
        y2 = y1 + DisplayCount + 1;

        //make sure x2 isn't too large
        if(x2 > BBSRender::RENDER_WIDTH)
            x2 = BBSRender::RENDER_WIDTH;

        //center box
        x1 = (BBSRender::RENDER_WIDTH - x2) / 2;
        x2 += x1;

        //now draw the border as needed
        _Render->DrawBox(x1, y1, x2, y2, BBSRender::COLOR_YELLOW, BBSRender::COLOR_BLACK, BBSRender::COLOR_BLACK);

        //fill in each entry
        _Render->SetX(x1 + 1);
        _Render->SetY(5);
        DisplayCount = 0;
        for(CurSelection = SelectionStart; (CurSelection < 26) && (DisplayCount < MAX_LINES); CurSelection++)
        {
            //if we have an entry then increment our count
            if(Selections[CurSelection].EntryDetails)
            {
                //displaying an entry, remove a count from the list
                DisplayCount++;

                _Render->SetX(x1 + 1);
                _Render->PrintF("[%2`c] %3`-*s %1`s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_GREY, OptionColor, UserDataColor, 'A' + CurSelection, MaxNameWidth, Selections[CurSelection].EntryDetails->Name, Selections[CurSelection].EntryDetails->Help);

                //see how long this line will be
                if(_Render->GetStringLen(Selections[CurSelection].EntryDetails->Name) > MaxNameWidth)
                    MaxNameWidth = _Render->GetStringLen(Selections[CurSelection].EntryDetails->Name);
                if(_Render->GetStringLen(Selections[CurSelection].EntryDetails->Help) > MaxHelpWidth)
                    MaxHelpWidth = _Render->GetStringLen(Selections[CurSelection].EntryDetails->Help);
            }
        }

        //update our start location
        SelectionStart = CurSelection + 1;

        //write our prompt
        _Render->SetX(0);
        _Render->OffsetY(1);

        _Render->PrintF("[%2~B%1~]ack ", BBSRender::COLOR_BLACK, TextColor, OptionColor);
        if(SelectionStart < 26)
            _Render->PrintF("[%2~N%1~]ext ", BBSRender::COLOR_BLACK, TextColor, OptionColor);

        _Render->PrintF(">> ", BBSRender::COLOR_BLACK, TextColor, OptionColor);
        _Render->Render(UserDataColor);

        _Conn->Readline((char *)Buffer, 2);
        Buffer[1] = 0;

        //too many entries or B then go back
        if((Buffer[0] == 'B') || (Buffer[0] == 'b') || (SelectionStart >= 26))
            break;

        //N, next screen
        if(((Buffer[0] == 'N') || (Buffer[0] == 'n')) && (SelectionStart < 26))
            continue;

        //unknown character, start back over
        SelectionStart = 0;
    }
}

void BBSMenu::RenderMenu()
{
    size_t CurPrivilege;
    size_t Count, MenuCount;
    size_t BoxCount;
    size_t x1, x2, y1, y2, max_y;
    ssize_t temp;
    size_t LineWidth, MaxStrLen;
    const MenuEntryStruct *CurMenu;
    size_t AddHelpEntry;
    size_t MaxNumLen, CurNum, CurNumLen;
    BBSRender::RENDER_COLOR CurLineColor;
    tm CurTime;
    time_t CurTimeSec;
    size_t ColonLoc;
    string TempLine1, TempLine2;

    if(_SelectionType == SELECTION_ALPHA)
        memset(Selections, 0, sizeof(Selections));

    //go through and render our menu as needed
    _Render->Clear();

    if(RenderHeader)
        _Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, LargeHeader);

    //header is 3 lines so set our minimum start as the 4th line allowing a gap between
    y1 = 4;
    x1 = 0;

    //go through and see what boxes need to be drawn, higher privilege groups sit above
    //in the box listings
    CurPrivilege = 256;
    max_y = y1;
    AddHelpEntry = 0;
    BoxCount = 0;
    for(Count = 0; _MenuGroups[Count]; Count++)
    {
        //if our current privilege level is too low for the entry we are on then skip it
        if(GlobalUser->GetPrivilegeLevel() < _MenuGroups[Count]->RequiredPrivilegeLevel())
            continue;

        //if our privilege level changed from what we were working on and we have things to render then update our box count
        if(_MenuGroups[Count]->RequiredPrivilegeLevel() != CurPrivilege)
        {
            //assign the new level
            CurPrivilege = _MenuGroups[Count]->RequiredPrivilegeLevel();

            //get a box count
            LineWidth = 0;
            for(BoxCount = 0; _MenuGroups[Count + BoxCount] && (_MenuGroups[Count + BoxCount]->RequiredPrivilegeLevel() == CurPrivilege); BoxCount++)
            {
                //figure out the max width for this box
                CurMenu = _MenuGroups[Count + BoxCount]->GetEntryList();
                MaxStrLen = _Render->GetStringLen(_MenuGroups[Count]->Header);
                y2 = y1;
                MaxNumLen = 1;
                for(MenuCount = 0; CurMenu[MenuCount].Name; MenuCount++)
                {
                    //if out of our access then ignore it
                    if(CurMenu[MenuCount].Level > CurPrivilege)
                        break;

                    //if doing a numeric selection find out the largest number
                    if(_SelectionType == SELECTION_NUMERIC)
                    {
                        CurNum = CurMenu[MenuCount].ID;
                        CurNumLen = 0;
                        while(CurNum)
                        {
                            CurNumLen++;
                            CurNum /= 10;
                        }

                        if(CurNumLen > MaxNumLen)
                            MaxNumLen = CurNumLen;
                    }

                    if(_Render->GetStringLen(CurMenu[MenuCount].Name) > MaxStrLen)
                        MaxStrLen = _Render->GetStringLen(CurMenu[MenuCount].Name);
                }

                //adjust our line width to account for the box borders, selection, etc
                //2 for border, 2 for padding around border on inside, 3 for [X], 1 for space, string
                //1 for space between boxes
                if((LineWidth + 9 + MaxStrLen + MaxNumLen - 1) > BBSRender::RENDER_WIDTH)
                {
                    BoxCount--;
                    break;
                }

                //update our width
                LineWidth += 9 + MaxStrLen + MaxNumLen - 1;

                //add a line entry
                y2++;
                if(y2 >= BBSRender::RENDER_HEIGHT)
                    break;
            }
 
            //if we hit the end then add the help entry if requested
            if(!_MenuGroups[Count + BoxCount] && _ShowHelp && (_SelectionType == SELECTION_ALPHA))
                AddHelpEntry = 1;

            //calculate where we will start on our x axis
            x1 = (BBSRender::RENDER_WIDTH - LineWidth) / 2;
        }

        //get the width of this box
        CurMenu = _MenuGroups[Count]->GetEntryList();
        MaxStrLen = _Render->GetStringLen(_MenuGroups[Count]->Header);
        if(AddHelpEntry && (MaxStrLen < _Render->GetStringLen(HelpMenuStr)))
            MaxStrLen = _Render->GetStringLen(HelpMenuStr);

        y2 = y1 + 1;
        MaxNumLen = 1;
        for(MenuCount = 0; CurMenu[MenuCount].Name; MenuCount++)
        {
            //if out of our access then ignore it
            if(CurMenu[MenuCount].Level > CurPrivilege)
                break;

            if(_Render->GetStringLen(CurMenu[MenuCount].Name) > MaxStrLen)
                MaxStrLen = _Render->GetStringLen(CurMenu[MenuCount].Name);

            //if doing a numeric selection find out the largest number
            if(_SelectionType == SELECTION_NUMERIC)
            {
                CurNum = CurMenu[MenuCount].ID;
                CurNumLen = 0;
                while(CurNum)
                {
                    CurNumLen++;
                    CurNum /= 10;
                }

                if(CurNumLen > MaxNumLen)
                    MaxNumLen = CurNumLen;
            }

            //add a line entry
            y2++;
            if(y2 >= BBSRender::RENDER_HEIGHT)
                break;
        }

        //if adding the help entry then add to the end of the box
        if(AddHelpEntry)
            y2++;

        //draw the box
        x2 = x1 + MaxStrLen + 4;
        if(_SelectionType != SELECTION_NONE)    //account for [x]
            x2 += 3;
        if(_SelectionType == SELECTION_NUMERIC) //add in padding
            x2 += MaxNumLen - 1;

        if(y2 > max_y)
            max_y = y2;

        _Render->DrawBox(x1, y1, x2, y2, BoxLineColor, BoxBackgroundColor, BoxBackgroundColor);

        //draw the box header
        temp = 4;
        if(_SelectionType == BBSMenu::SELECTION_ALPHA)
            temp += 3;
        else if(_SelectionType == BBSMenu::SELECTION_NUMERIC)
            temp += 2 + MaxNumLen;
        temp = ((MaxStrLen + temp) - _Render->GetStringLen(_MenuGroups[Count]->Header)) / 2;
        _Render->SetX(x1 + temp + 1);
        _Render->SetY(y1);
        _Render->PrintF("%s", BoxBackgroundColor, BoxLineColor, _MenuGroups[Count]->Header);

        //fill in the box now
        _Render->SetX(x1 + 2);
        _Render->SetY(y1 + 1);
        for(MenuCount = 0; CurMenu[MenuCount].Name; MenuCount++)
        {
            //if out of our access then ignore it
            if(CurMenu[MenuCount].Level > CurPrivilege)
                break;

            //get the color of text for this line
            CurLineColor = UserDataColor;

            //see if there is a colon, if so split the line up
            if(CurMenu[MenuCount].ColorOnColon)
            {
                TempLine1.clear();
                TempLine1.append(CurMenu[MenuCount].Name);
                ColonLoc = TempLine1.find(':');
                if(ColonLoc != string::npos)
                {
                    TempLine2 = TempLine1.substr(ColonLoc + 1);
                    TempLine1 = TempLine1.substr(0, ColonLoc + 1);
                }
            }
            else
                ColonLoc = string::npos;

            if((ColonLoc == string::npos) && (CurMenu[MenuCount].Color != BBSRender::COLOR_NOCHANGE))
                CurLineColor = CurMenu[MenuCount].Color;

            if(_SelectionType == SELECTION_ALPHA)
            {
                //find the option to use for this entry
                for(temp = 0; temp < (ssize_t)_Render->GetStringLen(CurMenu[MenuCount].Name); temp++)
                {
                    if(!Selections[(CurMenu[MenuCount].Name[temp] | 0x20) - 'a'].Menu)
                        break;
                }

                //if we didn't find an entry then look for an empty spot
                if(temp >= (ssize_t)_Render->GetStringLen(CurMenu[MenuCount].Name))
                {
                    for(temp = (sizeof(Selections) / sizeof(MenuSelectionStruct)) - 1; temp >= 0; temp--)
                    {
                        if(!Selections[temp].Menu)
                            break;
                    }
                }
                else
                    temp = (CurMenu[MenuCount].Name[temp] | 0x20) - 'a';

                //fill in the entry
                Selections[temp].Menu = _MenuGroups[Count];
                Selections[temp].EntryDetails = &CurMenu[MenuCount];
                if(ColonLoc != string::npos)
                    _Render->PrintF("[%2`c] %3`s%4`s\n", BoxBackgroundColor, TextColor, OptionColor, CurLineColor, CurMenu[MenuCount].Color, 'A' + temp, TempLine1.c_str(), TempLine2.c_str());
                else
                    _Render->PrintF("[%2`c] %3`s\n", BoxBackgroundColor, TextColor, OptionColor, CurLineColor, 'A' + temp, CurMenu[MenuCount].Name);
            }
            else if(_SelectionType == SELECTION_NUMERIC)
            {
                if(ColonLoc != string::npos)
                    _Render->PrintF("[%2`*d] %3`s%4`s\n", BoxBackgroundColor, TextColor, OptionColor, CurLineColor, CurMenu[MenuCount].Color, MaxNumLen, CurMenu[MenuCount].ID, TempLine1.c_str(), TempLine2.c_str());
                else
                    _Render->PrintF("[%2`*d] %3`s\n", BoxBackgroundColor, TextColor, OptionColor, CurLineColor, MaxNumLen, CurMenu[MenuCount].ID, CurMenu[MenuCount].Name);
            }
            else
            {
                if(ColonLoc != string::npos)
                    _Render->PrintF("%3`s%4`s\n", BoxBackgroundColor, TextColor, OptionColor, CurLineColor, CurMenu[MenuCount].Color, TempLine1.c_str(), TempLine2.c_str());
                else
                    _Render->PrintF("%3`s\n", BoxBackgroundColor, TextColor, OptionColor, CurLineColor, CurMenu[MenuCount].Name);
            }
        }

        //if we have a help entry then add it
        if(AddHelpEntry)
        {
            AddHelpEntry = 0;
            _Render->PrintF("[%2`c] %3`s\n", BoxBackgroundColor, TextColor, OptionColor, UserDataColor, '?', HelpMenuStr);
        }

        BoxCount--;
        x1 = x2 + 1;

        //if we ran out of boxes or we hit the end then update x/y
        if(!BoxCount)
            y1 = y2 + 1;
    }

    //display the current time
    _Render->SetX(0);
    _Render->SetY(max_y + 1);
    CurTimeSec = time(0);
    gmtime_r(&CurTimeSec, &CurTime);
    _Render->PrintF("Current Time: %2~%04d/%02d/%02d %02d:%02d:%02d", BBSRender::COLOR_BLACK, TextColor, OptionColor, UserDataColor, CurTime.tm_year + 1900, CurTime.tm_mon + 1, CurTime.tm_mday, CurTime.tm_hour, CurTime.tm_min, CurTime.tm_sec);

    //render the prompt
    _Render->SetX(0);
    _Render->SetY(max_y + 3);
    _Render->PrintF(Prompt, BBSRender::COLOR_BLACK, TextColor, OptionColor, UserDataColor);
    _Render->OffsetX(1);

    _Render->Render(UserDataColor);
}

void BBSMenu::GetAmountAndMarker(size_t Amount, char *Marker, size_t *Result)
{
    double dAmount = Amount;
    char MarkerList[] = " KMGT";
    size_t i;

    //find our size
    for(i = 0; i < sizeof(MarkerList); i++)
    {
        *Marker = MarkerList[i];
        if((dAmount < 1000.0) || ((i+1) >= sizeof(MarkerList)))
            break;

        dAmount /= 1000.0;
    }

    //return the amounts
    Result[0] = (size_t)dAmount;
    dAmount -= Result[0];
    Result[1] = (size_t)(dAmount * 100.0);
}

size_t BBSMenu::GetValue(const char *Name, void *Data, size_t DataLen)
{
    return 0;
}

void BBSMenu::SetValue(const char *Name, void *Data, size_t DataLen)
{
}

void RegisterBBSMenuPlugin(BBSMenuPluginInit Init, const char *Name)
{
    MenuPluginStruct *CurEntry;

    //cycle through our plugin list and see if anything matches, if so replace the init otherwise create a new entry
    for(CurEntry = MenuPluginEntries; CurEntry; CurEntry = CurEntry->Next)
    {
        if(strcasecmp(Name, CurEntry->Name) == 0)
        {
            CurEntry->Init = Init;
            return;
        }
    }

    //no match
    CurEntry = (MenuPluginStruct *)malloc(sizeof(MenuPluginStruct));
    CurEntry->Name = Name;
    CurEntry->Init = Init;
    CurEntry->Next = MenuPluginEntries;
    MenuPluginEntries = CurEntry;
}

BBSMenuPluginInit GetBBSMenuPlugin(const char *Name)
{
    MenuPluginStruct *CurEntry;

    //find a plugin that matches the request
    for(CurEntry = MenuPluginEntries; CurEntry; CurEntry = CurEntry->Next)
    {
        if(strcasecmp(Name, CurEntry->Name) == 0)
            return CurEntry->Init;
    }

    return 0;
}

void RegisterBBSMenu(BBSMenuClassInit Init)
{
    if(!MainMenu)
    {
        puts("Error: MainMenu not initialized\n");
        fflush(stdout);
    }
    else
        MainMenu->AddMenu(Init);
}

__attribute__((constructor)) static void init()
{
}