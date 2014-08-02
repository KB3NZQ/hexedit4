@echo off
REM HexEditPostBuild.bat - do a few things after HexEdit.exe has been built
REM NOTE: Help file building (map files etc) is no longer performed here.

ECHO Fix up hexedit.exe icon since the resource compiler cannot handle the new icon format
if exist ReleaseVS2008\hexedit.exe  ReplaceVistaIcon  ReleaseVS2008\hexedit.exe   res\hexedit2.ico
if exist ReleaseVS2010\hexedit.exe  ReplaceVistaIcon  ReleaseVS2010\hexedit.exe   res\hexedit2.ico
if exist ReleaseVS2012\hexedit.exe  ReplaceVistaIcon  ReleaseVS2012\hexedit.exe   res\hexedit2.ico
