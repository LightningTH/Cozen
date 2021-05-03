#include <sys/random.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include "BBSUser.h"

BBSUser *GlobalUser = 0;

#define USERFILE "data/users.dat"

BBSUser::BBSUser(BBSUser *Orig)
{
    //copy from an existing entry
    memcpy(&_UserData, &Orig->_UserData, sizeof(_UserData));
    _UserFileOffset = Orig->_UserFileOffset;
    _PreviousLoginTime = Orig->_PreviousLoginTime;
    _Username.append(Orig->_Username);

    _CanFind = true;
    _find_fd = -1;
}

BBSUser::BBSUser()
{
    //empty user, indicate we can search
    _CanFind = true;
    _find_fd = -1;
    memset(&_UserData, 0, sizeof(_UserData));
    _UserFileOffset = 0;
}

BBSUser::BBSUser(const char *Username)
{
    std::string SearchName;

    //setup what name to search for
    SearchName = Username;

    //make sure the name isn't too long for the file
    if(strlen(Username) > 256)
        SearchName.resize(256);

    //make sure the name doesn't have spaces at the end
    while((SearchName[SearchName.length() - 1] == ' ') || (SearchName[SearchName.length() - 1] == '\t'))
        SearchName.resize(SearchName.length() - 1);

    //make sure it doesn't start with a character less than a space
    while((SearchName[0] == ' ') || (SearchName[0] == '\t'))
        SearchName = SearchName.substr(1, SearchName.length() - 1);

    //try to find an exact match
    _CanFind = true;
    _find_fd = -1;
    if(!FindExact(&SearchName))
    {
        //couldn't find a match, must be a new user
        memset(&_UserData, 0, sizeof(_UserData));
        _UserFileOffset = 0;
        _UserData.ShowWelcome = 1;
        _UserData.UsernameLen = strlen(Username) - 1;
        _PreviousLoginTime = time(0);
        _Username = Username;
    }
    _CanFind = false;
}

BBSUser::~BBSUser()
{
    if(_find_fd >= 0)
        close(_find_fd);
}

bool BBSUser::FindExact(std::string *Name)
{
    //only allow searching when this class is initialized for searching
    if(!_CanFind)
        return false;

    //call find and see if the name matches, if not then keep calling findnext
    if(!Find(Name))
    {
        FindDone();
        return false;
    }

    //see if we can find a match
    do
    {
        //found something, check for a match
        if(!strcasecmp(Name->c_str(), _Username.c_str()))
        {
            FindDone();
            return true;
        }
    }
    while (FindNext());

    //never found the match, fail
    FindDone();
    return false;
}

bool BBSUser::FindExact(size_t ID)
{
    //only allow searching when this class is initialized for searching
    if(!_CanFind)
        return false;

    //call find and see if the ID matches, if not then keep calling findnext
    if(!Find(ID))
    {
        FindDone();
        return false;
    }

    //see if we can find a match
    do
    {
        //found something, check for a match
        if(ID == _UserData.ID)
        {
            FindDone();
            return true;
        }
    }
    while (FindNext());

    //never found the match, fail
    FindDone();
    return false;
}

void BBSUser::FindDone()
{
    //only allow searching when this class is initialized for searching
    if(!_CanFind)
        return;

    //if fd is valid then close it and set invalid
    if(_find_fd != -1)
        close(_find_fd);

    _find_fd = -1;
}

bool BBSUser::Find(std::string *MatchName)
{
    return Find(MatchName->c_str());
}

bool BBSUser::Find(const char *MatchName)
{
    //only allow searching when this class is initialized for searching
    if(!_CanFind)
        return false;

    //if a find was already running then move to the beginning
    //otherwise open the file
    if(_find_fd < 0)
    {
        //open up the user file and find our user if possible
        _find_fd = open(USERFILE, O_RDONLY);
        if(_find_fd < 0)
            return false;
    }

    //move to the beginning skipping the first few bytes for the last record storage offset
    lseek(_find_fd, sizeof(size_t), SEEK_SET);

    //setup the name we are matching on
    _FindMatchStr = MatchName;
    _FindMatchID = UINT64_MAX;

    //make sure the name doesn't have spaces at the end
    while((_FindMatchStr[_FindMatchStr.length() - 1] == ' ') || (_FindMatchStr[_FindMatchStr.length() - 1] == '\t'))
        _FindMatchStr.resize(_FindMatchStr.length() - 1);

    //make sure it doesn't start with a character less than a space
    while((_FindMatchStr[0] == ' ') || (_FindMatchStr[0] == '\t'))
        _FindMatchStr = _FindMatchStr.substr(1, _FindMatchStr.length() - 1);

    return FindNext();
}

