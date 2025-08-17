#include "AuthenticationService.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <Alert.h>
#include <Application.h>
#include <KeyStore.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>
#include <Roster.h>
#include <String.h>
#include <SupportDefs.h>

// HTTP functionality will be implemented later
// For now, we're using stubs to validate the KeyStore integration

// OAuth2 endpoint constants
const char* AuthenticationService::kAuthorizationEndpoint = 
	"https://login.microsoftonline.com/common/oauth2/v2.0/authorize";
const char* AuthenticationService::kTokenEndpoint = 
	"https://login.microsoftonline.com/common/oauth2/v2.0/token";
const char* AuthenticationService::kRedirectUri = 
	"http://localhost:8080/auth/callback";
const char* AuthenticationService::kScopes = 
	"files.readwrite.all offline_access";

// Removed using declaration for private API - will implement HTTP later

// Constructor
AuthenticationService::AuthenticationService()
	: BLooper("AuthenticationService"),
	  fExpiryTime(0),
	  fServerThread(-1),
	  fServerPort(8080),
	  fAuthInProgress(false),
	  fHttpSession(NULL)
{
	// Set thread priority for authentication service
	// Use normal priority as auth is user-initiated
	set_thread_priority(Thread(), B_NORMAL_PRIORITY);
	
	// Initialize the looper
	Run();
	
	// Load configuration (will be implemented with config file support later)
	// For now, use default values that users must override
	fClientId = "YOUR_CLIENT_ID_HERE";  // Placeholder - users must register their own app
	
	// Try to load existing tokens from KeyStore
	status_t loadResult = LoadTokens();
	if (loadResult == B_OK) {
		// Check if token needs refresh
		// Microsoft tokens expire in 60-90 minutes, refresh if less than 10 minutes left
		time_t currentTime = time(NULL);
		if (fExpiryTime > 0 && fExpiryTime - currentTime < 600) {
			// Token expires in less than 10 minutes, schedule refresh
			BMessage refreshMsg(MSG_AUTH_REFRESH);
			PostMessage(&refreshMsg);
		}
	}
	
	// Initialize OAuth2 state for CSRF protection
	fAuthState = "";
	
	// Clear any stale authentication state
	fAuthInProgress = false;
	
	// HTTP session will be initialized when needed
	fHttpSession = NULL;
}

// Destructor
AuthenticationService::~AuthenticationService()
{
	// Clean up HTTP session when implemented
}

