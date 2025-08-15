/**
 * @file AuthManager.cpp
 * @brief Implementation of AuthenticationManager for OneDrive OAuth2
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 */

#include "AuthManager.h"
#include "../shared/OneDriveConstants.h"
#include <Application.h>
#include <Roster.h>
#include <Url.h>
#include <NetworkKit.h>
#include <Autolock.h>
#include <syslog.h>
#include <stdio.h>
#include <time.h>
#include <DataIO.h>
#include <String.h>

// OAuth2 Configuration constants
const char* OAuth2Config::kAuthEndpoint = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize";
const char* OAuth2Config::kTokenEndpoint = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
const char* OAuth2Config::kRedirectURI = "https://login.live.com/oauth20_desktop.srf";
const char* OAuth2Config::kScope = "Files.ReadWrite.All offline_access";
const char* OAuth2Config::kClientId = ""; // To be configured by application

// AuthenticationManager static constants
const char* AuthenticationManager::kKeystoreKeyring = "OneDrive";
const char* AuthenticationManager::kAccessTokenKey = "access_token";
const char* AuthenticationManager::kRefreshTokenKey = "refresh_token";
const char* AuthenticationManager::kUserInfoKey = "user_info";
const char* AuthenticationManager::kExpiryTimeKey = "token_expiry";

AuthenticationManager::AuthenticationManager()
    : fLock("AuthManager Lock"),
      fAuthState(AUTH_STATE_NOT_AUTHENTICATED),
      fTokenExpiry(0)
{
    syslog(LOG_INFO, "OneDrive AuthenticationManager initializing");
    
    // Initialize keystore connection
    status_t result = _InitializeKeystore();
    if (result != B_OK) {
        syslog(LOG_ERR, "OneDrive AuthManager: Failed to initialize keystore: %s", 
               strerror(result));
        fAuthState = AUTH_STATE_ERROR;
        return;
    }
    
    // Check if we have existing valid authentication
    BString accessToken;
    if (_RetrieveToken(kAccessTokenKey, accessToken) == B_OK && accessToken.Length() > 0) {
        if (!_IsTokenExpired()) {
            fAuthState = AUTH_STATE_AUTHENTICATED;
            syslog(LOG_INFO, "OneDrive AuthManager: Found existing valid authentication");
        } else {
            fAuthState = AUTH_STATE_TOKEN_EXPIRED;
            syslog(LOG_INFO, "OneDrive AuthManager: Found expired authentication");
        }
    }
    
    syslog(LOG_INFO, "OneDrive AuthenticationManager initialized successfully");
}

AuthenticationManager::~AuthenticationManager()
{
    BAutolock lock(fLock);
    syslog(LOG_INFO, "OneDrive AuthenticationManager shutting down");
}

status_t
AuthenticationManager::StartAuthFlow()
{
    BAutolock lock(fLock);
    
    if (fClientId.IsEmpty()) {
        fLastError = "Client ID not configured. Please configure OAuth2 client ID before authentication.";
        syslog(LOG_ERR, "OneDrive AuthManager: Client ID not configured");
        return B_ERROR;
    }
    
    _UpdateAuthState(AUTH_STATE_AUTHENTICATING);
    
    // Construct OAuth2 authorization URL
    BString authUrl;
    status_t result = GetAuthorizationURL(authUrl);
    if (result != B_OK) {
        fLastError = "Failed to construct OAuth2 authorization URL";
        syslog(LOG_ERR, "OneDrive AuthManager: Failed to construct auth URL");
        _UpdateAuthState(AUTH_STATE_ERROR);
        return result;
    }
    
    // Launch web browser with authorization URL
    // Note: Using Haiku's native roster to launch browser
    BUrl url(authUrl.String(), false);
    const char* urlCStr = authUrl.String();
    be_roster->Launch("application/x-vnd.Be-NPOS", 1, const_cast<char**>(&urlCStr));
    
    fLastError = ""; // Clear error on success
    syslog(LOG_INFO, "OneDrive AuthManager: Started OAuth2 flow, browser launched");
    return B_OK;
}

