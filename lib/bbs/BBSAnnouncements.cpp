#include <sys/random.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <BBSUser.h>
#include <time.h>
#include "BBSAnnouncements.h"

#define ANNOUNCEMENTSFILE "data/announcements.dat"

BBSAnnouncements::BBSAnnouncements() :
    BBSMenu(0, 0)
{
    //we have no connection or render, just get our announcements
    _LastAnnouncementTime = 0;
    Read();
}

BBSAnnouncements::BBSAnnouncements(ConnectionBase *Conn, BBSRender *Render) :
    BBSMenu(Conn, Render)
{
    InitAnnouncements(false);
}

BBSAnnouncements::BBSAnnouncements(ConnectionBase *Conn, BBSRender *Render, bool DoSysOp) :
    BBSMenu(Conn, Render)
{
    InitAnnouncements(DoSysOp);
}

void BBSAnnouncements::InitAnnouncements(bool DoSysOp)
{
    _InSysopMenu = DoSysOp;
    _LastAnnouncementTime = 0;

    LargeHeader = "ANNOUNCEMENTS";
    Header = "ANNOUNCEMENTS";

    //if sysop then we want numeric selection for deletion
    if(DoSysOp)
        _SelectionType = SELECTION_NUMERIC;
    else
        _SelectionType = SELECTION_NONE;


    _ShowHelp = false;

    HeaderColor = BBSRender::COLOR_GREEN;
    BoxLineColor = BBSRender::COLOR_BLUE;
    OptionColor = BBSRender::COLOR_GREEN;

    //get our announcements
    Read();

    //generate a new menu
    GenerateMenu();

    if(DoSysOp)
        Prompt = "%3~[%2~B%3~]%1~ack %3~[%2~A%3~]%1~dd New Entry or number to delete >>";
    else
        Prompt = "Press enter to return";

    //add menu entries
    AddMenu(this);
}

BBSAnnouncements::~BBSAnnouncements()
{
    for(auto & entry : _Announcements)
        free(entry);
}

size_t BBSAnnouncements::DoAction(size_t ID)
{
    char Buffer[100];

    //if not sysop menu or user isn't sysop then don't allow
    if(!_InSysopMenu || (GlobalUser->GetPrivilegeLevel() != BBSUser::LEVEL_SYSOP))
        return 0;

    switch(ID)
    {
        case CharActionToInt('b'):  //back
            return 0;

        case CharActionToInt('a'):  //add new
            _Render->Clear();
            _Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, "NEW ANNOUNCEMENT");
            _Render->SetX(0);
            _Render->OffsetY(1);
            _Render->PrintF("New Announcement: ", BBSRender::COLOR_BLACK, TextColor);
            _Render->Render(UserDataColor);

            memset(Buffer, 0, sizeof(Buffer));
            _Conn->Readline(Buffer, sizeof(Buffer) - 1);

            if(strlen(Buffer))
                AddAnnouncement(Buffer);
            break;

        default:
            //if an ID and between our ID range then we have an entry to delete
            if((ID != 0) && (ID <= _Announcements.size()))
                DeleteAnnouncement(ID - 1);
    };
    
    return 1;
}

time_t BBSAnnouncements::LastAnnouncementTime()
{
    return _LastAnnouncementTime;
}

void BBSAnnouncements::AddAnnouncement(char *Announcement)
{
    AnnouncementStruct *NewEntry;
    NewEntry = (AnnouncementStruct *)malloc(sizeof(AnnouncementStruct) + strlen(Announcement) + 1);
    NewEntry->Timestamp = time(0);
    memcpy(NewEntry->Announcement, Announcement, strlen(Announcement) + 1);
    _Announcements.push_back(NewEntry);

    Write();
    GenerateMenu();
}

void BBSAnnouncements::DeleteAnnouncement(size_t Index)
{
    AnnouncementStruct *Entry;
    if(Index < _Announcements.size())
    {
        Entry = _Announcements[Index];
        _Announcements.erase(_Announcements.cbegin() + Index);
        free(Entry);
        Write();
        GenerateMenu();
    }
}