// BLooper overrides
void
AuthenticationService::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_AUTH_START:
		{
			BMessenger replyTo;
			if (message->FindMessenger("messenger", &replyTo) == B_OK) {
				StartAuthentication(replyTo);
			} else {
				// No reply messenger provided
				BMessage reply(MSG_AUTH_FAILED);
				reply.AddString("error", "No reply messenger provided");
				reply.AddInt32("code", B_BAD_VALUE);
				message->SendReply(&reply);
			}
			break;
		}
		
		case MSG_AUTH_CANCEL:
		{
			status_t result = CancelAuthentication();
			BMessage reply(MSG_AUTH_SUCCESS);
			reply.AddBool("cancelled", true);
			reply.AddInt32("status", result);
			
			// Send to stored reply target if we have one
			if (fReplyTarget.IsValid()) {
				fReplyTarget.SendMessage(&reply);
			} else {
				message->SendReply(&reply);
			}
			break;
		}
		
		case MSG_AUTH_REFRESH:
		{
			status_t result = RefreshToken();
			
			if (result == B_OK) {
				// Token refresh will be implemented later
				// For now, just acknowledge the request
				BMessage reply(MSG_AUTH_SUCCESS);
				reply.AddBool("refreshed", true);
				
				if (fReplyTarget.IsValid()) {
					fReplyTarget.SendMessage(&reply);
				} else {
					message->SendReply(&reply);
				}
			} else {
				BMessage reply(MSG_AUTH_FAILED);
				reply.AddString("error", "Token refresh failed");
				reply.AddInt32("code", result);
				
				if (fReplyTarget.IsValid()) {
					fReplyTarget.SendMessage(&reply);
				} else {
					message->SendReply(&reply);
				}
			}
			break;
		}
		
		case MSG_AUTH_LOGOUT:
		{
			status_t result = Logout();
			
			// Logout already sends notification through its implementation
			// Just send a reply to the direct sender as well
			BMessage reply(MSG_AUTH_SUCCESS);
			reply.AddBool("logged_out", true);
			reply.AddInt32("status", result);
			message->SendReply(&reply);
			break;
		}
		
		case MSG_AUTH_STATUS:
		{
			BMessenger replyTo;
			bool hasReplyTo = (message->FindMessenger("messenger", &replyTo) == B_OK);
			
			BMessage reply(MSG_AUTH_SUCCESS);
			reply.AddBool("authenticated", IsAuthenticated());
			
			if (IsAuthenticated()) {
				reply.AddString("email", fAccountEmail);
				reply.AddInt64("expires", fExpiryTime);
				
				// Calculate time until expiry
				time_t currentTime = time(NULL);
				if (fExpiryTime > currentTime) {
					reply.AddInt64("expires_in", fExpiryTime - currentTime);
				} else {
					reply.AddInt64("expires_in", 0);
				}
			}
			
			if (hasReplyTo) {
				replyTo.SendMessage(&reply);
			} else {
				message->SendReply(&reply);
			}
			break;
		}
		
		default:
			BLooper::MessageReceived(message);
			break;
	}
}

bool
AuthenticationService::QuitRequested()
{
	return true;
}

// Authentication operations
status_t
AuthenticationService::StartAuthentication(BMessenger replyTo)
{
	// Check if authentication is already in progress
	if (fAuthInProgress) {
		BMessage reply(MSG_AUTH_FAILED);
		reply.AddString("error", "Authentication already in progress");
		reply.AddInt32("code", B_BUSY);
		replyTo.SendMessage(&reply);
		return B_BUSY;
	}
	
	// Store the reply target
	fReplyTarget = replyTo;
	fAuthInProgress = true;
	
	// Generate the OAuth2 authorization URL
	BString authUrl;
	status_t result = GenerateAuthUrl(authUrl);
	if (result != B_OK) {
		fAuthInProgress = false;
		BMessage reply(MSG_AUTH_FAILED);
		reply.AddString("error", "Failed to generate authorization URL");
		reply.AddInt32("code", result);
		replyTo.SendMessage(&reply);
		return result;
	}
	
	// Start the redirect server to catch the callback
	result = StartRedirectServer();
	if (result != B_OK) {
		fAuthInProgress = false;
		BMessage reply(MSG_AUTH_FAILED);
		reply.AddString("error", "Failed to start redirect server");
		reply.AddInt32("code", result);
		replyTo.SendMessage(&reply);
		return result;
	}
	
	// Send progress message with the URL to open
	BMessage progress(MSG_AUTH_PROGRESS);
	progress.AddString("stage", "authorization");
	progress.AddString("url", authUrl);
	replyTo.SendMessage(&progress);
	
	// In a full implementation, we would open the browser here
	// For now, the UI component will handle opening the URL
	
	return B_OK;
}

status_t
AuthenticationService::CancelAuthentication()
{
	if (!fAuthInProgress) {
		return B_NOT_ALLOWED;
	}
	
	// Stop the redirect server if running
	StopRedirectServer();
	
	// Clear authentication state
	fAuthInProgress = false;
	fAuthState = "";
	
	return B_OK;
}

status_t
AuthenticationService::RefreshToken()
{
	if (fRefreshToken.IsEmpty())
		return AUTH_ERROR_NO_REFRESH_TOKEN;
	return B_OK;
}