bool BBSUser::Find(size_t ID)
{
    //only allow searching when this class is initialized for searching
    if(!_CanFind)
        return false;

    //if a find was already running then move to the beginning
    //otherwise open the file
    if(_find_fd < 0)
    {
        //open up the user file and find our user if possible
        _find_fd = open(USERFILE, O_RDONLY);
        if(_find_fd < 0)
            return false;
    }

    //move to the beginning skipping the first few bytes for the last record storage offset
    lseek(_find_fd, sizeof(size_t), SEEK_SET);

    //setup the name we are matching on
    _FindMatchStr.erase();
    _FindMatchID = ID;
    return FindNext();
}

bool BBSUser::FindNext()
{
    char FileUsername[257];
    UserDataStruct CurUser;
    size_t CurFilePos;

    //only allow searching when this class is initialized for searching
    if(!_CanFind)
        return false;

    //if no valid file is open then fail
    if(_find_fd < 0)
        return false;

    //got the file open, start reading users
    while(1)
    {
        //try to read a user record
        CurFilePos = lseek(_find_fd, 0, SEEK_CUR);
        if(read(_find_fd, &CurUser, sizeof(CurUser)) != sizeof(CurUser))
            break;

        //try to read the username
        if(read(_find_fd, FileUsername, CurUser.UsernameLen + 1) != (CurUser.UsernameLen + 1))
            break;

        //got a user entry, see if it matches our user
        FileUsername[CurUser.UsernameLen + 1] = 0;
        if((_FindMatchStr.length() && strcasestr(FileUsername, _FindMatchStr.c_str())) ||
            ((_FindMatchID != UINT64_MAX) && (CurUser.ID == _FindMatchID)))
        {
            //found a user that matches, store their data
            memcpy(&_UserData, &CurUser, sizeof(_UserData));
            _Username = FileUsername;
            _UserFileOffset = CurFilePos;
            _PreviousLoginTime = _UserData.LastLoginTime;
            return true;
        }
    };

    //failed to find a match
    return false;
}

BBSUser::CHANGE_ENUM BBSUser::ChangeAllowed()
{
    //user is sysop and is altering someone else
    if(_CanFind && (GlobalUser->GetPrivilegeLevel() == LEVEL_SYSOP))
        return IS_SYSOP;

    //user can alter their own user
    if(this == GlobalUser)
        return IS_OWNER;

    return NOT_ALLOWED;
}

int BBSUser::ValidatePassword(const char *Password)
{
    int Ret;
    uint8_t CurHash[16];

    HashBase *Hash = GetHash(MAGIC_VAL('M', 'D', '5', 0))();
    Hash->AddData(_UserData.PasswordSalt, sizeof(_UserData.PasswordSalt));
    Hash->AddData((void *)Password, strlen(Password));
    memcpy(CurHash, Hash->GetHash(), sizeof(CurHash));
    Hash->AddData(CurHash, sizeof(CurHash));
    memcpy(CurHash, Hash->GetHash(), sizeof(CurHash));
    Hash->AddData(CurHash, sizeof(CurHash));
    memcpy(CurHash, Hash->GetHash(), sizeof(CurHash));
    Hash->AddData(CurHash, sizeof(CurHash));

    Ret = Hash->CompareHash(_UserData.PasswordHash);
    delete Hash;

    //if the password validated then it was a successful login
    if(Ret)
    {
        GlobalStats->IncrementTotalLogins();
        _UserData.LastLoginTime = time(0);
        Save();
    }

    return Ret;
}

size_t BBSUser::GetID()
{
    return _UserData.ID;
}

int BBSUser::GetPrivilegeLevel()
{
    return _UserData.PrivilegeLevel;
}

int BBSUser::IsNewUser()
{
    //if our ID is set to max then it is a new user
    return !_CanFind && (_UserFileOffset == 0);
}

bool BBSUser::IsLocked()
{
    return (_UserData.Locked == 1);
}