status_t
AuthenticationManager::HandleAuthCallback(const BString& authCode)
{
    BAutolock lock(fLock);
    
    if (authCode.IsEmpty()) {
        syslog(LOG_ERR, "OneDrive AuthManager: Empty authorization code received");
        _UpdateAuthState(AUTH_STATE_ERROR);
        return B_BAD_VALUE;
    }
    
    syslog(LOG_INFO, "OneDrive AuthManager: Processing authorization callback");
    
    BString accessToken, refreshToken;
    int32 expiresIn;
    
    // Exchange authorization code for tokens
    status_t result = _ExchangeCodeForTokens(authCode, accessToken, refreshToken, expiresIn);
    if (result != B_OK) {
        syslog(LOG_ERR, "OneDrive AuthManager: Failed to exchange code for tokens");
        _UpdateAuthState(AUTH_STATE_ERROR);
        return result;
    }
    
    // Store tokens in keystore
    result = _StoreToken(kAccessTokenKey, accessToken);
    if (result != B_OK) {
        syslog(LOG_ERR, "OneDrive AuthManager: Failed to store access token");
        _UpdateAuthState(AUTH_STATE_ERROR);
        return result;
    }
    
    result = _StoreToken(kRefreshTokenKey, refreshToken);
    if (result != B_OK) {
        syslog(LOG_ERR, "OneDrive AuthManager: Failed to store refresh token");
        _UpdateAuthState(AUTH_STATE_ERROR);
        return result;
    }
    
    // Calculate and store expiry time
    fTokenExpiry = time(NULL) + expiresIn - 60; // 60 second buffer
    BString expiryStr;
    expiryStr << fTokenExpiry;
    _StoreToken(kExpiryTimeKey, expiryStr);
    
    _UpdateAuthState(AUTH_STATE_AUTHENTICATED);
    syslog(LOG_INFO, "OneDrive AuthManager: Authentication successful");
    
    return B_OK;
}

status_t
AuthenticationManager::RefreshToken()
{
    BAutolock lock(fLock);
    
    BString refreshToken;
    status_t result = _RetrieveToken(kRefreshTokenKey, refreshToken);
    if (result != B_OK || refreshToken.IsEmpty()) {
        syslog(LOG_ERR, "OneDrive AuthManager: No refresh token available");
        _UpdateAuthState(AUTH_STATE_NOT_AUTHENTICATED);
        return B_NOT_ALLOWED;
    }
    
    syslog(LOG_INFO, "OneDrive AuthManager: Refreshing access token");
    
    BString newAccessToken, newRefreshToken;
    int32 expiresIn;
    
    result = _RefreshAccessToken(refreshToken, newAccessToken, newRefreshToken, expiresIn);
    if (result != B_OK) {
        syslog(LOG_ERR, "OneDrive AuthManager: Token refresh failed");
        _UpdateAuthState(AUTH_STATE_ERROR);
        return result;
    }
    
    // Store updated tokens
    _StoreToken(kAccessTokenKey, newAccessToken);
    if (!newRefreshToken.IsEmpty()) {
        _StoreToken(kRefreshTokenKey, newRefreshToken);
    }
    
    // Update expiry time
    fTokenExpiry = time(NULL) + expiresIn - 60; // 60 second buffer
    BString expiryStr;
    expiryStr << fTokenExpiry;
    _StoreToken(kExpiryTimeKey, expiryStr);
    
    _UpdateAuthState(AUTH_STATE_AUTHENTICATED);
    syslog(LOG_INFO, "OneDrive AuthManager: Token refresh successful");
    
    return B_OK;
}

status_t
AuthenticationManager::Logout()
{
    BAutolock lock(fLock);
    
    syslog(LOG_INFO, "OneDrive AuthManager: Logging out");
    
    status_t result = _ClearAllTokens();
    if (result != B_OK) {
        syslog(LOG_WARNING, "OneDrive AuthManager: Error clearing tokens during logout");
    }
    
    fTokenExpiry = 0;
    fUserId.SetTo("");
    _UpdateAuthState(AUTH_STATE_NOT_AUTHENTICATED);
    
    return B_OK;
}