status_t
AuthenticationService::Logout()
{
	// Delete all stored tokens
	status_t result = DeleteTokens();
	
	// Clear authentication state
	fAuthInProgress = false;
	fAuthState = "";
	
	// Notify reply target if set
	if (fReplyTarget.IsValid()) {
		BMessage reply(MSG_AUTH_SUCCESS);
		reply.AddBool("logged_out", true);
		fReplyTarget.SendMessage(&reply);
	}
	
	return result;
}

// Token management
bool
AuthenticationService::IsAuthenticated() const
{
	return !fAccessToken.IsEmpty() && (fExpiryTime > time(NULL));
}

status_t
AuthenticationService::GetAccessToken(BString& token)
{
	if (fAccessToken.IsEmpty())
		return AUTH_ERROR_NO_REFRESH_TOKEN;
	
	token = fAccessToken;
	return B_OK;
}

time_t
AuthenticationService::GetTokenExpiry() const
{
	return fExpiryTime;
}

// Account information
status_t
AuthenticationService::GetUserInfo(BMessage& info)
{
	if (!IsAuthenticated())
		return AUTH_ERROR_NO_REFRESH_TOKEN;
	
	info.MakeEmpty();
	info.AddString("email", fAccountEmail);
	return B_OK;
}

status_t
AuthenticationService::GetAccountEmail(BString& email)
{
	if (fAccountEmail.IsEmpty())
		return AUTH_ERROR_NO_REFRESH_TOKEN;
		
	email = fAccountEmail;
	return B_OK;
}

// OAuth2 flow (private)
status_t
AuthenticationService::GenerateAuthUrl(BString& url)
{
	// Check if client ID is configured
	if (fClientId == "YOUR_CLIENT_ID_HERE" || fClientId.IsEmpty()) {
		return AUTH_ERROR_PARSE;  // Using parse error to indicate config issue
	}
	
	// Generate random state for CSRF protection
	GenerateRandomState(fAuthState);
	
	// URL encode parameters
	BString encodedRedirectUri;
	BString encodedScopes;
	BString encodedClientId;
	
	UrlEncode(kRedirectUri, encodedRedirectUri);
	UrlEncode(kScopes, encodedScopes);
	UrlEncode(fClientId, encodedClientId);
	
	// Build the authorization URL with required parameters
	url = kAuthorizationEndpoint;
	url << "?client_id=" << encodedClientId;
	url << "&response_type=code";
	url << "&redirect_uri=" << encodedRedirectUri;
	url << "&scope=" << encodedScopes;
	url << "&state=" << fAuthState;  // State doesn't need encoding (alphanumeric only)
	
	// Add optional parameters for better UX
	url << "&response_mode=query";  // Return code as query parameter
	url << "&prompt=select_account";  // Allow user to select account
	
	return B_OK;
}

void
AuthenticationService::GenerateRandomState(BString& state)
{
	// Generate a random state string for CSRF protection
	// Using current time and random number for uniqueness
	srand(time(NULL) + system_time());
	
	state = "";
	const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	int chars_len = strlen(chars);
	
	// Generate 32 character random string
	for (int i = 0; i < 32; i++) {
		state << chars[rand() % chars_len];
	}
}

void
AuthenticationService::UrlEncode(const BString& input, BString& output)
{
	output = "";
	const char* str = input.String();
	
	for (int i = 0; i < input.Length(); i++) {
		char c = str[i];
		
		// Check if character needs encoding
		if ((c >= 'A' && c <= 'Z') || 
		    (c >= 'a' && c <= 'z') || 
		    (c >= '0' && c <= '9') ||
		    c == '-' || c == '_' || c == '.' || c == '~') {
			// Safe characters - no encoding needed
			output << c;
		} else if (c == ' ') {
			// Space can be encoded as + in query strings
			output << '+';
		} else {
			// Encode as %XX
			char encoded[4];
			snprintf(encoded, sizeof(encoded), "%%%02X", (unsigned char)c);
			output << encoded;
		}
	}
}

