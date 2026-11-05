BuildAndDeploy:

# Making changes:

## To change truck inventory:
Edit Source\CyberWagonFirmware\CWFU_WorldSubsystem
static const int FCWFU_INVENTORY_SLOTS = 120;

## To change station appearance:

Edit the script that creates the bluprints:
	Scripts/CreateStationBlueprints.py

Run it from D:\SML with the command:
"D:\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" FactoryGame.uproject -run=pythonscript -script=D:/SML/Mods/GameFeatures/CyberWagonFirmware/Scripts/CreateStationBlueprints.py -unattended -nullrhi


# To create version:


Edit CyberWagonFirmware.upluging
Update:
	"FileVersion": 3,
	"Version": 4,
	"VersionName": "1.2.0",
	"SemVersion": "1.2.0",

Start unreal engine: 
    Click c:\SML\FactoryGame.uproject

Click AlPakit
Create pack the changes and test

Click AlPakit Release (Alpaka in a rocket ship)

Select the CyberWagon and Alpakit for all platforms

git commit -m "Version 1.2.0"
git tag -f -a v1.2.0 -m "Version 1.2.0"
git push origin main refs/tags/v1.2.0

Create a new release on GitHub
Push the new code and set the tag for the latest release version