status_t
AuthenticationManager::GetAccessToken(BString& token)
{
    BAutolock lock(fLock);
    
    // Check if we need to refresh the token
    if (_IsTokenExpired()) {
        syslog(LOG_INFO, "OneDrive AuthManager: Token expired, attempting refresh");
        status_t result = RefreshToken();
        if (result != B_OK) {
            return result;
        }
    }
    
    return _RetrieveToken(kAccessTokenKey, token);
}

bool
AuthenticationManager::IsAuthenticated() const
{
    BAutolock lock(fLock);
    return (fAuthState == AUTH_STATE_AUTHENTICATED || fAuthState == AUTH_STATE_TOKEN_EXPIRED);
}

AuthState
AuthenticationManager::GetAuthState() const
{
    BAutolock lock(fLock);
    return fAuthState;
}

BString
AuthenticationManager::GetLastError() const
{
    BAutolock lock(fLock);
    return fLastError;
}

status_t
AuthenticationManager::GetUserInfo(BMessage& userInfo)
{
    BAutolock lock(fLock);
    
    if (!IsAuthenticated()) {
        return B_NOT_ALLOWED;
    }
    
    BString userInfoStr;
    status_t result = _RetrieveToken(kUserInfoKey, userInfoStr);
    if (result != B_OK) {
        return result;
    }
    
    // Parse stored user info JSON (simplified implementation)
    userInfo.MakeEmpty();
    userInfo.AddString("user_id", fUserId);
    // TODO: Parse full user info from stored JSON
    
    return B_OK;
}

status_t
AuthenticationManager::SetClientId(const BString& clientId)
{
    BAutolock lock(fLock);
    
    if (clientId.IsEmpty()) {
        return B_BAD_VALUE;
    }
    
    fClientId = clientId;
    syslog(LOG_INFO, "OneDrive AuthManager: Client ID configured");
    return B_OK;
}

status_t
AuthenticationManager::GetAuthorizationURL(BString& url)
{
    if (fClientId.IsEmpty()) {
        return B_ERROR;
    }
    
    // Construct OAuth2 authorization URL
    url = OAuth2Config::kAuthEndpoint;
    url << "?client_id=" << fClientId;
    url << "&response_type=code";
    url << "&redirect_uri=" << OAuth2Config::kRedirectURI;
    url << "&scope=" << OAuth2Config::kScope;
    url << "&response_mode=query";
    
    return B_OK;
}

// Private implementation methods

status_t
AuthenticationManager::_InitializeKeystore()
{
    // Note: BKeyStore will prompt user for permissions as needed
    // This follows Haiku's security model for keystore access
    
    // Check if our keyring exists, create if needed
    BPasswordKey testKey;
    status_t result = fKeyStore.GetKey(B_KEY_TYPE_PASSWORD, 
                                      kKeystoreKeyring,
                                      kAccessTokenKey,
                                      testKey);
    
    if (result == B_ENTRY_NOT_FOUND) {
        // Keyring doesn't exist, which is normal for first run
        syslog(LOG_INFO, "OneDrive AuthManager: Creating keyring for first use");
    }
    
    return B_OK; // BKeyStore handles keyring creation automatically
}

status_t
AuthenticationManager::_StoreToken(const char* tokenType, const BString& tokenValue)
{
    if (!tokenType || tokenValue.IsEmpty()) {
        return B_BAD_VALUE;
    }
    
    // Use BPasswordKey for string token storage
    BPasswordKey key(tokenValue.String(), 
                    B_KEY_PURPOSE_WEB,
                    kKeystoreKeyring,    // Primary identifier (our keyring)
                    tokenType);          // Secondary identifier (token type)
    
    status_t result = fKeyStore.AddKey(key);
    if (result == B_FILE_EXISTS) {
        // Key already exists, remove and re-add with new value
        _RemoveToken(tokenType);
        result = fKeyStore.AddKey(key);
    }
    
    if (result != B_OK) {
        syslog(LOG_ERR, "OneDrive AuthManager: Failed to store %s: %s", 
               tokenType, strerror(result));
    }
    
    return result;
}

