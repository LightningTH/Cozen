#include <string>
#include <string.h>
#include "BBSStats.h"
#include <BBSUser.h>
#include <BBSGlobalStats.h>

using namespace std;

BBSStats::BBSStats(ConnectionBase *Conn, BBSRender *Render) :
    BBSMenu(Conn, Render)
{
    LargeHeader = "STATS";
    Header = "USER";

    _StatMenu = 0;
    _SelectionType = SELECTION_NONE;
    _ShowHelp = false;

    HeaderColor = BBSRender::COLOR_WHITE;
    BoxLineColor = BBSRender::COLOR_RED;
    OptionColor = BBSRender::COLOR_GREEN;

    //generate a new menu
    GenerateMenu(false);
    _Menu = _StatMenu;
    Prompt = "Press enter to return";

    //add menu entries
    AddMenu(this);
    _GlobalStatMenu = new BBSStats(Conn, Render, true);
    AddMenu(_GlobalStatMenu);
}

BBSStats::BBSStats(ConnectionBase *Conn, BBSRender *Render, bool ShowGlobal) :
    BBSMenu(Conn, Render)
{
    Header = "GLOBAL";

    _StatMenu = 0;
    _GlobalStatMenu = 0;

    //generate a new menu
    GenerateMenu(true);
    _Menu = _StatMenu;
}

BBSStats::~BBSStats()
{
    size_t i;
    if(_StatMenu)
    {
        //delete our string objects
        for(i = 0; _StatMenu[i].Name; i++)
        {
            if(_StatMenu[i].Data)
                delete (string *)_StatMenu[i].Data;
        }

        //free the buffer
        free(_StatMenu);
    }

    //make sure our global menu is cleaned up
    if(_GlobalStatMenu)
        delete _GlobalStatMenu;
}

void BBSStats::GenerateMenu(bool ShowGlobal)
{
    size_t i;
    size_t AmountN[2];
    char AmountMarker;
    string *NewStr;
    char Buffer[100];
    BBSGlobalStats::GlobalStatsStruct Stats;
    size_t EntryCount;
    size_t CurEntry;

    if(ShowGlobal)
        GlobalStats->GetGlobalStats(&Stats);
    else
        GlobalUser->GetTrafficStats(&Stats.Traffic);

    if(!_StatMenu)
    {
        if(ShowGlobal)
            EntryCount = 7;
        else
            EntryCount = 4;

        _StatMenu = (BBSMenu::MenuEntryStruct *)calloc(EntryCount + 1, sizeof(BBSMenu::MenuEntryStruct));
        for(i = 0; i < EntryCount; i++)
        {
            _StatMenu[i].ID = i+1;
            _StatMenu[i].Color = BBSRender::COLOR_YELLOW;
        }

        CurEntry = 0;

        //if global then add it's entries first
        if(ShowGlobal)
        {
            //fill in our info about file stats
            NewStr = new string();
            NewStr->append("Total Users:          ");
            snprintf(Buffer, sizeof(Buffer) - 1, "%6ld", Stats.TotalUsers);
            NewStr->append(Buffer);
            _StatMenu[CurEntry].Name = NewStr->c_str();
            _StatMenu[CurEntry].Data = NewStr;
            CurEntry++;

            NewStr = new string();
            NewStr->append("Total Logins:         ");
            snprintf(Buffer, sizeof(Buffer) - 1, "%6ld", Stats.TotalLogins);
            NewStr->append(Buffer);
            _StatMenu[CurEntry].Name = NewStr->c_str();
            _StatMenu[CurEntry].Data = NewStr;
            CurEntry++;

            NewStr = new string();
            NewStr->append("Total Messages:     ");
            snprintf(Buffer,sizeof(Buffer) - 1,  "%8ld", Stats.TotalMessages);
            NewStr->append(Buffer);
            _StatMenu[CurEntry].Name = NewStr->c_str();
            _StatMenu[CurEntry].Data = NewStr;
            CurEntry++;
        }

        //fill in our info about file stats
        NewStr = new string();
        NewStr->append("Files Downloaded:     ");
        snprintf(Buffer, sizeof(Buffer) - 1, "%6d", Stats.Traffic.DownloadCount);
        NewStr->append(Buffer);
        _StatMenu[CurEntry].Name = NewStr->c_str();
        _StatMenu[CurEntry].Data = NewStr;
        CurEntry++;

        NewStr = new string();
        NewStr->append("Downloaded Amount: ");
        GetAmountAndMarker(Stats.Traffic.DownloadSize, &AmountMarker, AmountN);
        snprintf(Buffer, sizeof(Buffer) - 1, "%6ld.%02ld %cb", AmountN[0], AmountN[1], AmountMarker);
        NewStr->append(Buffer);
        _StatMenu[CurEntry].Name = NewStr->c_str();
        _StatMenu[CurEntry].Data = NewStr;
        CurEntry++;

        NewStr = new string();
        NewStr->append("Files Uploaded:       ");
        snprintf(Buffer, sizeof(Buffer) - 1, "%6d", Stats.Traffic.UploadCount);
        NewStr->append(Buffer);
        _StatMenu[CurEntry].Name = NewStr->c_str();
        _StatMenu[CurEntry].Data = NewStr;
        CurEntry++;

        NewStr = new string();
        NewStr->append("Upload Amount:     ");
        GetAmountAndMarker(Stats.Traffic.UploadSize, &AmountMarker, AmountN);
        snprintf(Buffer, sizeof(Buffer) - 1, "%6ld.%02ld %cb", AmountN[0], AmountN[1], AmountMarker);
        NewStr->append(Buffer);
        _StatMenu[CurEntry].Name = NewStr->c_str();
        _StatMenu[CurEntry].Data = NewStr;
        CurEntry++;
    }
}

size_t BBSStats::DoAction(size_t ID)
{
    return 0;
}