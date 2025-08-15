# OneDrive Preferences GUI Test Cases

## Test Environment Setup

### Prerequisites
- Haiku R1/beta4 or later
- OneDrive Preferences application built and available at: `build/src/preferences/OneDrive Preferences`
- OneDrive daemon built and available at: `build/src/daemon/onedrive_daemon`
- Internet connection (for authentication testing)
- Microsoft OneDrive account (for authentication testing)

### Test Data Preparation
- Valid Microsoft account credentials
- Test folder path: `/boot/home/TestOneDrive`
- Test client ID: `test-client-12345` (for development testing)

---

## Test Suite 1: Application Launch and Basic UI

### TC001: Application Launch
**Objective**: Verify the preferences application launches successfully
**Priority**: High

**Steps**:
1. Navigate to the build directory: `cd build/src/preferences`
2. Launch the application: `./OneDrive\ Preferences`
3. Verify the application window opens

**Expected Results**:
- Application window opens within 3 seconds
- Window title shows "OneDrive Preferences"
- Window size is approximately 500x400 pixels
- Window is centered on screen

**Pass Criteria**: Application launches without errors and UI appears correctly

---

### TC002: Tab Navigation
**Objective**: Verify tab navigation works correctly
**Priority**: High

**Steps**:
1. Launch OneDrive Preferences
2. Verify three tabs are visible: "Account", "Sync", "Advanced"
3. Click on each tab in sequence
4. Verify tab content changes appropriately
5. Use keyboard navigation (Ctrl+Tab) to switch tabs

**Expected Results**:
- All three tabs are visible and clickable
- Tab content updates when switching tabs
- Active tab is visually highlighted
- Keyboard navigation works
- Tab labels are clear and readable

**Pass Criteria**: All tabs are accessible and display correct content

---

### TC003: Window Controls
**Objective**: Verify window controls function correctly
**Priority**: Medium

**Steps**:
1. Launch OneDrive Preferences
2. Test minimize button
3. Test close button
4. Relaunch and test window resizing
5. Test window moving

**Expected Results**:
- Minimize button minimizes window to Deskbar
- Close button closes application cleanly
- Window can be resized within reasonable limits
- Window can be moved around the desktop
- All controls are responsive

**Pass Criteria**: All window controls work as expected

---

## Test Suite 2: Account Tab Testing

### TC101: Account Tab Layout
**Objective**: Verify Account tab layout and controls
**Priority**: High

**Steps**:
1. Launch OneDrive Preferences
2. Ensure "Account" tab is selected
3. Verify presence of all UI elements:
   - Account status text
   - "Sign In" or "Sign Out" button
   - User information display area
   - "Test Connection" button

**Expected Results**:
- All UI elements are visible and properly aligned
- Text is readable and not truncated
- Buttons are appropriately sized
- Account status shows "Not connected" initially

**Pass Criteria**: UI layout matches design specification

---

### TC102: Sign In Process
**Objective**: Test OAuth2 authentication workflow
**Priority**: High

**Steps**:
1. Ensure OneDrive daemon is running: `./onedrive_daemon --foreground`
2. In Preferences, go to Account tab
3. Verify initial state shows "Not connected"
4. Click "Sign In" button
5. Verify browser opens with Microsoft OAuth page
6. Complete authentication in browser
7. Return to Preferences application
8. Verify account status updates

**Expected Results**:
- "Sign In" button is clickable when not authenticated
- Browser opens automatically to Microsoft OAuth page
- OAuth page loads correctly
- After authentication, status updates to "Connected"
- User information (name, email) appears
- "Sign In" button changes to "Sign Out"

**Pass Criteria**: Complete authentication workflow succeeds

---

### TC103: User Information Display
**Objective**: Verify authenticated user information display
**Priority**: Medium

**Steps**:
1. Complete sign in process (TC102)
2. Verify user information display shows:
   - Display name
   - Email address
   - Account type
   - Last sync time (if available)

