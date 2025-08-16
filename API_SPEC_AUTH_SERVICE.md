# Authentication Service API Specification

## Overview
The Authentication Service manages OAuth2 authentication with Microsoft accounts, token storage, and refresh operations. It runs as a BLooper within the OneDrive daemon and communicates via BMessage protocol.

## Class Hierarchy
```
BLooper
  |
  +-- AuthenticationService
```

## Public API

### Constants
```cpp
// Message commands
const uint32 MSG_AUTH_START = 'aust';
const uint32 MSG_AUTH_CANCEL = 'auca';
const uint32 MSG_AUTH_REFRESH = 'aure';
const uint32 MSG_AUTH_LOGOUT = 'aulo';
const uint32 MSG_AUTH_STATUS = 'aust';

// OAuth2 endpoints
const char* kAuthorizationEndpoint = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize";
const char* kTokenEndpoint = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
const char* kRedirectUri = "http://localhost:8080/auth/callback";

// Scopes
const char* kScopes = "files.readwrite.all offline_access";
```

### Main Class: AuthenticationService

```cpp
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
    
    // Members
    BString fAccessToken;
    BString fRefreshToken;
    time_t fExpiryTime;
    BString fAccountEmail;
    BMessenger fReplyTarget;
    thread_id fServerThread;
    int32 fServerPort;
    bool fAuthInProgress;
};
```

## Message Protocol

### Incoming Messages

#### MSG_AUTH_START
Initiates OAuth2 authentication flow.
```
Fields:
- "messenger" (BMessenger): Reply target for results
```

#### MSG_AUTH_CANCEL
Cancels ongoing authentication.
```
Fields: None
```

#### MSG_AUTH_REFRESH
Refreshes the access token using refresh token.
```
Fields: None
```

#### MSG_AUTH_LOGOUT
Logs out current user and clears tokens.
```
Fields: None
```

#### MSG_AUTH_STATUS
Requests current authentication status.
```
Fields:
- "messenger" (BMessenger): Reply target
```

### Outgoing Messages

#### MSG_AUTH_SUCCESS
Sent when authentication succeeds.
```
Fields:
- "email" (string): User email address
- "expires" (int64): Token expiry timestamp
```

#### MSG_AUTH_FAILED
Sent when authentication fails.
```
Fields:
- "error" (string): Error description
- "code" (int32): Error code
```

#### MSG_AUTH_PROGRESS
Sent during authentication process.
```
Fields:
- "stage" (string): Current stage description
- "url" (string): Browser URL (when applicable)
```

## Usage Example

```cpp
// From OneDriveDaemon
void OneDriveDaemon::HandleAuthenticate(BMessage* message)
{
    BMessenger messenger;
    if (message->FindMessenger("messenger", &messenger) == B_OK) {
        fAuthService->StartAuthentication(messenger);
    }
}

// From Preferences App
void PreferencesWindow::AuthenticateClicked()
{
    BMessage msg(MSG_AUTH_START);
    msg.AddMessenger("messenger", BMessenger(this));
    
    BMessenger daemon(ONEDRIVE_SIGNATURE);
    daemon.SendMessage(&msg);
}

// Handling response
void PreferencesWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case MSG_AUTH_SUCCESS:
        {
            BString email;
            if (message->FindString("email", &email) == B_OK) {
                fAccountView->SetEmail(email);
                fAuthButton->SetLabel("Sign Out");
            }
            break;
        }
        
        case MSG_AUTH_FAILED:
        {
            BString error;
            if (message->FindString("error", &error) == B_OK) {
                BAlert* alert = new BAlert("Auth Failed",
                    error.String(), "OK");
                alert->Go();
            }
            break;
        }
    }
}
```

## Error Codes

```cpp
enum auth_error {
    AUTH_OK = B_OK,
    AUTH_ERROR_NETWORK = B_NETWORK_ERROR,
    AUTH_ERROR_CANCELLED = B_INTERRUPTED,
    AUTH_ERROR_INVALID_TOKEN = B_PERMISSION_DENIED,
    AUTH_ERROR_EXPIRED = B_TIMED_OUT,
    AUTH_ERROR_NO_REFRESH_TOKEN = B_ENTRY_NOT_FOUND,
    AUTH_ERROR_KEYSTORE = B_NOT_ALLOWED,
    AUTH_ERROR_PARSE = B_BAD_DATA
};
```

## Security Considerations

1. **Token Storage**: All tokens stored in BKeyStore, never in plain text
2. **HTTPS Only**: All communication with Microsoft uses HTTPS
3. **State Validation**: CSRF protection using state parameter
4. **Redirect URI**: Localhost only to prevent interception
5. **Token Refresh**: Automatic refresh before expiry
6. **Secure Cleanup**: Tokens cleared from memory after use

## Threading Model

- Main thread: BLooper message handling
- HTTP server thread: Handles OAuth redirect
- Network operations: Asynchronous with callbacks

## Dependencies

- Haiku Kits: Application, Network, Storage
- BKeyStore for secure token storage
- BHttpSession for API calls
- Native HTTP server for OAuth redirect

---
*This specification defines the public API for the Authentication Service component.*