status_t
AuthenticationService::ExchangeCodeForToken(const char* code)
{
	// Validate input
	if (!code || strlen(code) == 0) {
		return B_BAD_VALUE;
	}
	
	// Build POST data for token request
	BString postData;
	postData << "client_id=" << fClientId;
	postData << "&scope=";
	BString encodedScopes;
	UrlEncode(kScopes, encodedScopes);
	postData << encodedScopes;
	postData << "&code=" << code;
	postData << "&redirect_uri=";
	BString encodedRedirectUri;
	UrlEncode(kRedirectUri, encodedRedirectUri);
	postData << encodedRedirectUri;
	postData << "&grant_type=authorization_code";
	
	// For public clients (desktop apps), we don't need client_secret
	// Microsoft supports public client flows for desktop applications
	
	// This is a placeholder for the actual HTTP POST request
	// In a full implementation, we would:
	// 1. Create BHttpRequest for POST to kTokenEndpoint
	// 2. Set Content-Type: application/x-www-form-urlencoded
	// 3. Send postData as the body
	// 4. Get the JSON response
	
	// For now, simulate a response for testing
	BString mockResponse;
	mockResponse << "{";
	mockResponse << "\"token_type\":\"Bearer\",";
	mockResponse << "\"scope\":\"files.readwrite.all offline_access\",";
	mockResponse << "\"expires_in\":3600,";
	mockResponse << "\"access_token\":\"mock_access_token_" << system_time() << "\",";
	mockResponse << "\"refresh_token\":\"mock_refresh_token_" << system_time() << "\"";
	mockResponse << "}";
	
	// Parse the response
	status_t result = ParseTokenResponse(mockResponse);
	if (result != B_OK) {
		return result;
	}
	
	// Store tokens securely
	result = StoreTokens();
	if (result != B_OK) {
		// Clear tokens on storage failure
		fAccessToken = "";
		fRefreshToken = "";
		fExpiryTime = 0;
		return result;
	}
	
	// Send success notification
	if (fReplyTarget.IsValid()) {
		BMessage success(MSG_AUTH_SUCCESS);
		success.AddString("email", fAccountEmail.IsEmpty() ? "user@example.com" : fAccountEmail.String());
		success.AddInt64("expires", fExpiryTime);
		fReplyTarget.SendMessage(&success);
	}
	
	// Clear auth state
	fAuthInProgress = false;
	fAuthState = "";
	
	return B_OK;
}

status_t
AuthenticationService::ParseTokenResponse(const BString& response)
{
	// TODO: Implement response parsing
	return B_OK;
}

// Token storage (private)
status_t
AuthenticationService::StoreTokens()
{
	BKeyStore keyStore;
	BPasswordKey accessKey;
	BPasswordKey refreshKey;
	BPasswordKey expiryKey;
	BPasswordKey emailKey;
	
	// First, try to remove any existing keys to avoid duplicates
	DeleteTokens();
	
	// Store access token
	accessKey.SetTo(fAccessToken.String(), B_KEY_PURPOSE_GENERIC, "OneDrive_AccessToken");
	status_t result = keyStore.AddKey(accessKey);
	if (result != B_OK)
		return result;
	
	// Store refresh token
	refreshKey.SetTo(fRefreshToken.String(), B_KEY_PURPOSE_GENERIC, "OneDrive_RefreshToken");
	result = keyStore.AddKey(refreshKey);
	if (result != B_OK) {
		// Clean up access token if refresh token fails
		keyStore.RemoveKey(accessKey);
		return result;
	}
	
	// Store token expiry time as string
	BString expiryStr;
	expiryStr << fExpiryTime;
	expiryKey.SetTo(expiryStr.String(), B_KEY_PURPOSE_GENERIC, "OneDrive_TokenExpiry");
	result = keyStore.AddKey(expiryKey);
	if (result != B_OK) {
		// Clean up previously stored keys
		keyStore.RemoveKey(accessKey);
		keyStore.RemoveKey(refreshKey);
		return result;
	}
	
	// Store account email if available
	if (!fAccountEmail.IsEmpty()) {
		emailKey.SetTo(fAccountEmail.String(), B_KEY_PURPOSE_GENERIC, "OneDrive_AccountEmail");
		result = keyStore.AddKey(emailKey);
		if (result != B_OK) {
			// This is non-critical, don't fail the whole operation
			// Just log it (logging to be implemented)
		}
	}
	
	return B_OK;
}

