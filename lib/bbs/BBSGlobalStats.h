#ifndef NEKISAHLOTH_LIB_BBS_GLOBAL_STATS
#define NEKISAHLOTH_LIB_BBS_GLOBAL_STATS

#include <unistd.h>
#include <stdint.h>
#include <HashBase.h>
#include <string>

class BBSGlobalStats
{
	public:
		typedef struct TrafficStatsStruct
		{
			uint32_t DownloadCount;
			uint32_t UploadCount;
			size_t DownloadSize;
			size_t UploadSize;
		} TrafficStatsStruct;

		typedef struct GlobalStatsStruct
		{
			size_t TotalLogins;
			size_t TotalUsers;
			size_t TotalMessages;
			TrafficStatsStruct Traffic;
		} GlobalStatsStruct;

		BBSGlobalStats();

		int GetGlobalStats(GlobalStatsStruct *Stats);
		void AddDownloadTrafficStats(size_t DownloadSize);
		void AddUploadTrafficStats(size_t UploadSize);
		void RemoveDownloadTrafficStats(size_t DownloadSize, size_t DownloadCount);
		void RemoveUploadTrafficStats(size_t UploadSize, size_t UploadCount);

		void IncrementTotalLogins();
		void IncrementTotalUsers();
		void IncrementTotalMessages();

	private:
		void LockFile();
		void UnlockFile();
		void Read();
		void Read(bool LeaveOpen);
		void Write();

		GlobalStatsStruct _GlobalData;
		int _lockfd;
		int _fd;
};

extern BBSGlobalStats *GlobalStats;

#endif