void BBSAnnouncements::LockFile()
{
    int fd;
    timespec delay;

    //attempt to get a lock file first
    while(1)
    {
        fd = open(ANNOUNCEMENTSFILE ".lock", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

        //if we got lock then exit otherwise wait a short moment
        if(fd >= 0)
            break;

        delay.tv_sec = 0;
        delay.tv_nsec = 1000;
        nanosleep(&delay, 0);
    }

    _lockfd = fd;
}

void BBSAnnouncements::UnlockFile()
{
    if(_lockfd != -1)
    {
        unlink(ANNOUNCEMENTSFILE ".lock");
        close(_lockfd);
        _lockfd = -1;
    }
}

void BBSAnnouncements::GenerateMenu()
{
	size_t count;
    string *TimeInfo;
    char Buffer[100];
    tm CurTime;

	//erase the menu we have
	if(_Menu)
	{
        //free any memory if need be
        for(count = 0; _Menu[count].Name; count++)
        {
            if(_Menu[count].Data)
                delete (string *)_Menu[count].Data;
        }

		//erase the menu itself
		free(_Menu);
		_Menu = 0;
	}

    //allocate our menu, sysop gets numbers while
    //normal gets dates
    if(_InSysopMenu)
        _Menu = (MenuEntryStruct *)calloc(_Announcements.size() + 1, sizeof(MenuEntryStruct));
    else
    {
        //get the number of lines needed
        count = _Announcements.size() * 2;
        _Menu = (MenuEntryStruct *)calloc(count + 1, sizeof(MenuEntryStruct));
    }

    //start adding entries
    count = 0;
    for(auto & entry : _Announcements)
    {
        //if not sysop then add a timestamp entry if time is set
        if(!_InSysopMenu)
        {
            //add a time entry
            TimeInfo = new string();
            gmtime_r(&(entry->Timestamp), &CurTime);
            snprintf(Buffer, sizeof(Buffer) - 1, "%04d/%02d/%02d %02d:%02d:%02d", CurTime.tm_year + 1900, CurTime.tm_mon + 1, CurTime.tm_mday, CurTime.tm_hour, CurTime.tm_min, CurTime.tm_sec);
            TimeInfo->append(Buffer);
            _Menu[count].ID = count + 1;
            _Menu[count].Data = TimeInfo;
            _Menu[count].Name = TimeInfo->c_str();
            if(GlobalUser->GetPreviousLoginTime() < entry->Timestamp)
                _Menu[count].Color = BBSRender::COLOR_GREEN;
            else
                _Menu[count].Color = BBSRender::COLOR_GREY;
            count++;
        }

        //add menu entry of the announcement
        _Menu[count].ID = count + 1;
        _Menu[count].Name = entry->Announcement;
        if(GlobalUser->GetPreviousLoginTime() < entry->Timestamp)
            _Menu[count].Color = BBSRender::COLOR_GREEN;

        count++;
    }
}

void BBSAnnouncements::Read()
{
    int fd;
    AnnouncementFileStruct CurEntry;
    AnnouncementStruct *NewAnnouncement;

    //open up the global data file and get info
    fd = open(ANNOUNCEMENTSFILE, O_RDONLY);

    //if file failed to open, return
    if(fd < 0)
        return;

    while(1)
    {
        if(read(fd, &CurEntry, sizeof(CurEntry)) != sizeof(CurEntry))
            break;

        //got a record, allocate an entry
        NewAnnouncement = (AnnouncementStruct *)malloc(sizeof(AnnouncementStruct) + CurEntry.Len + 1);
        NewAnnouncement->Timestamp = CurEntry.Timestamp;
    
        //get the string associated with it
        if(read(fd, NewAnnouncement->Announcement, CurEntry.Len) != CurEntry.Len)
            break;

        NewAnnouncement->Announcement[CurEntry.Len] = 0;        
        _Announcements.push_back(NewAnnouncement);

        //update our announcement time if need be
        if(NewAnnouncement->Timestamp > _LastAnnouncementTime)
            _LastAnnouncementTime = NewAnnouncement->Timestamp;
    };

    close(fd);
}

void BBSAnnouncements::Write()
{
    int fd;
    AnnouncementFileStruct CurEntry;

    LockFile();

    //open the announcement file
    fd = open(ANNOUNCEMENTSFILE, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if(fd < 0)
        return;

    //write all entries out
    for(auto & entry : _Announcements)
    {
        //record each entry to the common structure then write it out
        CurEntry.Timestamp = entry->Timestamp;
        CurEntry.Len = strlen(entry->Announcement);
        write(fd, &CurEntry, sizeof(CurEntry));
        write(fd, entry->Announcement, CurEntry.Len);
    }

    //close everything
    close(fd);
    UnlockFile();
}

size_t BBSAnnouncements::GetValue(const char *Name, void *Data, size_t DataLen)
{
    //if lasttime then return last announcement time
    if(strcasecmp(Name, "LastTime") == 0)
    {
        if((DataLen < sizeof(_LastAnnouncementTime)) || !Data)
            return 0;

        memcpy(Data, &_LastAnnouncementTime, sizeof(_LastAnnouncementTime));
        return sizeof(_LastAnnouncementTime);
    }

    //default
    return 0;
}

BBSMenu *CreateAnnouncementMenu(BBSMENU_PlUGIN_TYPE Type, ConnectionBase *Conn, BBSRender *Render, void *Value)
{
    switch(Type)
    {
        case BBSMENU_PlUGIN_TYPE::TYPE_NORMAL_WITHFLAG:
            return new BBSAnnouncements(Conn, Render, true);

        case BBSMENU_PlUGIN_TYPE::TYPE_NO_PARAMS:
            return new BBSAnnouncements();

        default:
            return new BBSAnnouncements(Conn, Render);
    }
}

__attribute__((constructor)) static void init()
{
    RegisterBBSMenuPlugin(CreateAnnouncementMenu, "announcements");
}