status_t
AuthenticationService::LoadTokens()
{
	BKeyStore keyStore;
	BPasswordKey accessKey;
	BPasswordKey refreshKey;
	BPasswordKey expiryKey;
	BPasswordKey emailKey;
	
	// Load access token
	status_t result = keyStore.GetKey(B_KEY_TYPE_PASSWORD, "OneDrive_AccessToken", accessKey);
	if (result == B_OK) {
		fAccessToken = accessKey.Password();
	}
	
	// Load refresh token
	result = keyStore.GetKey(B_KEY_TYPE_PASSWORD, "OneDrive_RefreshToken", refreshKey);
	if (result == B_OK) {
		fRefreshToken = refreshKey.Password();
	}
	
	// Load token expiry time
	result = keyStore.GetKey(B_KEY_TYPE_PASSWORD, "OneDrive_TokenExpiry", expiryKey);
	if (result == B_OK) {
		// Convert string timestamp back to time_t
		fExpiryTime = (time_t)atol(expiryKey.Password());
	}
	
	// Load account email
	result = keyStore.GetKey(B_KEY_TYPE_PASSWORD, "OneDrive_AccountEmail", emailKey);
	if (result == B_OK) {
		fAccountEmail = emailKey.Password();
	}
	
	// Validate we have all required tokens
	if (!fAccessToken.IsEmpty() && !fRefreshToken.IsEmpty() && fExpiryTime > 0) {
		// Check if token is already expired
		time_t currentTime = time(NULL);
		if (fExpiryTime <= currentTime) {
			// Token is expired, will need refresh
			return AUTH_ERROR_EXPIRED;
		}
		return B_OK;
	}
	
	return AUTH_ERROR_NO_REFRESH_TOKEN;
}

status_t
AuthenticationService::DeleteTokens()
{
	BKeyStore keyStore;
	
	// Remove all stored authentication data
	BPasswordKey key;
	
	// Remove access token
	if (keyStore.GetKey(B_KEY_TYPE_PASSWORD, "OneDrive_AccessToken", key) == B_OK) {
		keyStore.RemoveKey(key);
	}
	
	// Remove refresh token
	if (keyStore.GetKey(B_KEY_TYPE_PASSWORD, "OneDrive_RefreshToken", key) == B_OK) {
		keyStore.RemoveKey(key);
	}
	
	// Remove token expiry
	if (keyStore.GetKey(B_KEY_TYPE_PASSWORD, "OneDrive_TokenExpiry", key) == B_OK) {
		keyStore.RemoveKey(key);
	}
	
	// Remove account email
	if (keyStore.GetKey(B_KEY_TYPE_PASSWORD, "OneDrive_AccountEmail", key) == B_OK) {
		keyStore.RemoveKey(key);
	}
	
	// Clear in-memory values
	fAccessToken = "";
	fRefreshToken = "";
	fAccountEmail = "";
	fExpiryTime = 0;
	
	return B_OK;
}

// HTTP server for redirect (private)
status_t
AuthenticationService::StartRedirectServer()
{
	// TODO: Implement redirect server
	return B_OK;
}

status_t
AuthenticationService::StopRedirectServer()
{
	// TODO: Implement server shutdown
	return B_OK;
}