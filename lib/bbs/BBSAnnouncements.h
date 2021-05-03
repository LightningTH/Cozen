#ifndef NEKISAHLOTH_LIB_BBS_ANNOUNCEMENTS
#define NEKISAHLOTH_LIB_BBS_ANNOUNCEMENTS

#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <string>
#include <vector>
#include "BBSMenu.h"

using namespace std;

class BBSAnnouncements : public BBSMenu
{
	public:
		BBSAnnouncements();
		BBSAnnouncements(ConnectionBase *Conn, BBSRender *Render);
		BBSAnnouncements(ConnectionBase *Conn, BBSRender *Render, bool DoSysOp);
		~BBSAnnouncements();
		size_t DoAction(size_t ID);
		time_t LastAnnouncementTime();
		size_t GetValue(const char *Name, void *Data, size_t DataLen);


	private:
		typedef struct AnnouncementFileStruct
		{
			time_t Timestamp;
			uint16_t Len;
		} AnnouncementFileStruct;

		typedef struct AnnouncementStruct
		{
			time_t Timestamp;
			char Announcement[0];
		} AnnouncementStruct;

		void InitAnnouncements(bool DoSysOp);
		void AddAnnouncement(char *Announcement);
		void DeleteAnnouncement(size_t Index);
		void GenerateMenu();

		void LockFile();
		void UnlockFile();
		void Read();
		void Write();

		vector<AnnouncementStruct *> _Announcements;
		int _lockfd;
		bool _InSysopMenu;
		time_t _LastAnnouncementTime;

};

#endif
