BuildAndDeploy:

# To create version 1.1.1, incereasing trunk capacity:

Edit Source\CyberWagonFirmware\CWFU_WorldSubsystem
static const int FCWFU_INVENTORY_SLOTS = 120;

Edit CyberWagonFirmware.upluging
Update:
	"FileVersion": 3,
	"Version": 3,
	"VersionName": "1.1.1",
	"SemVersion": "1.1.1",

Start unreal engine: 
    Click c:\SML\FactoryGame.uproject

Click AlPakit
Create pack the changes and test

Click AlPakit Release (Alpaka in a rocket ship)

Select the CyberWagon and Alpakit for all platforms

git commit -m "Version 1.1.1"
git tag -f -a v1.1.1 -m "Version 1.1.1"
git push origin main refs/tags/v1.1.1