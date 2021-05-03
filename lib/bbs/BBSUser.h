#ifndef NEKISAHLOTH_LIB_BBS_USER
#define NEKISAHLOTH_LIB_BBS_USER

#include <unistd.h>
#include <stdint.h>
#include <HashBase.h>
#include <string>
#include <BBSGlobalStats.h>

class BBSUser
{
	public:
		typedef enum ColorChoice : uint8_t
		{
			COLOR_UNSET,
			COLOR_NO,
			COLOR_YES,
		} ColorChoice;

		static const uint8_t LEVEL_SYSOP = 255;

		BBSUser();
		BBSUser(const char *Username);
		BBSUser(BBSUser *Orig);
		~BBSUser();
	
		int ValidatePassword(const char *Password);
		size_t GetID();
		int GetPrivilegeLevel();
		int IsNewUser();
		int GetTotalLogins();
		time_t GetPreviousLoginTime();
		bool IsLocked();
		bool SetLocked(bool Lock);
		
		int GetTrafficStats(BBSGlobalStats::TrafficStatsStruct *Stats);
		void AddDownloadTrafficStats(size_t DownloadSize);
		void AddUploadTrafficStats(size_t UploadSize);
		void ResetStats();

		ColorChoice GetColorChoice();
		void SetColorChoice(ColorChoice Color);

		int SetPassword(const char *CurPassword, const char *NewPassword);
		void SetPrivilegeLevel(int Level);

		bool GetShowWelcome();
		void SetShowWelcome(bool Display);

		const char *GetName();

		bool Find(const char *Name);
		bool Find(std::string *Name);
		bool FindExact(std::string *Name);
		bool Find(size_t ID);
		bool FindExact(size_t ID);
		bool FindNext();
		void FindDone();

	private:
		typedef enum CHANGE_ENUM
		{
			NOT_ALLOWED,
			IS_OWNER,
			IS_SYSOP
		} CHANGE_ENUM;
		CHANGE_ENUM ChangeAllowed();
		void Save();

		bool _CanFind;
		int _find_fd;
		std::string _FindMatchStr;
		size_t _FindMatchID;

		typedef struct UserDataStruct
		{
			size_t ID;
			uint8_t PasswordSalt[16];
			uint8_t PasswordHash[16];
			size_t TotalLogins;
			uint8_t PrivilegeLevel;
			uint8_t UsernameLen;
			uint8_t Color : 2;
			uint8_t ShowWelcome : 1;
			uint8_t Locked : 1;
			uint8_t SysopAltered : 1;
			uint8_t PaddingBits : 3;
			uint8_t Padding;
			BBSGlobalStats::TrafficStatsStruct Traffic;
			time_t CreationTime;
			time_t LastLoginTime;
		} UserDataStruct;

		std::string _Username;
		UserDataStruct _UserData;
		size_t _UserFileOffset;
		time_t _PreviousLoginTime;
};

extern BBSUser *GlobalUser;

#endif
