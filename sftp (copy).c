#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "sftp.h"

struct FtpFile {
	char *filename;
	FILE *stream;
};

#define SFTP_MAX_SERVER 100
static char _server[SFTP_MAX_SERVER + 1];

#define SFTP_MAX_USER 100
static char _user[SFTP_MAX_USER + 1];

#define SFTP_MAX_PASSWORD 100
static char _password[SFTP_MAX_PASSWORD + 1];

#define SFTP_MAX_REMOTE_ADDR 500
static char _remoteAddr[SFTP_MAX_REMOTE_ADDR + 1];

static long int _sftpErrorCode = 0;
static CURLcode _curlErrorCode = CURLE_OK;
static char *_curlError = NULL;

static long int _timeOut = -1L;

static size_t writefunc(void *buffer, size_t size, size_t nmemb, void *stream)
{
	struct FtpFile *out = (struct FtpFile *)stream;
	if (out && !out->stream) {
		/* open file for writing */
		out->stream = fopen(out->filename, "wb");
		if (!out->stream) {
			_sftpErrorCode = SFTP_ERROR_FAILED_TO_WRITE_DEST;
			return -1; /* failure, can't open file to write */
		}
	}
	return fwrite(buffer, size, nmemb, out->stream);
}

/* read data to upload */
static size_t readfunc(void *ptr, size_t size, size_t nmemb, void *stream)
{
	FILE *f = (FILE *)stream;
	size_t n;

	if (ferror(f)) {
		return CURL_READFUNC_ABORT;
	}
	n = fread(ptr, size, nmemb, f) * size;
	return n;
}


static int createRemotePath(char *fileName, char *directory,
	char *server, char *user, char *password)
{
	if (strlen(server) + strlen(user) + strlen(password) + strlen(directory) + strlen(fileName) + 7 >= SFTP_MAX_REMOTE_ADDR) {
		return -1;
	}
	strcpy(_remoteAddr, "sftp://");
	strcat(_remoteAddr, user);
	strcat(_remoteAddr, ":");
	strcat(_remoteAddr, password);
	strcat(_remoteAddr, "@");
	strcat(_remoteAddr, server);
	strcat(_remoteAddr, directory);
	strcat(_remoteAddr, "/");
	strcat(_remoteAddr, fileName);
	return 0;
}


static curl_off_t getRemoteFileSize(char *remoteFile)
{
	curl_off_t remoteFileSizeByte = -1;
	CURL *curl = NULL;

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_URL, remoteFile);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	curl_easy_setopt(curl, CURLOPT_HEADER, 1);
	curl_easy_setopt(curl, CURLOPT_FILETIME, 1);

	if (_timeOut > 0) {
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, _timeOut);
	}
	_curlErrorCode = curl_easy_perform(curl);
	if (_curlErrorCode == CURLE_OK) {
		_curlErrorCode = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &remoteFileSizeByte);
		if( _curlErrorCode != CURLE_OK ) { 
			_curlError = (char *)curl_easy_strerror(_curlErrorCode);		
		}
	} else {
		_curlError = (char *)curl_easy_strerror(_curlErrorCode);				
	}
	curl_easy_cleanup(curl);

	return remoteFileSizeByte;
}


static int upload(CURL *curl, char *remotePath, char *srcFileName)
{
	int returnStatus;
	FILE *f = NULL;

	f = fopen(srcFileName, "rb");
	if (f) {
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl, CURLOPT_URL, remotePath);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfunc);
		curl_easy_setopt(curl, CURLOPT_READDATA, f);
		//curl_easy_setopt(curl, CURLOPT_APPEND, 1L);

		if (_timeOut > 0) {
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, _timeOut);
		}
		_curlErrorCode = curl_easy_perform(curl);
		if( _curlErrorCode != CURLE_OK ) {
			returnStatus = -1;
			_curlError = (char *)curl_easy_strerror(_curlErrorCode);		
		} else {			
			returnStatus = 0;
		}
		fclose(f);
	} else {
		returnStatus = -2;		
	}
}


static int download(CURL *curl, char *remotePath, char *dstFileName) {
	int returnStatus;

	struct FtpFile ftpfile = {
		dstFileName,
		NULL
	};

	curl_easy_setopt(curl, CURLOPT_URL, remotePath);
	/* Define our callback to get called when there's data to be written */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
	/* Set a pointer to our struct to pass to the callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ftpfile);
	/* Switch on full protocol/debug output */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

	if (_timeOut > 0) {
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, _timeOut);
	}
	_curlErrorCode = curl_easy_perform(curl);
	if (_curlErrorCode != CURLE_OK) {
		_curlError = (char *)curl_easy_strerror(_curlErrorCode);		
		if( _curlError == CURLE_REMOTE_FILE_NOT_FOUND ) {
			returnStatus = -2;
		} else {
			returnStatus = -1;
		}
	} else {
		returnStatus = 0;		
	}
	return returnStatus;
}


