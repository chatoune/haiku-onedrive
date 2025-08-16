#ifndef ONEDRIVE_AUTHENTICATION_SERVICE_H
#define ONEDRIVE_AUTHENTICATION_SERVICE_H

#include <Looper.h>
#include <Messenger.h>
#include <String.h>

// Forward declarations
class BMessage;

// Message commands
const uint32 MSG_AUTH_START = 'aust';
const uint32 MSG_AUTH_CANCEL = 'auca';
const uint32 MSG_AUTH_REFRESH = 'aure';
const uint32 MSG_AUTH_LOGOUT = 'aulo';
const uint32 MSG_AUTH_STATUS = 'auss';

// Response messages
const uint32 MSG_AUTH_SUCCESS = 'ausc';
const uint32 MSG_AUTH_FAILED = 'aufa';
const uint32 MSG_AUTH_PROGRESS = 'aupr';

// Error codes
enum auth_error {
	AUTH_OK = B_OK,
	AUTH_ERROR_NETWORK = B_ERROR,
	AUTH_ERROR_CANCELLED = B_INTERRUPTED,
	AUTH_ERROR_INVALID_TOKEN = B_PERMISSION_DENIED,
	AUTH_ERROR_EXPIRED = B_TIMED_OUT,
	AUTH_ERROR_NO_REFRESH_TOKEN = B_ENTRY_NOT_FOUND,
	AUTH_ERROR_KEYSTORE = B_NOT_ALLOWED,
	AUTH_ERROR_PARSE = B_BAD_DATA
};

class AuthenticationService : public BLooper {
public:
	// Constructor/Destructor
	AuthenticationService();
	virtual ~AuthenticationService();
	
	// BLooper overrides
	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();
	
	// Authentication operations
	status_t StartAuthentication(BMessenger replyTo);
	status_t CancelAuthentication();
	status_t RefreshToken();
	status_t Logout();
	
	// Token management
	bool IsAuthenticated() const;
	status_t GetAccessToken(BString& token);
	time_t GetTokenExpiry() const;
	
	// Account information
	status_t GetUserInfo(BMessage& info);
	status_t GetAccountEmail(BString& email);
	
private:
	// OAuth2 flow
	status_t GenerateAuthUrl(BString& url);
	status_t ExchangeCodeForToken(const char* code);
	status_t ParseTokenResponse(const BString& response);
	
	// Token storage
	status_t StoreTokens();
	status_t LoadTokens();
	status_t DeleteTokens();
	
	// HTTP server for redirect
	status_t StartRedirectServer();
	status_t StopRedirectServer();
	
	// OAuth2 endpoints
	static const char* kAuthorizationEndpoint;
	static const char* kTokenEndpoint;
	static const char* kRedirectUri;
	static const char* kScopes;
	
	// Members
	BString fAccessToken;
	BString fRefreshToken;
	time_t fExpiryTime;
	BString fAccountEmail;
	BMessenger fReplyTarget;
	thread_id fServerThread;
	int32 fServerPort;
	bool fAuthInProgress;
	
	// HTTP session for API calls (placeholder for now)
	void* fHttpSession;
};

#endif // ONEDRIVE_AUTHENTICATION_SERVICE_H