bool BBSUser::SetLocked(bool Lock)
{
    if(ChangeAllowed() != IS_SYSOP)
        return false;

    _UserData.Locked = Lock;
    Save();
    return true;
}

time_t BBSUser::GetPreviousLoginTime()
{
    return _PreviousLoginTime;
}

int BBSUser::SetPassword(const char *CurPassword, const char *NewPassword)
{
    int Ret;
    uint8_t CurHash[16];
    HashBase *Hash = GetHash(MAGIC_VAL('M', 'D', '5', 0))();

    //see if any change is allowed
    if(!IsNewUser() && !ChangeAllowed())
    {
        delete Hash;
        return 0;
    }

    //before the password can be set it must be validated unless it is admin changing
    //someone's password
    if(!IsNewUser() && (GlobalUser == this))
    {
        Hash->AddData(_UserData.PasswordSalt, sizeof(_UserData.PasswordSalt));
        Hash->AddData((void *)CurPassword, strlen(CurPassword));
        memcpy(CurHash, Hash->GetHash(), sizeof(CurHash));
        Hash->AddData(CurHash, sizeof(CurHash));
        memcpy(CurHash, Hash->GetHash(), sizeof(CurHash));
        Hash->AddData(CurHash, sizeof(CurHash));
        memcpy(CurHash, Hash->GetHash(), sizeof(CurHash));
        Hash->AddData(CurHash, sizeof(CurHash));

        Ret = Hash->CompareHash(_UserData.PasswordHash);

        //if we failed then abort
        if(!Ret)
        {
            delete Hash;
            return 0;
        }
    }

    //all good, get a new seed value
    getrandom(_UserData.PasswordSalt, sizeof(_UserData.PasswordSalt), 0);

    //generate a new hash
    Hash->Reset();
    Hash->AddData(_UserData.PasswordSalt, sizeof(_UserData.PasswordSalt));
    Hash->AddData((void *)NewPassword, strlen(NewPassword));
    memcpy(CurHash, Hash->GetHash(), sizeof(CurHash));
    Hash->AddData(CurHash, sizeof(CurHash));
    memcpy(CurHash, Hash->GetHash(), sizeof(CurHash));
    Hash->AddData(CurHash, sizeof(CurHash));
    memcpy(CurHash, Hash->GetHash(), sizeof(CurHash));
    Hash->AddData(CurHash, sizeof(CurHash));

    //get the final hash
    memcpy(_UserData.PasswordHash, Hash->GetHash(), sizeof(_UserData.PasswordHash));
    delete Hash;

    //all good
    Save();
    return 1;
}

void BBSUser::SetPrivilegeLevel(int Level)
{
    if(ChangeAllowed() != IS_SYSOP)
        return;

    _UserData.PrivilegeLevel = Level;
    Save();
}

int BBSUser::GetTrafficStats(BBSGlobalStats::TrafficStatsStruct *Stats)
{
    memcpy(Stats, &_UserData.Traffic, sizeof(BBSGlobalStats::TrafficStatsStruct));
    return 1;
}

void BBSUser::AddDownloadTrafficStats(size_t DownloadSize)
{
    if(!ChangeAllowed())
        return;

    //update user
    _UserData.Traffic.DownloadCount++;
    _UserData.Traffic.DownloadSize += DownloadSize;
    Save();

    //update global
    GlobalStats->AddDownloadTrafficStats(DownloadSize);
}

void BBSUser::AddUploadTrafficStats(size_t UploadSize)
{
    if(!ChangeAllowed())
        return;

    //update user
    _UserData.Traffic.UploadCount++;
    _UserData.Traffic.UploadSize += UploadSize;
    Save();

    //update global
    GlobalStats->AddUploadTrafficStats(UploadSize);
}

void BBSUser::ResetStats()
{
    if(ChangeAllowed() != IS_SYSOP)
        return;

    //update global stats
    GlobalStats->RemoveDownloadTrafficStats(_UserData.Traffic.DownloadSize, _UserData.Traffic.DownloadCount);
    GlobalStats->RemoveUploadTrafficStats(_UserData.Traffic.UploadSize, _UserData.Traffic.UploadCount);

    //update user
    _UserData.Traffic.DownloadCount = 0;
    _UserData.Traffic.DownloadSize = 0;
    _UserData.Traffic.UploadCount = 0;
    _UserData.Traffic.UploadSize = 0;
    Save();
}