**Expected Results**:
- User display name is shown correctly
- Email address is formatted properly
- Account information is accurate
- Text is properly formatted and readable

**Pass Criteria**: All user information displays correctly

---

### TC104: Test Connection
**Objective**: Test connection verification functionality
**Priority**: Medium

**Steps**:
1. Ensure user is signed in (TC102)
2. Click "Test Connection" button
3. Wait for operation to complete
4. Verify result display

**Expected Results**:
- "Test Connection" button becomes temporarily disabled
- Progress indicator or status message appears
- Connection test completes within 10 seconds
- Success/failure message is displayed clearly
- Button re-enables after test

**Pass Criteria**: Connection test provides clear feedback

---

### TC105: Sign Out Process
**Objective**: Test logout functionality
**Priority**: High

**Steps**:
1. Ensure user is signed in (TC102)
2. Click "Sign Out" button
3. Confirm logout in dialog (if present)
4. Verify UI updates

**Expected Results**:
- "Sign Out" button is clickable when authenticated
- Confirmation dialog may appear
- After logout, status changes to "Not connected"
- User information is cleared
- "Sign Out" button changes to "Sign In"

**Pass Criteria**: Logout process completes successfully

---

## Test Suite 3: Sync Tab Testing

### TC201: Sync Tab Layout
**Objective**: Verify Sync tab layout and controls
**Priority**: High

**Steps**:
1. Navigate to "Sync" tab
2. Verify presence of all UI elements:
   - Sync folder path display/selector
   - "Browse" button for folder selection
   - "Enable Sync" checkbox
   - "Sync Hidden Files" checkbox
   - "Sync Attributes" checkbox
   - Sync status display

**Expected Results**:
- All controls are visible and properly aligned
- Default sync folder shows "/boot/home/OneDrive"
- Checkboxes have appropriate labels
- Status area shows current sync state

**Pass Criteria**: UI layout is complete and functional

---

### TC202: Folder Selection
**Objective**: Test sync folder selection functionality
**Priority**: High

**Steps**:
1. Navigate to "Sync" tab
2. Note current sync folder path
3. Click "Browse" button
4. Navigate to a different folder in file browser
5. Select "/boot/home/TestOneDrive"
6. Click "Select" or "OK"
7. Verify path updates in preferences

**Expected Results**:
- "Browse" button opens file browser
- File browser allows folder navigation
- Only folders can be selected (not files)
- Selected path updates in preferences UI
- Path is stored and persists

**Pass Criteria**: Folder selection works correctly

---

### TC203: Sync Options Configuration
**Objective**: Test sync option checkboxes
**Priority**: Medium

**Steps**:
1. Navigate to "Sync" tab
2. Test each checkbox:
   - Toggle "Enable Sync" checkbox
   - Toggle "Sync Hidden Files" checkbox
   - Toggle "Sync Attributes" checkbox
3. Verify each setting is saved
4. Close and reopen preferences
5. Verify settings persist

**Expected Results**:
- All checkboxes are clickable and responsive
- Checkbox states change visually when clicked
- Settings are saved automatically or on apply
- Settings persist across application restarts

**Pass Criteria**: All sync options work and persist

---

### TC204: Sync Status Display
**Objective**: Verify sync status information
**Priority**: Medium

**Steps**:
1. Navigate to "Sync" tab
2. If authenticated and sync enabled, verify status shows:
   - Current sync state (idle, syncing, error)
   - Last sync time
   - Number of files in sync queue
   - Sync progress (if active)

**Expected Results**:
- Status information is accurate and current
- Text is clear and informative
- Status updates automatically
- Progress information is meaningful

**Pass Criteria**: Status display provides useful information

---

## Test Suite 4: Advanced Tab Testing

### TC301: Advanced Tab Layout
**Objective**: Verify Advanced tab layout and controls
**Priority**: Medium