status_t
AuthenticationManager::_RetrieveToken(const char* tokenType, BString& tokenValue) const
{
    if (!tokenType) {
        return B_BAD_VALUE;
    }
    
    BPasswordKey key;
    status_t result = const_cast<BKeyStore&>(fKeyStore).GetKey(B_KEY_TYPE_PASSWORD,
                                      kKeystoreKeyring,    // Primary identifier
                                      tokenType,           // Secondary identifier  
                                      key);
    
    if (result == B_OK) {
        tokenValue = key.Password();
    } else {
        tokenValue.SetTo("");
        if (result != B_ENTRY_NOT_FOUND) {
            syslog(LOG_ERR, "OneDrive AuthManager: Failed to retrieve %s: %s",
                   tokenType, strerror(result));
        }
    }
    
    return result;
}

status_t
AuthenticationManager::_RemoveToken(const char* tokenType)
{
    if (!tokenType) {
        return B_BAD_VALUE;
    }
    
    BPasswordKey key;
    status_t result = fKeyStore.GetKey(B_KEY_TYPE_PASSWORD,
                                      kKeystoreKeyring,
                                      tokenType,
                                      key);
    
    if (result == B_OK) {
        result = fKeyStore.RemoveKey(key);
    }
    
    return result;
}

status_t
AuthenticationManager::_ClearAllTokens()
{
    status_t result = B_OK;
    
    // Remove all token types
    const char* tokenTypes[] = {
        kAccessTokenKey,
        kRefreshTokenKey, 
        kUserInfoKey,
        kExpiryTimeKey,
        nullptr
    };
    
    for (int i = 0; tokenTypes[i] != nullptr; i++) {
        status_t removeResult = _RemoveToken(tokenTypes[i]);
        if (removeResult != B_OK && removeResult != B_ENTRY_NOT_FOUND) {
            result = removeResult; // Return last error
        }
    }
    
    return result;
}

status_t
AuthenticationManager::_ExchangeCodeForTokens(const BString& authCode,
                                              BString& accessToken,
                                              BString& refreshToken,
                                              int32& expiresIn)
{
    syslog(LOG_INFO, "OneDrive AuthManager: Exchanging auth code for tokens");
    
    if (fClientId.IsEmpty()) {
        syslog(LOG_ERR, "OneDrive AuthManager: Client ID not configured for token exchange");
        return B_ERROR;
    }
    
    // TODO: Implement actual HTTP POST request to OAuth2Config::kTokenEndpoint
    // This requires proper HTTP client implementation using Haiku's networking APIs
    // For now, return success with placeholder tokens for development
    
    syslog(LOG_INFO, "OneDrive AuthManager: Token exchange (DEVELOPMENT MODE)");
    
    // Development placeholder tokens
    accessToken = "dev_access_token_";
    accessToken << system_time(); // Add timestamp for uniqueness
    
    refreshToken = "dev_refresh_token_";
    refreshToken << system_time();
    
    expiresIn = 3600; // 1 hour
    
    syslog(LOG_INFO, "OneDrive AuthManager: Generated development tokens");
    return B_OK;
}

status_t
AuthenticationManager::_RefreshAccessToken(const BString& refreshToken,
                                          BString& newAccessToken,
                                          BString& newRefreshToken,
                                          int32& expiresIn)
{
    syslog(LOG_INFO, "OneDrive AuthManager: Refreshing access token");
    
    if (fClientId.IsEmpty()) {
        syslog(LOG_ERR, "OneDrive AuthManager: Client ID not configured for token refresh");
        return B_ERROR;
    }
    
    // TODO: Implement actual HTTP POST request to refresh endpoint
    // This requires proper HTTP client implementation using Haiku's networking APIs
    // For now, return success with new placeholder tokens for development
    
    syslog(LOG_INFO, "OneDrive AuthManager: Token refresh (DEVELOPMENT MODE)");
    
    // Generate new development tokens
    newAccessToken = "dev_refreshed_access_token_";
    newAccessToken << system_time();
    
    newRefreshToken = refreshToken; // Refresh token often stays the same
    expiresIn = 3600; // 1 hour
    
    syslog(LOG_INFO, "OneDrive AuthManager: Generated refreshed development tokens");
    return B_OK;
}

