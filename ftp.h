// File transfer via SSH
#ifndef __FTP_H
#define __FTP_H

#define FTP_ERROR_Ok 0
#define FTP_ERROR -1
#define FTP_ERROR_FAILED_TO_READ_LOCAL -2
#define FTP_ERROR_FAILED_TO_READ_REMOTE -3
#define FTP_ERROR_FAILED_TO_WRITE_LOCAL -4
#define FTP_ERROR_FAILED_TO_WRITE_REMOTE -5

// Uploads a file to a server. Returns negative value if failed, 0 if ok.
// The connection credentials must be set earlier...
int ftpUpload( char *srcFileName,					// A file to transfer to a server
	char *dstFileName, 								// A name for the file when it is stored at the server
	char *dstDirectory); 							// A directory to transfer the file into. For Linux servers starts with '/'

// Downloads a file from a server. Returns negative value if failed, 0 if ok.
// The connection credentials must be set earlier...
int ftpDownload( char *dstFileName, 				// A file name to save the downloaded file under 
	char *srcFileName, 								// A file to download
	char *srcDirectory); 							// The directory to find the file at the server

// Test is a file exists at a server. "1" - yes, "0" - no, "-1" - error.
// The connection credentials must be set earlier...
int ftpTest( char *fileName, 					// A file name to test
	char *directory, 							// A server directory to test in
  	unsigned long int *size );					// If not NULL receives the size of file in bytes

// 
void ftpSetCredentials(char *server, char *user, char *password);

int ftpInit( void ); // Must be called before doing anything else...

void ftpClose( void ); // Must be called when all transfers are finished...

int ftpGetLastError( int *ftpErrorCode, 		// 
	DWORD WINAPI *winInetErrorCode, 						//
	char *winInetErrorText );						//

#endif