**Steps**:
1. Navigate to "Advanced" tab
2. Verify presence of all UI elements:
   - Bandwidth limiting controls
   - "WiFi Only" checkbox
   - "Start at Boot" checkbox
   - "Show Notifications" checkbox
   - Log level selector
   - "Reset Settings" button

**Expected Results**:
- All controls are visible and accessible
- Bandwidth controls show current limits
- Checkboxes show current states
- All text is readable

**Pass Criteria**: Advanced tab UI is complete

---

### TC302: Bandwidth Limiting
**Objective**: Test bandwidth limiting controls
**Priority**: Medium

**Steps**:
1. Navigate to "Advanced" tab
2. Locate bandwidth limiting controls
3. Test upload limit slider/spinner
4. Test download limit slider/spinner
5. Try setting various values (0, 100, 1000 KB/s)
6. Verify "Unlimited" option works

**Expected Results**:
- Controls respond to user input
- Values update correctly in UI
- Reasonable limits are enforced
- "Unlimited" (0) option is available
- Settings are saved

**Pass Criteria**: Bandwidth controls function correctly

---

### TC303: Advanced Options
**Objective**: Test advanced option checkboxes
**Priority**: Low

**Steps**:
1. Navigate to "Advanced" tab
2. Test each checkbox:
   - Toggle "WiFi Only" sync
   - Toggle "Start at Boot"
   - Toggle "Show Notifications"
3. Verify settings are saved

**Expected Results**:
- All checkboxes are functional
- Settings persist across restarts
- "Start at Boot" affects system startup
- Notifications setting affects behavior

**Pass Criteria**: Advanced options work as expected

---

### TC304: Reset Settings
**Objective**: Test settings reset functionality
**Priority**: Medium

**Steps**:
1. Configure various settings across all tabs
2. Navigate to "Advanced" tab
3. Click "Reset Settings" button
4. Confirm reset in dialog
5. Verify all settings return to defaults

**Expected Results**:
- "Reset Settings" button is clickable
- Confirmation dialog appears
- All settings reset to default values
- UI updates to show defaults
- User must re-authenticate

**Pass Criteria**: Settings reset works completely

---

## Test Suite 5: Integration and Error Handling

### TC401: Daemon Communication
**Objective**: Test preferences communication with daemon
**Priority**: High

**Steps**:
1. Start daemon: `./onedrive_daemon --foreground`
2. Launch preferences application
3. Perform various operations:
   - Sign in/out
   - Change sync settings
   - Start/stop sync
4. Verify daemon responds appropriately

**Expected Results**:
- Preferences can communicate with daemon
- Settings changes are reflected in daemon
- Authentication state is synchronized
- Operations complete successfully

**Pass Criteria**: Preferences and daemon work together

---

### TC402: No Daemon Error Handling
**Objective**: Test behavior when daemon is not running
**Priority**: Medium

**Steps**:
1. Ensure daemon is not running
2. Launch preferences application
3. Attempt various operations:
   - Sign in
   - Change settings
   - Test connection

**Expected Results**:
- Clear error messages when daemon unavailable
- Application doesn't crash
- User guidance provided
- Option to start daemon (if available)

**Pass Criteria**: Graceful handling of daemon absence

---

### TC403: Network Error Handling
**Objective**: Test behavior with network issues
**Priority**: Medium

**Steps**:
1. Disconnect network connection
2. Launch preferences with daemon running
3. Attempt authentication
4. Attempt connection test
5. Reconnect network and retry

**Expected Results**:
- Clear error messages for network issues
- No application crashes
- Retry mechanisms work
- Success after network restoration

**Pass Criteria**: Network errors handled gracefully

---

### TC404: Invalid Input Handling
**Objective**: Test response to invalid user input
**Priority**: Low

**Steps**:
1. Try to select non-existent folder paths
2. Enter invalid bandwidth limits
3. Test with very long folder paths
4. Test with special characters in paths

