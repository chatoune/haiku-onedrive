/**
 * @file AuthManager.h
 * @brief Authentication manager for OneDrive OAuth2 integration
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-14
 * 
 * This file contains the AuthenticationManager class which handles OAuth2
 * authentication with Microsoft OneDrive using Haiku's native BKeyStore API.
 * 
 * @note Current BKeyStore implementation has limited security (no encryption).
 *       This implementation is designed to work properly when BKeyStore security
 *       is improved while providing functional authentication storage today.
 */

#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <String.h>
#include <app/KeyStore.h>
#include <app/Key.h>
#include <Locker.h>
#include <Message.h>

/**
 * @brief OAuth2 authentication states
 */
enum AuthState {
    AUTH_STATE_NOT_AUTHENTICATED = 0,  ///< User not authenticated
    AUTH_STATE_AUTHENTICATING,         ///< Authentication in progress
    AUTH_STATE_AUTHENTICATED,          ///< Successfully authenticated
    AUTH_STATE_TOKEN_EXPIRED,          ///< Access token has expired
    AUTH_STATE_ERROR                   ///< Authentication error occurred
};

/**
 * @brief Microsoft Graph OAuth2 endpoints and configuration
 */
struct OAuth2Config {
    static const char* kAuthEndpoint;      ///< Authorization endpoint URL
    static const char* kTokenEndpoint;     ///< Token exchange endpoint URL
    static const char* kRedirectURI;       ///< OAuth2 redirect URI
    static const char* kScope;             ///< Requested permissions scope
    static const char* kClientId;          ///< Microsoft app client ID
};

/**
 * @brief Authentication manager for OneDrive OAuth2 integration
 * 
 * The AuthenticationManager class handles OAuth2 authentication flow with
 * Microsoft OneDrive using Haiku's native BKeyStore API for credential storage.
 * 
 * Key features:
 * - OAuth2 authorization code flow implementation
 * - Secure token storage using Haiku BKeyStore
 * - Automatic token refresh handling
 * - Thread-safe operation with proper locking
 * - Integration with Haiku's permission system
 * 
 * Security considerations:
 * - Uses Haiku's native keystore API (BKeyStore, BPasswordKey)
 * - Creates application-specific keyring for OneDrive credentials
 * - Implements proper access control and permission handling
 * - Designed for future BKeyStore security improvements
 * 
 * @note Current BKeyStore has no encryption. Use with awareness of security
 *       limitations until Haiku implements proper keystore encryption.
 * 
 * @see BKeyStore
 * @see BPasswordKey
 * @since 1.0.0
 */
class AuthenticationManager {
public:
    /**
     * @brief Default constructor
     * 
     * Initializes the authentication manager and sets up the keystore
     * connection. Creates application keyring if it doesn't exist.
     */
    AuthenticationManager();
    
    /**
     * @brief Destructor
     * 
     * Cleans up resources and ensures proper keystore cleanup.
     */
    virtual ~AuthenticationManager();
    
    // Authentication flow methods
    
    /**
     * @brief Start OAuth2 authentication flow
     * 
     * Initiates the OAuth2 authorization code flow by constructing the
     * authorization URL and launching the user's web browser.
     * 
     * @return B_OK on success, error code on failure
     * @retval B_OK Authorization URL opened successfully
     * @retval B_ERROR Failed to launch browser
     * @retval B_NO_MEMORY Insufficient memory
     * 
     * @see HandleAuthCallback()
     */
    status_t StartAuthFlow();
    
    /**
     * @brief Handle OAuth2 callback with authorization code
     * 
     * Processes the authorization code received from the OAuth2 callback
     * and exchanges it for access and refresh tokens.
     * 
     * @param authCode The authorization code from OAuth2 callback
     * @return B_OK on success, error code on failure
     * @retval B_OK Tokens obtained and stored successfully
     * @retval B_BAD_VALUE Invalid authorization code
     * @retval B_ERROR Network or API error
     * 
     * @see StartAuthFlow()
     */
    status_t HandleAuthCallback(const BString& authCode);
    
    /**
     * @brief Refresh access token using refresh token
     * 
     * Uses the stored refresh token to obtain a new access token
     * when the current one has expired.
     * 
     * @return B_OK on success, error code on failure
     * @retval B_OK Token refreshed successfully
     * @retval B_NOT_ALLOWED No valid refresh token available
     * @retval B_ERROR Network or API error
     */
    status_t RefreshToken();
    
    /**
     * @brief Logout and clear all stored credentials
     * 
     * Removes all stored tokens from the keystore and resets
     * authentication state.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t Logout();
    
    // Token access methods
    
    /**
     * @brief Get current access token
     * 
     * Retrieves the current access token for API requests. Automatically
     * attempts token refresh if the token has expired.
     * 
     * @param token Reference to store the access token
     * @return B_OK on success, error code on failure
     * @retval B_OK Valid token retrieved
     * @retval B_NOT_ALLOWED No valid authentication
     * @retval B_ERROR Token refresh failed
     */
    status_t GetAccessToken(BString& token);
    
    /**
     * @brief Check if user is currently authenticated
     * 
     * @return true if authenticated with valid tokens, false otherwise
     */
    bool IsAuthenticated() const;
    
    /**
     * @brief Get current authentication state
     * 
     * @return Current AuthState value
     * @see AuthState
     */
    AuthState GetAuthState() const;
    
