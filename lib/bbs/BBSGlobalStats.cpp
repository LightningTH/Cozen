#include <sys/random.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include "BBSGlobalStats.h"

BBSGlobalStats *GlobalStats = 0;

#define GLOBALFILE "data/global.dat"

BBSGlobalStats::BBSGlobalStats()
{
    _lockfd = -1;
    _fd = -1;

    memset(&_GlobalData, 0, sizeof(_GlobalData));
    Read();
}

int BBSGlobalStats::GetGlobalStats(GlobalStatsStruct *Stats)
{
    Read();
    memcpy(Stats, &_GlobalData, sizeof(GlobalStatsStruct));
    return 1;
}

void BBSGlobalStats::AddDownloadTrafficStats(size_t DownloadSize)
{
    //lock our file and read the latest info
    LockFile();
    Read(true);

    //now add what was provided
    _GlobalData.Traffic.DownloadCount++;
    _GlobalData.Traffic.DownloadSize += DownloadSize;

    //write and unlock
    Write();
    UnlockFile();
}

void BBSGlobalStats::RemoveDownloadTrafficStats(size_t DownloadSize, size_t DownloadCount)
{
    //lock our file and read the latest info
    LockFile();
    Read(true);

    //now add what was provided
    _GlobalData.Traffic.DownloadCount -= DownloadCount;
    _GlobalData.Traffic.DownloadSize -= DownloadSize;

    //write and unlock
    Write();
    UnlockFile();
}

void BBSGlobalStats::AddUploadTrafficStats(size_t UploadSize)
{
    //lock our file and read the latest info
    LockFile();
    Read(true);

    //now add what was provided
    _GlobalData.Traffic.UploadCount++;
    _GlobalData.Traffic.UploadSize += UploadSize;

    //write and unlock
    Write();
    UnlockFile();
}

void BBSGlobalStats::RemoveUploadTrafficStats(size_t UploadSize, size_t UploadCount)
{
    //lock our file and read the latest info
    LockFile();
    Read(true);

    //now add what was provided
    _GlobalData.Traffic.UploadCount -= UploadCount;
    _GlobalData.Traffic.UploadSize -= UploadSize;

    //write and unlock
    Write();
    UnlockFile();
}

void BBSGlobalStats::IncrementTotalLogins()
{
    //lock our file and read the latest info
    LockFile();
    Read(true);

    //update
    _GlobalData.TotalLogins++;
    Write();
    UnlockFile();
}

void BBSGlobalStats::IncrementTotalUsers()
{
    //lock our file and read the latest info
    LockFile();
    Read(true);

    //update
    _GlobalData.TotalUsers++;
    Write();
    UnlockFile();
}

void BBSGlobalStats::IncrementTotalMessages()
{
    //lock our file and read the latest info
    LockFile();
    Read(true);

    //update
    _GlobalData.TotalMessages++;
    Write();
    UnlockFile();
}

void BBSGlobalStats::LockFile()
{
    int fd;
    timespec delay;

    //attempt to get a lock file first
    while(1)
    {
        fd = open(GLOBALFILE ".lock", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

        //if we got lock then exit otherwise wait a short moment
        if(fd >= 0)
            break;

        delay.tv_sec = 0;
        delay.tv_nsec = 1000;
        nanosleep(&delay, 0);
    }

    _lockfd = fd;
}

void BBSGlobalStats::UnlockFile()
{
    if(_lockfd != -1)
    {
        unlink(GLOBALFILE ".lock");
        close(_lockfd);
        _lockfd = -1;
    }
}

void BBSGlobalStats::Read()
{
    return Read(false);
}

void BBSGlobalStats::Read(bool LeaveOpen)
{
    int fd;

    //open up the global data file and get info
    if(LeaveOpen)
        fd = open(GLOBALFILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    else
        fd = open(GLOBALFILE, O_RDONLY);

    //if file failed to open, return
    if(fd < 0)
        return;

    read(fd, &_GlobalData, sizeof(_GlobalData));

    //if leaving open then we will be writing so go to the beginning
    if(LeaveOpen)
    {
        _fd = fd;
        lseek(fd, 0, SEEK_SET);
    }
    else
        close(fd);
}

void BBSGlobalStats::Write()
{
    int fd;

    //if no lock then fail
    if(_lockfd == -1)
        return;

    //if a file is already open then use it
    if(_fd != -1)
    {
        fd = _fd;
        _fd = -1;
    }
    else
    {
        //open the global file
        fd = open(GLOBALFILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if(fd < 0)
            return;
    }

    //write out the structure
    write(fd, &_GlobalData, sizeof(_GlobalData));

    //close everything
    close(fd);
}