**Expected Results**:
- Invalid inputs are rejected or sanitized
- Clear error messages provided
- Application remains stable
- Input validation works correctly

**Pass Criteria**: Invalid inputs handled properly

---

## Test Suite 6: Accessibility and Usability

### TC501: Keyboard Navigation
**Objective**: Verify keyboard accessibility
**Priority**: Medium

**Steps**:
1. Launch preferences application
2. Use only keyboard to navigate:
   - Tab between controls
   - Use arrow keys in tabs
   - Press Enter/Space on buttons
   - Test keyboard shortcuts

**Expected Results**:
- All controls accessible via keyboard
- Focus indicators are visible
- Logical tab order maintained
- Keyboard shortcuts work
- No keyboard traps

**Pass Criteria**: Full keyboard accessibility

---

### TC502: Text Sizing and Fonts
**Objective**: Test text readability
**Priority**: Low

**Steps**:
1. Test with different system font sizes
2. Verify text doesn't overflow
3. Check for text truncation
4. Verify contrast and readability

**Expected Results**:
- Text scales appropriately
- No text overflow or truncation
- Good contrast ratio
- Readable at various sizes

**Pass Criteria**: Text remains readable in all conditions

---

### TC503: Window Scaling
**Objective**: Test UI scaling behavior
**Priority**: Low

**Steps**:
1. Test on different screen resolutions
2. Test with different UI scaling factors
3. Verify minimum/maximum window sizes
4. Test with multi-monitor setups

**Expected Results**:
- UI scales appropriately
- Minimum size maintains usability
- Maximum size doesn't become unwieldy
- Multi-monitor handling works

**Pass Criteria**: UI scales correctly across configurations

---

## Test Execution Checklist

### Pre-Testing Setup
- [ ] Build all components successfully
- [ ] Verify daemon executable works
- [ ] Verify preferences executable works
- [ ] Have Microsoft account credentials ready
- [ ] Create test folder structure
- [ ] Document system configuration

### Test Execution Order
1. **Basic UI Tests**: TC001-TC003
2. **Account Functionality**: TC101-TC105
3. **Sync Configuration**: TC201-TC204
4. **Advanced Settings**: TC301-TC304
5. **Integration Tests**: TC401-TC404
6. **Accessibility Tests**: TC501-TC503

### Post-Testing Cleanup
- [ ] Sign out of test account
- [ ] Reset preferences to defaults
- [ ] Remove test folders
- [ ] Stop daemon processes
- [ ] Document any issues found

---

## Bug Reporting Template

When issues are found during testing, use this template:

**Bug ID**: BUG-XXXX
**Test Case**: TCXXX
**Severity**: High/Medium/Low
**Priority**: High/Medium/Low

**Summary**: Brief description of the issue

**Steps to Reproduce**:
1. Step one
2. Step two
3. Step three

**Expected Result**: What should happen
**Actual Result**: What actually happened
**Environment**: Haiku version, build details
**Screenshots**: If applicable
**Workaround**: If available

---

## Performance Benchmarks

### Response Time Targets
- Application launch: < 3 seconds
- Tab switching: < 0.5 seconds
- Authentication: < 10 seconds
- Connection test: < 5 seconds
- Settings save: < 1 second

### Resource Usage Targets
- Memory usage: < 50 MB
- CPU usage: < 5% when idle
- Disk usage: < 10 MB for preferences data

---

## Test Coverage Summary

| Component | Unit Tests | Integration Tests | GUI Tests |
|-----------|------------|-------------------|-----------|
| AuthManager | ✅ | ✅ | ✅ |
| OneDriveAPI | ✅ | ✅ | ✅ |
| OneDriveDaemon | ✅ | ✅ | ✅ |
| OneDrivePrefs | Manual | ✅ | ✅ |

**Total Test Cases**: 30 GUI test cases + Unit/Integration tests
**Estimated Testing Time**: 4-6 hours for complete manual testing
**Automation Potential**: 70% of tests could be automated with UI testing framework