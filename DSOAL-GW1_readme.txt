DSOAL-GW1 is a fork a DSOAL that has been modified to work with Guild Wars 1.
DSOAL is a DirectSound-to-OpenAL compatibility layer that is able to emulate
DirectSound3D and EAX in software. Put simply, this makes it possible to
activate GW’s “Use 3D Audio Hardware” and “Use EAX” options and to hear GW's
sound effects as originally intended.


Some history:
-------------
When GW was released in 2005, the audio component of Microsoft’s DirectX API
was something called DirectSound. DirectSound had a 3D audio component called
DirectSound3D, or DS3D, that could pan and amplify/attenuate sound sources
based on their position relative to the camera in the game’s 3D world. PC’s
with a high-end Creative sound card also had access to EAX, an extension to
DS3D with a suite of hardware DSP effects for occlusion, obstruction, reverb,
echo, etc. Like most games of its era, GW’s audio system was designed around
DirectSound and DS3D, and owners of high-end PCs could get the “definitive”
audio experience with EAX.

All that ended in 2007 with Windows Vista. Vista completely broke DS3D and EAX.
Rather than fix it, Microsoft deprecated DirectSound and pushed developers to
adopt its new XAudio2 API for future games. With DS3D and EAX broken, GW hasn’t
sounded “right” in any version of Windows since XP.


Installation:
-------------
(1) Copy dsound.dll into ONE of the following two locations:
    (a) The Guild Wars installation directory where GW.exe resides.
        * On Windows 8 or 10, you may need to make changes to the registry for
           dsound.dll to be loaded from this location. See here:
           https://www.indirectsound.com/registryIssues.html.
           Try setting the reg entries for DirectSound, DirectSound8,
           DirectSoundCapture, DirectSoundCapture8, and DirectSoundFullDuplex.
        * At least some versions of Wine simply will not load dsound.dll from
           this location no matter what you do. Use the other location if you
           encounter this problem.
    (b) The system directory for 32-bit .dll’s. On modern 64-bit Windows
         computers, this is C:\Windows\SysWOW64\. (Yes, that is correct.) On
         ancient 32-bit Windows computers (or 32-bit Wine prefixes), this is
         C:\Windows\System32\. If dsound.dll already exists in this location,
         then MAKE A BACKUP before replacing it.
(2) Copy dsoal-aldrv.dll to the same location you put dsound.dll
    * Note: This file is just a renamed copy of soft_oal.dll from openal-soft.
       I’ve included version 1.21.0 because versions 1.21.1 has a crash bug
       with custom .ambdec files. If you want to keep an eye out for updates,
       here is openal-soft’s website: https://openal-soft.org/
(3) Copy alsoft.ini to the Guild Wars installation directory where GW.exe
     resides.
(4) Copy the hrtf_defs and presets folders to
     “C:\users\<your username>\Application Data\openal\”.
     (On newer version of Windows, "Application Data" has been replaced by
     AppData\Roaming. Use that instead.)
(5) Create the directory
     “C:\users\<your username>\Application Data\openal\hrtf” and extract all of
     the .mhr files from HRTF_OAL_1.19.0.zip into that directory. (On newer
     version of Windows, "Application Data" has been replaced by
     AppData\Roaming. Use that instead.)
(6) Make the following edits to alsoft.ini:
    * (Note: Lines beginning with a # symbol are comments/examples that are
       ignored. If you want a setting to take effect, make sure there’s no #
       symbol.)
    * “sources” can be set to any power of two between 128 and 2048. Because of
       GW’s idiosyncratic approach to DirectSound buffers, only half of these
       will actually be used. Unless you’re trying to run GW on a toaster,
       leaving this at 2048 is recommended.
    * The choice of “resampler” is a matter of taste. Cubic has many fans. See
       this video for a comparison:
       https://www.youtube.com/watch?v=62U6UnaUGDE.
    * If you have four or more speakers, using an ambisonic decoder is highly
       recommended. See instructions below.
    * If you use headphones, using HRTF is highly recommended. Some people even
       describe it as “mind-blowing.” See instructions below.
(7) On Windows, it may be necessary to add -dsound to GW’s command line.
(8) On Wine, set the library override for dsound to “native, builtin.”
(9) Launch GW, hit F11 to bring up the options menu, and it should now be
     possible to enable “Use 3D Audio Hardware” and “Use EAX” in the sound tab.


Ambisonic Setup (recommended for systems with 4 or more speakers):
------------------------------------------------------------------
(1) Disable any virtual surround software, equalizers, compressors,
     crystalizers, etc. on your PC. 
(2) Consult “C:\users\<your username>\Application Data\openal\presets\presets.txt”
     to determine which preset best matches your speaker layout.