bool
AuthenticationManager::_IsTokenExpired() const
{
    if (fTokenExpiry == 0) {
        // No expiry time stored, try to retrieve it
        BString expiryStr;
        if (_RetrieveToken(kExpiryTimeKey, expiryStr) == B_OK) {
            const_cast<AuthenticationManager*>(this)->fTokenExpiry = atol(expiryStr.String());
        }
    }
    
    return (fTokenExpiry != 0 && time(NULL) >= fTokenExpiry);
}

void
AuthenticationManager::_UpdateAuthState(AuthState newState)
{
    if (fAuthState != newState) {
        AuthState oldState = fAuthState;
        fAuthState = newState;
        
        syslog(LOG_INFO, "OneDrive AuthManager: State changed from %d to %d", 
               (int)oldState, (int)newState);
        
        // TODO: Notify observers of state change
        // This could be done via BMessage broadcasting or callbacks
    }
}

status_t
AuthenticationManager::_ParseTokenResponse(const BString& jsonResponse,
                                          BString& accessToken,
                                          BString& refreshToken,
                                          int32& expiresIn)
{
    if (jsonResponse.IsEmpty()) {
        syslog(LOG_ERR, "OneDrive AuthManager: Empty JSON response");
        return B_BAD_VALUE;
    }
    
    syslog(LOG_DEBUG, "OneDrive AuthManager: Parsing token response");
    
    // Simple JSON parsing for OAuth2 token response
    // Using basic string parsing since Haiku's JsonWriter is for output only
    
    // Extract access_token
    int32 accessStart = jsonResponse.FindFirst("\"access_token\":");
    if (accessStart >= 0) {
        accessStart = jsonResponse.FindFirst("\"", accessStart + 15); // Skip past "access_token":
        if (accessStart >= 0) {
            accessStart++; // Skip opening quote
            int32 accessEnd = jsonResponse.FindFirst("\"", accessStart);
            if (accessEnd > accessStart) {
                jsonResponse.CopyInto(accessToken, accessStart, accessEnd - accessStart);
            }
        }
    }
    
    // Extract refresh_token (optional)
    int32 refreshStart = jsonResponse.FindFirst("\"refresh_token\":");
    if (refreshStart >= 0) {
        refreshStart = jsonResponse.FindFirst("\"", refreshStart + 16); // Skip past "refresh_token":
        if (refreshStart >= 0) {
            refreshStart++; // Skip opening quote
            int32 refreshEnd = jsonResponse.FindFirst("\"", refreshStart);
            if (refreshEnd > refreshStart) {
                jsonResponse.CopyInto(refreshToken, refreshStart, refreshEnd - refreshStart);
            }
        }
    }
    
    // Extract expires_in
    int32 expiresStart = jsonResponse.FindFirst("\"expires_in\":");
    if (expiresStart >= 0) {
        expiresStart = jsonResponse.FindFirst(":", expiresStart) + 1;
        // Skip whitespace and potential quotes
        while (expiresStart < jsonResponse.Length() && 
               (jsonResponse[expiresStart] == ' ' || jsonResponse[expiresStart] == '\"')) {
            expiresStart++;
        }
        
        if (expiresStart < jsonResponse.Length()) {
            BString expiresStr;
            int32 pos = expiresStart;
            while (pos < jsonResponse.Length() && 
                   jsonResponse[pos] >= '0' && jsonResponse[pos] <= '9') {
                expiresStr += jsonResponse[pos];
                pos++;
            }
            expiresIn = atoi(expiresStr.String());
        }
    }
    
    // If no expires_in found, use default
    if (expiresIn <= 0) {
        syslog(LOG_WARNING, "OneDrive AuthManager: No expires_in in response, using default");
        expiresIn = 3600; // Default to 1 hour
    }
    
    // Validate results
    if (accessToken.IsEmpty()) {
        syslog(LOG_ERR, "OneDrive AuthManager: No access token extracted");
        return B_ERROR;
    }
    
    if (expiresIn <= 0) {
        syslog(LOG_WARNING, "OneDrive AuthManager: Invalid expiration time, using default");
        expiresIn = 3600; // Default to 1 hour
    }
    
    syslog(LOG_INFO, "OneDrive AuthManager: Successfully parsed token response");
    return B_OK;
}