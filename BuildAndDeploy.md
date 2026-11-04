BuildAndDeploy:

# To create version, incereasing trunk capacity:

Edit Source\CyberWagonFirmware\CWFU_WorldSubsystem
static const int FCWFU_INVENTORY_SLOTS = 120;

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