(3) Use a tape measure to measure the distance from each speaker to your
     listening position. Then edit the speaker distance values in the preset\
     accordingly. The unit is meters.
(4) Edit alasoft.ini as follows:
    * Set “channels” explicitly if your speaker setup isn’t automatically
       recognized.
    * Set “hq-mode = true”.
    * Set “distance-comp = true”.
    * Set the path to your preset for the appropriate speaker layout.
       For example: “surround51=C:/users/billybobobbubba/Application Data/openal/presets/itu5.1.ambdec”
        * Use forward slashes (/) instead of backslashes (\).
        * Quote marks aren’t necessary even if there’s a space in the pathname.

HRTF Setup (recommended for headphones):
----------------------------------------
(1) Wearing headphones, watch this video
     (https://www.youtube.com/watch?v=VCXQp7swp5k) to determine which HRTF
     preset works best for you. (It varies according to head size and shape.)
    * Note: Plug in your headphones *before* you load the youtube webpage.
(2) Disable any virtual surround software, equalizers, compressors,
     crystalizers, etc. on your PC. (See the above video for examples.)
(3) Edit alasoft.ini as follows:
    * If your headphones aren’t automatically detected, explicitly set
       “channels = stereo” and “stereo-mode = headphones”.
    * Set “frequency = 44100” or “frequency = 48000” depending on the frequency
       needed for your chosen HRTF preset.
    * Set “hrtf = true”.
    * Set “hrtf-mode = full”.
    * Set “default.hrtf” to the name of your chosen preset, minus the “.mhr”.
       (For example: “default-hrtf = irc_1007_44100”)

Configuring Adjustable Rolloff:
-------------------------------
As of version r420+GW1_rev1, DSOAL-GW1 includes a fudge factor that makes
sounds carry farther, so their diminution with distance better accords with
perceived in-game distance. This departs from the authentic “GW sound as
originally intended” experience, but most listeners consider it a large
improvement. If you don’t like the default, you can change the fudge factor by
setting the environment variable DSOAL_ROLLOFF_FUDGEFACTOR to any floating
point value between 0 and 1.0. The smaller you set this value, the farther
sounds will carry. A setting of 1.0 makes no change to the rolloff strength as
set by GW, and thus gives the authentic experience. A setting a 0 totally
disables diminution of sound with distance (which sounds terrible and is not
recommended). The default setting is one-third (0.333…).
       
Troubleshooting:
----------------
Set the following environment variables:
* DSOAL_LOGLEVEL=2
* DSOAL_LOGFILE="C:\blah\blah\blah\DSOAL_log.txt"
* ALSOFT_LOGLEVEL=3
* ALSOFT_LOGFILE="C:\blah\blah\blah\ALSOFT_log.txt"
(Use a real directory that exists, and you have write permissions for, rather
than C:\blah\blah\blah\.)

If a log file isn’t being created, then the corresponding .dll isn’t getting
loaded. The .dll files may be in the wrong place, or you may need to fix the
registry entries or add -dsound to GW’s command line. If all else fails, try
installing to the system directory.

The ALSoft log will show whether your .ini file and any presets for ambisonics
or HRTF are getting found and loaded.


Credit:
-------
The overwhelming majority of the credit for this belongs to Christopher
Robinson (kcat) and the other openal-soft developers who have spent *years*
working on open-alsoft and dsoal. They built a fricking transcontinental
railroad; I just laid the last mile of track to hook up GW station.


Comparison to Other Methods of Restoring DS3D+EAX:
--------------------------------------------------
There are other options for restoring DS3D and EAX functionality, but DSOAL is
generally superior.
* Most listeners prefer the quality of DSOAL’s emulated EAX effects to that of
   Creative ALchemy.
* You can’t *legally* obtain Creative ALchemy without buying an expensive sound
   card.
* Wine Staging’s EAX emulation only implements features up through EAX 2.0.
* IndirectSound implements DS3D, but not EAX.
* Other options don’t offer ambisonics or HRTF.


Using DSOAL-GW1 for Other Games:
--------------------------------
Will DSOAL-GW1 work for other games besides Guild Wars? It will probably work,
but provide no benefits over mainline DSOAL, and possibly hurt performance a
bit. DSOAL-GW1 is only useful if a game shares GW’s rather idiosyncratic
approach to DirectSound buffers. This might be the case if you are experiencing
missing sounds after a few minutes of gameplay when using mainline DSOAL and
your log file is full of errors that say, “DSBuffer_SetLoc Out of software
sources.” If you try DSOAL-GW1 and your log file is full of warnings that say,
“Assigning a source for software buffer that was previously deferred as per
Guild Wars hack,” then DSOAL-GW1 is probably not suitable for that game.

- Chthon
