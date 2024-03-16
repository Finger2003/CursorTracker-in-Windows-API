# CursorTracker
CursorTracker is a simple window application built using the Windows API that displays a semi-transparent pulsing circle around the cursor and shows pressed keys. Upon running the application, it provides keyboard shortcuts instructions and information about system tray icon.
## Features
- Displays a semi-transparent circle in chosen color around the cursor.
- Tracks and displays pressed keys.
- Provides keyboard shortcuts instructions.
- Supports system tray icon functionality.
- Saves user settings to .ini file.
## Installation
1. Clone this repository to your local machine.
2. Open the project in Visual Studio or other IDE for Windows development.
3. Build the project to generate the executable file.
## Usage
1. Run the executable file.
2. A semi-transparent circle will appear around the cursor.
3. Press any key combination to see it displayed in the left corner of the screen.
4. Refer to the instructions displayed by the application for keyboard shortcuts - if it does not show up, press 'Ctrl + F12'.
5. Access additional functionalities through the system tray icon.
## Keyboard Shortcuts
- Toggle instructions: 'Ctrl + F12'
- Toggle pulsing: 'Alt + Shift + C'
- Close the app: 'Alt + Shift + F4'
## Remarks
Before closing the application, user settings are saved to "settings.ini" file in a working directory. These settings include the color of the circle, whether to display instructions, and whether to enable pulsing.