BBSUser::ColorChoice BBSUser::GetColorChoice()
{
    return (BBSUser::ColorChoice)_UserData.Color;
}

void BBSUser::SetColorChoice(ColorChoice Color)
{
    if(!ChangeAllowed())
        return;

    _UserData.Color = Color & 0x3;
    Save();
}

bool BBSUser::GetShowWelcome()
{
    return _UserData.ShowWelcome;
}

void BBSUser::SetShowWelcome(bool Display)
{
    if(!ChangeAllowed())
        return;

    _UserData.ShowWelcome = (int)Display;
    Save();
}

const char *BBSUser::GetName()
{
    return _Username.c_str();
}

void BBSUser::Save()
{
    int fd;
    int fd_lock;
    timespec delay;
    size_t LastRecordPos = 0;
    UserDataStruct LastUser;
    bool NewUser = false;

    //attempt to get a lock file first
    while(1)
    {
        fd_lock = open(USERFILE ".lock", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

        //if we got lock then exit otherwise wait a short moment
        if(fd_lock >= 0)
            break;

        delay.tv_sec = 0;
        delay.tv_nsec = 1000;
        nanosleep(&delay, 0);
    }

    //we got the lock, open the user file
    fd = open(USERFILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(fd < 0)
    {
        //have an issue, abort
        unlink(USERFILE ".lock");
        close(fd_lock);
    }

    if(_UserFileOffset == 0)
    {
        //read where the last record is
        if(read(fd, &LastRecordPos, sizeof(LastRecordPos)) != sizeof(LastRecordPos))
        {
            //must be the first entry, write the first entry and set them as sysop
            _UserFileOffset = write(fd, &LastRecordPos, sizeof(LastRecordPos));

            //first entry so set id and privilege
            _UserData.PrivilegeLevel = UINT8_MAX;
            _UserData.ID = 1;
        }
        else
        {
            //no file offset yet and new user so get the last ID as we need a new user id
            lseek(fd, LastRecordPos, SEEK_SET);
            read(fd, &LastUser, sizeof(LastUser));
            _UserData.ID = LastUser.ID + 1;
        }

        //move to the end and set our position
        _UserFileOffset = lseek(fd, 0, SEEK_END);

        //move back to the beginning and write the new offset for the last record
        lseek(fd, 0, SEEK_SET);
        write(fd, &_UserFileOffset, sizeof(_UserFileOffset));

        //set the creation time
        _UserData.CreationTime = time(0);
        _UserData.LastLoginTime = _UserData.CreationTime;
        _PreviousLoginTime = 0;
        NewUser = true;
    }

    //make sure we are at the right location for this user
    lseek(fd, _UserFileOffset, SEEK_SET);

    if(!NewUser)
    {
        //sysop may have altered our record, read it first to confirm
        read(fd, &LastUser, sizeof(LastUser));
        lseek(fd, _UserFileOffset, SEEK_SET);

        //if the sysop flag is set then we have a few things to alter locally
        if(LastUser.SysopAltered)
        {
            //if the stats are 0 then copy them over if our new stats don't indicate
            //a single file due to completing an upload or download
            if((LastUser.Traffic.DownloadCount == 0) && (LastUser.Traffic.UploadCount == 0) &&
                ((_UserData.Traffic.DownloadCount > 1) || (_UserData.Traffic.UploadCount > 1)))
                _UserData.Traffic = LastUser.Traffic;

            //if the privilege level or locked changed copy it
            _UserData.PrivilegeLevel = LastUser.PrivilegeLevel;
            _UserData.Locked = LastUser.Locked;

            //we do not copy password hashes as the only way it should change is if a user
            //can't login so leave whatever password they are aware of
        }
    }

    //write out the structure
    write(fd, &_UserData, sizeof(_UserData));
    if(NewUser)
    {
        //write the username
        write(fd, _Username.c_str(), _Username.length());

        //update global as they didn't login with a password first time through
        GlobalStats->IncrementTotalLogins();
        GlobalStats->IncrementTotalUsers();
    }

    //close everything
    close(fd);
    unlink(USERFILE ".lock");
    close(fd_lock);

    //if our locked flag got set then kill our connection
    if((this == GlobalUser) && _UserData.Locked)
    {
        close(0);
        close(1);
        close(2);
        _exit(0);
    }
}