    /**
     * @brief Get authenticated user information
     * 
     * Retrieves basic user profile information from stored authentication.
     * 
     * @param userInfo BMessage to store user information
     * @return B_OK on success, error code on failure
     */
    status_t GetUserInfo(BMessage& userInfo);
    
    /**
     * @brief Get last error message
     * 
     * @return Human-readable error message from last operation
     */
    BString GetLastError() const;
    
    // Settings and configuration
    
    /**
     * @brief Set OAuth2 client configuration
     * 
     * Updates the OAuth2 client ID and other configuration parameters.
     * 
     * @param clientId Microsoft application client ID
     * @return B_OK on success, error code on failure
     */
    status_t SetClientId(const BString& clientId);
    
    /**
     * @brief Get OAuth2 authorization URL
     * 
     * Constructs the authorization URL for the OAuth2 flow.
     * 
     * @param url Reference to store the authorization URL
     * @return B_OK on success, error code on failure
     */
    status_t GetAuthorizationURL(BString& url);

private:
    /// @name Keystore Integration
    /// @{
    
    /**
     * @brief Initialize keystore connection
     * 
     * Sets up the BKeyStore connection and creates application keyring.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t _InitializeKeystore();
    
    /**
     * @brief Store token in keystore
     * 
     * Stores a token (access or refresh) in the Haiku keystore using
     * BPasswordKey for secure storage.
     * 
     * @param tokenType Type identifier ("access_token" or "refresh_token")
     * @param tokenValue The token value to store
     * @return B_OK on success, error code on failure
     */
    status_t _StoreToken(const char* tokenType, const BString& tokenValue);
    
    /**
     * @brief Retrieve token from keystore
     * 
     * Retrieves a stored token from the Haiku keystore.
     * 
     * @param tokenType Type identifier ("access_token" or "refresh_token") 
     * @param tokenValue Reference to store retrieved token
     * @return B_OK on success, error code on failure
     */
    status_t _RetrieveToken(const char* tokenType, BString& tokenValue) const;
    
    /**
     * @brief Remove token from keystore
     * 
     * Deletes a specific token from the keystore.
     * 
     * @param tokenType Type identifier to remove
     * @return B_OK on success, error code on failure
     */
    status_t _RemoveToken(const char* tokenType);
    
    /**
     * @brief Clear all stored tokens
     * 
     * Removes all OneDrive-related tokens from the keystore.
     * 
     * @return B_OK on success, error code on failure
     */
    status_t _ClearAllTokens();
    
    /// @}
    
    /// @name OAuth2 Implementation
    /// @{
    
    /**
     * @brief Exchange authorization code for tokens
     * 
     * Makes HTTP request to token endpoint to exchange auth code for tokens.
     * 
     * @param authCode Authorization code from OAuth2 callback
     * @param accessToken Reference to store access token
     * @param refreshToken Reference to store refresh token
     * @param expiresIn Reference to store token expiration time
     * @return B_OK on success, error code on failure
     */
    status_t _ExchangeCodeForTokens(const BString& authCode,
                                   BString& accessToken,
                                   BString& refreshToken,
                                   int32& expiresIn);
    
    /**
     * @brief Refresh access token via HTTP request
     * 
     * Makes HTTP request to refresh endpoint to get new access token.
     * 
     * @param refreshToken Current refresh token
     * @param newAccessToken Reference to store new access token
     * @param newRefreshToken Reference to store new refresh token (may be same)
     * @param expiresIn Reference to store new expiration time
     * @return B_OK on success, error code on failure
     */
    status_t _RefreshAccessToken(const BString& refreshToken,
                                BString& newAccessToken,
                                BString& newRefreshToken,
                                int32& expiresIn);
    
    /**
     * @brief Check if access token has expired
     * 
     * @return true if token is expired, false if still valid
     */
    bool _IsTokenExpired() const;
    
    /**
     * @brief Update authentication state
     * 
     * Updates internal state and notifies observers of changes.
     * 
     * @param newState New authentication state
     */
    void _UpdateAuthState(AuthState newState);
    
    /**
     * @brief Parse JSON token response from OAuth2 endpoints
     * 
     * Extracts access token, refresh token, and expiration from JSON response.
     * 
     * @param jsonResponse Raw JSON response string
     * @param accessToken Reference to store extracted access token
     * @param refreshToken Reference to store extracted refresh token
     * @param expiresIn Reference to store token expiration time in seconds
     * @return B_OK on success, error code on failure
     */
    status_t _ParseTokenResponse(const BString& jsonResponse,
                                BString& accessToken,
                                BString& refreshToken,
                                int32& expiresIn);
    
    /// @}
    
    /// @name Member Variables
    /// @{
    
    BKeyStore           fKeyStore;          ///< Haiku keystore interface
    mutable BLocker     fLock;              ///< Thread safety lock
    
    AuthState           fAuthState;         ///< Current authentication state
    BString             fClientId;          ///< OAuth2 client ID
    time_t              fTokenExpiry;       ///< Access token expiration time
    BString             fUserId;            ///< Authenticated user ID
    BString             fLastError;         ///< Last error message
    
    /// @}
    
    /// @name Static Constants
    /// @{
    
    static const char* kKeystoreKeyring;     ///< Keyring name for OneDrive credentials
    static const char* kAccessTokenKey;      ///< Key identifier for access token
    static const char* kRefreshTokenKey;     ///< Key identifier for refresh token
    static const char* kUserInfoKey;         ///< Key identifier for user information
    static const char* kExpiryTimeKey;       ///< Key identifier for token expiry
    
    /// @}
};

#endif // AUTH_MANAGER_H