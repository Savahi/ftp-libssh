#include <windows.h>
#include <wininte.h>
#include "ftp.h"

#define MAX_SERVER 100
static char _server[FTP_MAX_SERVER + 1];

#define FTP_MAX_USER 100
static char _user[FTP_MAX_USER + 1];

#define FTP_MAX_PASSWORD 100
static char _password[FTP_MAX_PASSWORD + 1];

#define FTP_MAX_REMOTE_ADDR 500
static char _remoteAddr[FTP_MAX_REMOTE_ADDR + 1];

static unsigned long _port;

static long int _ftpErrorCode = 0;
static DWORD WINAPI _winInetErrorCode = 0;
static char *_winInetErrorText = NULL;

static long int _timeOut = -1L;

static HINTERNET _hInternet = NULL;
static HINTERNET _hFtpSession = NULL;

static int createRemoteAddr(char *fileName, char *directory, char *server, char *user, char *password)
{
	if( server != NULL && user != NULL && password != NULL ) {	
		if (strlen(server) + strlen(user) + strlen(password) + strlen(directory) + strlen(fileName) + 7 >= FTP_MAX_REMOTE_ADDR) {
			return -1;
		}
		strcpy(_remoteAddr, "ftp://");
		strcat(_remoteAddr, user);
		strcat(_remoteAddr, ":");
		strcat(_remoteAddr, password);
		strcat(_remoteAddr, "@");
		strcat(_remoteAddr, server);
		strcat(_remoteAddr, directory);
		strcat(_remoteAddr, "/");
		strcat(_remoteAddr, fileName);
	} else {
		int directoryLength = strlen(directory);
		int fileNameLength = strlen(fileName);
		if (directoryLength + fileNameLength + 1 >= SFTP_MAX_REMOTE_ADDR) {
			return -1;
		}
		strcpy(_remoteAddr, directory);
		if( _remoteAddr[ directoryLength-1 ] != '/' ) {
			strcat(_remoteAddr, "/");
		}
		strcat(_remoteAddr, fileName);					
	}
	return 0;
}



int ftpTest(char *fileName,	char *directory, unsigned long int *size)
{
	_ftpErrorCode = 0;
	_winInetErrorCode = 0;

	return 1;
}


int ftpUpload(char *srcFileName, char *dstFileName, char *dstDirectory ) 
{
	_ftpErrorCode = 0;
	_winInteErrorCode = 0;
	BOOLAPI status;

	if (createRemotePath(dstFileName, dstDirectory, NULL, NULL, NULL) == -1) {
		_ftpErrorCode = -1;
	} else {
		status = FtpPutFileA(_hFtpSession, srcFileName, _remoteAddr, FTP_TRANSFER_TYPE_BINARY, 0); 
		if( !status ) {
			_ftpErrorCode = -1;
		}
	}
	return _ftpErrorCode;
}


int ftpDownload(char *dstFileName, char *srcFileName, char *srcDirectory ) 
{
	_ftpErrorCode = 0;
	_winInteErrorCode = 0;

	if (createRemotePath(srcFileName, srcDirectory, NULL, NULL, NULL) == -1) {
		_ftpErrorCode = -1;
	} else {
		status = FtpGetFileA(_hFtpSession, _remoteAddr, dstFileName, false, FILE_ATTRIBUTE_NORMAL, FTP_TRANSFER_TYPE_BINARY, 0); 
		if( !status ) {
			_ftpErrorCode = -1;
		}
	}
	return _ftpErrorCode;
}


void ftpSetTimeOut(unsigned long int timeOut) {
	_ftpErrorCode = 0;
	_winInteErrorCode = 0;

	_timeOut = timeOut;
}


void ftpSetCredentials(char *server, char *user, char *password, int port) {
	_ftpErrorCode = 0;
	_winInteErrorCode = 0;

	if( strlen(server) > FTP_MAX_SERVER || strlen(user) > FTP_MAX_USER || strlen(password) > FTP_MAX_PASSWORD ) {
		_ftpErrorCode = -1;
	} else {
		strcpy( _server, server );
		strcpy( _user, user );
		strcpy( _password, password );
		_port = port;		
	}
	return _ftpErrorCode;
}


int ftpInit(void) {
	_ftpErrorCode = 0;
	_winInetErrorCode = 0;

	HINTERNET _hInternet = InternetOpenA(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (!_hInternet) {
		_ftpErrorCode = -1;
	} else {
		HINTERNET _hFtpSession = InternetConnectA(_hInternet, _server, INTERNET_DEFAULT_FTP_PORT, _user, _password, 
			INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0);
		if (!_hFtpSession) {
			_ftpErrorCode = -1;
		} 
	}

	if( _ftpErrorCode != 0 ) {
		ftpClose();
	}
}


void ftpClose(void) {
	_ftpErrorCode = 0;
	_winInteErrorCode = 0;

	if( _hFtpSession != NULL ) {
	    InternetCloseHandle(_hFtpSession);
	}
	_hFtpSession = NULL;
	if( _hInternet != NULL ) {
	    InternetCloseHandle(_hInternet);
	}
	_hInternet = NULL;
}


int ftpGetLastError(int *ftpErrorCode, DWORD WINAPI *winInetErrorCode, char *winInetErrorText) {
	if (ftpErrorCode != NULL) {
		*ftpErrorCode = _ftpErrorCode;
	}
	if (winInetErrorCode != NULL) {
		*winInetErrorCode = GetLastError();
	}
	if (winInetErrorText != NULL) {
		winInetErrorText = NULL;
	}
	return 0;
}