int sftpTest(char *fileName,		// A file name to test
	char *directory,				// A server directory to test in
	unsigned long int *size) 
{
	int status = sftpTest( fileName, directory, _server, _user, _password, size );
	return status;
}


// Tests if a file exists at a server
int sftpTest(char *fileName,					// A file name to test
	char *directory,							// A server directory to test in
	char *server, char *user, char *password,	// Connection requisites: server, login, password
	unsigned long int *size)
{
	_sftpErrorCode = 0;
	_curlErrorCode = CURLE_OK;

	if (createRemotePath(fileName, directory, server, user, password) == -1) {
		return -1;
	}

	CURL *curl = curl_easy_init();
	if (curl == NULL) {
		return -1;
	}

	curl_off_t remoteFileSizeByte = getRemoteFileSize(_remoteAddr);
	if (remoteFileSizeByte == -1) {
		if (_curlErrorCode == CURLE_REMOTE_FILE_NOT_FOUND) {
			return 0;
		}
		else {
			return -1;
		}
	}

	if (size != NULL) {
		*size = remoteFileSizeByte;
	}

	return 1;
}


int sftpUpload(char *srcFileName, char *dstFileName, char *dstDirectory ) {
	int status = sftpUploadP(srcFileName, dstFileName, dstDirectory, server, user, password);
	return status;
}


int sftpUploadP(char *srcFileName, char *dstFileName, char *dstDirectory,
	char *server, char *user, char *password)
{
	_curlErrorCode = CURLE_OK;

	if (createRemotePath(dstFileName, dstDirectory, server, user, password) == -1) {
		_sftpErrorCode = -1;
	} else {
		CURL *curl = curl_easy_init();
		if (curl == NULL) {
			_sftpErrorCode = -1;
		} else {
			int status = upload(curl, _remoteAddr, srcFileName);
			if( status == 0 ) {
				_sftpErrorCode = 0;
			} else {
				if( status == -2 ) {
					_sftpErrorCode = SFTP_ERROR_FAILED_TO_READ_LOCAL;
				} else {
					_sftpErrorCode = -1;
				}
			}
			curl_easy_cleanup(curl);		
		}
	}

	return _sftpErrorCode;
}


int sftpDownload(char *dstFileName, char *srcFileName, char *srcDirectory ) {
	int status = sftpDownloadP(dstFileName, srcFileName, srcDirectory, _server, _user, _password);
	return status;
}


int sftpDownloadP(char *dstFileName, char *srcFileName, char *srcDirectory,
	char *server, char *user, char *password)
{
	_curlErrorCode = CURLE_OK;

	if (createRemotePath(srcFileName, srcDirectory, server, user, password) == -1) {
		_sftpErrorCode = -1;
	} else {
		CURL *curl = curl_easy_init();
		if (curl == NULL) {
			_sftpErrorCode = -1;
		} else {
			int status = download(curl, _remoteAddr, dstFileName);
			if( status != 0 ) {
				if( _curlErrorCode != CURLE_REMOTE_FILE_NOT_FOUND ) {
					_sftpErrorCode = SFTP_ERROR_FAILED_TO_READ_REMOTE;
				} else {
					_sftpErrorCode = -1;				
				}
			} else {
				_sftpErrorCode = 0;
			}
			curl_easy_cleanup(curl);		
		}
	}
	return _sftpErrorCode;
}


void sftpSetTimeOut(unsigned long int timeOut) {
	_sftpErrorCode = 0;
	_timeOut = timeOut;
}


void sftpSetCredentials(char *server, char *user, char *password) {
	_sftpErrorCode = 0;

	if( strlen(server) > SFTP_MAX_SERVER || strlen(user) > SFTP_MAX_USER || strlen(password) > SFTP_MAX_PASSWORD ) {
		_sftpErrorCode = -1;
	} else {
		strcpy( _server, server );
		strcpy( _user, user );
		strcpy( _password, password );		
	}
	return _sftpErrorCode;
}


int sftpInit(void) {
	_sftpErrorCode = 0;
	_curlErrorCode = CURLE_OK;

	if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
		return -1;
	}
	return 0;
}


void sftpClose(void) {
	_sftpErrorCode = 0;
	_curlErrorCode = CURLE_OK;

	curl_global_cleanup();
}


int sftpGetLastError(int *sftpErrorCode, int *curlErrorCode, char *curlErrorText) {
	if (sftpErrorCode != NULL) {
		*sftpErrorCode = _sftpErrorCode;
	}
	if (curlErrorCode != NULL) {
		*curlErrorCode = _curlErrorCode;
	}
	if (curlErrorCode != NULL) {
		curlErrorText = (char *)curl_easy_strerror(_curlErrorCode);
	}
	return 0;
}