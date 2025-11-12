# Building Starship for iOS

This guide explains how to build Starship for iOS devices (iPhone/iPad).

## Prerequisites

### Required Software
- **macOS** (required for iOS development)
- **Xcode 14.0 or later** (downloadable from Mac App Store)
- **Xcode Command Line Tools**
  ```bash
  xcode-select --install
  ```
- **CMake 3.16 or later**
  ```bash
  brew install cmake
  ```

### Apple Developer Account
- Free account: Can build and run on your own devices for 7 days
- Paid account ($99/year): Required for App Store distribution

## Build Instructions

### 1. Clone the Repository
```bash
git clone https://github.com/yourusername/Starship.git
cd Starship
git submodule update --init --recursive
```

### 2. Prepare ROM File
Place your Star Fox 64 ROM (`baserom.z64`) in the project root directory. The ROM must be the US version.

### 3. Configure for iOS Build

First, set your Apple Developer Team ID in `CMakeLists.txt`:
```cmake
# Line 71 in CMakeLists.txt
set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "YOUR_TEAM_ID")
```

To find your Team ID:
1. Open Xcode
2. Go to Xcode → Preferences → Accounts
3. Select your Apple ID
4. Click "Manage Certificates"
5. Your Team ID is shown next to your account

### 4. Generate Xcode Project

```bash
mkdir build-ios
cd build-ios

# For iOS device (recommended)
cmake .. -G Xcode -DIOS=ON -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake

# For iOS Simulator (testing on Mac)
cmake .. -G Xcode -DIOS=ON -DPLATFORM=SIMULATOR64 -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake
```

### 5. Build with Xcode

#### Option A: Using Xcode GUI (Recommended)
1. Open the generated `Starship.xcodeproj` in Xcode:
   ```bash
   open Starship.xcodeproj
   ```
2. Select your iOS device or simulator from the device menu
3. Click the Play button (▶) or press Cmd+R to build and run

#### Option B: Using Command Line
```bash
# Build for device
xcodebuild -project Starship.xcodeproj -scheme Starship -configuration Release -destination generic/platform=iOS

# Build for simulator
xcodebuild -project Starship.xcodeproj -scheme Starship -configuration Release -destination "platform=iOS Simulator,name=iPhone 14 Pro"
```

### 6. Install on Device

#### Via Xcode (Easiest)
1. Connect your iOS device via USB
2. Trust the computer on your device if prompted
3. Select your device in Xcode's device menu
4. Click Run (▶)
5. On your device, go to Settings → General → Device Management
6. Tap your Apple ID and trust the app

#### Via IPA Export
1. In Xcode, select Product → Archive
2. Once archived, click "Distribute App"
3. Choose "Ad Hoc" or "Development"
4. Export the IPA file
5. Install using Apple Configurator 2 or a similar tool

## iOS-Specific Features

### Touch Controls
- **Left Stick**: Movement control
- **Right Stick**: Camera control
- **A/B/X/Y Buttons**: Primary action buttons
- **D-Pad**: Directional controls
- **L/R Shoulders**: Shoulder buttons
- **+/−**: Start and Select buttons

Touch controls automatically hide when a physical game controller is connected via Bluetooth.

### Game Controller Support
Starship supports MFi (Made for iPhone) game controllers:
- Xbox Wireless Controller (Bluetooth)
- PlayStation DualShock 4 / DualSense
- Any MFi-certified controller

Controllers are automatically detected when connected, and touch controls will hide.

### File Management
The app uses iOS's document storage:
- **Save files**: Stored in app's Documents directory
- **Mods**: Place in the app's Documents folder using Files app
- **ROMs**: Can be imported via the Files app share sheet

To access the app's files:
1. Open the Files app on iOS
2. Navigate to "On My iPhone" → "Starship"
3. Place mod files here

### Graphics
- Uses **Metal** graphics API (native iOS GPU acceleration)
- Supports Retina displays with automatic scaling
- Runs at 60 FPS on modern devices (iPhone X and later)

## Troubleshooting

### Code Signing Issues
**Error**: "Failed to create provisioning profile"
**Solution**: Make sure you've set your Team ID in CMakeLists.txt and are signed in to Xcode with your Apple ID.

### Build Fails with "No Such Module 'Metal'"
**Solution**: Make sure you're using Xcode 14+ and targeting iOS 13.0 or later.

### App Crashes on Launch
**Solution**: Check that all required resources are included:
- Verify `sf64.o2r` and `starship.o2r` are in the app bundle
- Check the Xcode console for detailed error messages

### Touch Controls Not Appearing
**Solution**: The controls hide automatically when a controller is connected. Disconnect any Bluetooth game controllers to see touch controls.

### Performance Issues
**Solution**:
- Use Release build configuration (not Debug)
- Test on physical device (simulator is slower)
- Older devices (iPhone 7 and earlier) may have reduced performance

## Distribution

### TestFlight (Beta Testing)
1. Enroll in Apple Developer Program ($99/year)
2. Create an App ID in App Store Connect
3. Archive the app in Xcode
4. Upload to App Store Connect
5. Configure TestFlight beta testing
6. Invite testers via email

### App Store Release
**Note**: Distributing game emulators/ports on the App Store requires careful attention to Apple's guidelines. Consult with legal counsel before attempting App Store distribution.

## Additional Notes

### Minimum Requirements
- **iOS Version**: 13.0 or later
- **Devices**: iPhone 6s and later, iPad Air 2 and later
- **Storage**: ~500 MB free space
- **Chipset**: A9 chip or later (Metal 2 support)

### Recommended Devices
- iPhone X or later
- iPad Pro (any generation)
- iPad Air 3 or later

### Known Limitations
- No multitasking/Picture-in-Picture support
- Must be in landscape orientation
- Save states stored locally (no iCloud sync currently)

## Support

For iOS-specific issues, please check:
1. GitHub Issues: https://github.com/alexvnesta/Starship/issues
2. Build logs in Xcode for detailed error messages
3. Console output during runtime for crashes

## Credits

iOS port implementation follows the Android port structure created by izzy2lost.
- Android port: izzy2lost
- iOS port: [Your Name]
- Original Starship project: SonicDcer & Lywx
- libultraship: HarbourMasters team
