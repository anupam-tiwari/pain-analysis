Rodent Face Finder (RFF)
Version 0.1.7, July 7, 2011.

Copyright (c) 2011 Boston Biomedical Research Institute (BBRI)
Authors: Peng Wei and Oliver D. King (king@bbri.org) 

Documentation updated March 6, 2012.

See LICENSE.TXT for RFF license details, 
and OPENCV-LICENSE.TXT for OpenCV license details.  

--------------------------------
INSTALLATION INSTRUCTIONS:

RFF has been installed and tested on Windows 7 (64 bit), 
Ubuntu 10.10 (32 bit) and Macintosh OS X 10.6. It should also 
work on Windows XP, and may work on other platforms but has
not been tried.

RFF requires OpenCV libraries (http://opencv.willowgarage.com).
By itself OpenCV can decode a limited set of video formats
(http://opencv.willowgarage.com/wiki/VideoCodecs), but 
it can decode more video formats using FFmpeg 
(http://www.ffmpeg.org/), or for Mac OS X using QuickTime 
plug-ins such as Perian (http://perian.org/). 

The GUI requires Python. Python should already be installed 
under Ubuntu and Mac OS X 10.6 (type "which python" at the 
command prompt to verify), and for Windows it can be 
downloaded from http://www.python.org. Alternately, the
precompiled Windows binary includes the required Python 
libraries.

PRECOMPILED BINARIES:

Precompiled versions of RFF and the required OpenCV libraries 
are available for Windows 7 and Mac OS X 10.6. The Windows 7
version may also work for Windows XP. Just unzip these folder 
where-ever you like --- no additional installation should be 
required. To Uninstall, delete the folder, but  that this 
will also delete any output images if your output image directory 
is contained within this directory.

Note that because of licensing restrictions, the Mac OpenCV 
libraries are not compiled with support for FFmpeg, so may 
not be able to decode as many video formats. The Windows 
version does not include the OpenCV FFmpeg dll, but may be 
able to use it if it is present.  

COMPILING FROM SOURCE:

The compile RFF from the source-code you will need a C++ compiler. 

For Ubuntu, gcc should already be installed. 

For Mac OS X, gcc is included with Xcode. Xcode 3.0 used 
to be (and perhaps still is) free with registration from 
http://developer.apple.com/xcode. Xcode 4.0 is available 
for $5 from the Apple App Store. Note that Xcode is a 
large download (over 4GB).

For Windows, gcc is included with MinGW (http://www.mingw.org).
Instructions for installing MinGW, including changing the
environment variable for the path, are given at
http://www.mingw.org/wiki/InstallationHOWTOforMinGW

See http://opencv.willowgarage.com/wiki/ for instructions
on installing OpenCV and FFmpeg from source using CMake. 
You can also try using apt-get to install OpenCV on Ubuntu 
or MacPorts to install OpenCV for OS X. RFF should work 
with version 2.0 or later of OpenCV. 

After installing OpenCV, the simplest way to compile 
cross-platform programs that link to the OpenCV libraries 
should be by using CMake. We have had problems with 
CMake auto-detecting the location of the OpenCV libraries, 
though, so below give instructions for compiling RFF by 
invoking gcc at the command line, with explicit flags 
for the locations of the headers and libraries.
You will need to know where the opencv headers are 
located, and also where the opencv library are 
located (e.g. at /usr/local/include and /usr/local/bin 
for Ubuntu or OS X, or at C:\OpenCV\include\opencv  
and C:\OpenCV\lib for Windows.)

Also, the names of the libraries themselves has changed 
between OpenCV2.1 and OpenCV2.2, and they sometimes have 
the version number included in the filenames and sometimes 
not, so these should be adjusted as needed.

Examples: (Each command should be a single line --- 
they are split into two lines here (separated by \) 
for readability. Also, if a directory name contain 
spaces it should be enclosed in quotes.)


Ubuntu or OS X, OpenCV 2.2
g++ -o rff-bin rffmain.cpp -I/usr/local/include/opencv -L/usr/local/lib \
  -lopencv_core -lopencv_highgui -lopencv_ml -lopencv_objdetect -lopencv_imgproc

Ubuntu or OS X, OpenCV 2.1 
g++ -o rff-bin  rffmain.cpp -I/usr/local/include/opencv -L/usr/local/lib \
   -lcxcore -lcv -lml -lhighgui 

Windows 7, OpenCV 2.0
g++ -o rff-bin.exe rffmain.cpp -I C:\OpenCV\include\opencv -L C:\OpenCV\lib \
   -lcxcore200 -lcv200 -lml200 -lhighgui200 


----------------------------------------------------------------
USAGE INSTRUCTIONS:

For the precompiled Windows version, there is a program rff-gui.exe;
double-clicking this should launch the RFF program, and this option 
should not require that Python be installed. You can create a shortcut
to rff-gui.exe and place it elsewhere on your computer if you like, 
and launch the program by double-clicking on the shortcut.

Otherwise, if Python is installed, launch the program rff-gui.py. 

This can be done in several ways, depending on your platform and file associations.

(1) Using the Terminal, cd to the directory containing rff-gui.py, 
   and type the command
      python rff-gui.py 
   If rff-gui has permissions as an executable file, just typing
      ./rff-gui.py  
   may also work (provided the current directory includes the file). 

(2) Double-clicking on the rff-gui.py file might work for you, 
    or right-clicking on rff-gui.py, and selecting an option 
    like "Open with -> Python Launcher".  
    
This will open a GUI which lets you configure options, and which 
has usage information accessible from the menu-bar. 

The program rff-bin can also be invoked directly from the command-line,
with configuration options given as command-line flags, but we suggest 
using the GUI instead. The GUI will create necessary output directories 
and warn about possible over-writing of output files, whereas 
rff-bin will not.

----------------------------------------TIPS FOR GUI OPTIONS (AHT): For optimal results, name your input video files after mouse numbers.  The RFF is able to simultaneously code 2 mice in side-by-side cubicles; you can thus label your video after 2 mice.  EXAMPLE: For mouse 1 (left) and mouse 2 (right), label your video "Ms1&Ms2" and the program will automatically label each set of images for you. We recommend speed set to "Auto", linear de-interlacing, manual adjustment of zones, "just face" cropping option, and only "without boxees" selected under image options (N.B.- DO NOT SELECT "motion view"). We also recommend skipping first second to account for video weirdness depending on camera used to collect video data and analyzing at half speed to improve system stability. "Display video" should be selected, or the system may crash.  For Multicore processors (i7) you can analyze up to 8 simultaneous processes, although we recommend limiting to 4 to ensure system stability if you are not sure of your system specs.  After uploading a video you will need to queue the monitoring zones if you've selected manual. Drag a box around your cubicle you would like to analyze and press "c" to lock in the box.  If you make a mistake, you can highlight the entry you just created in the bottom of the gui and click "delete selected jobs."  Once you have uploaded all relevant videos and set your zones, click "Analyze all pending jobs" to run the program.  If you need to stop the analysis at any point, select "Kill all active jobs and stop." All pulled images will be saved under "images" in the RFFwin7 folder, and can be copied and pasted in the aMGS software image folder.  Examples of ideal settings can be found in the .JPEG- "RFF-gui example of settings". ----------------------------------------------------------------

KNOWN ISSUES: 

(1) RFF has not yet been trained or tested for mice or rats with 
coat colors other than white, so is currently not suitable for 
non-white rodents.

(2) OpenCV does not automatically detect when a video uses non-square 
pixels (see http://en.wikipedia.org/wiki/Pixel_aspect_ratio). 
Consequently videos with non-square pixels can appear to be too 
narrow. This can be over-ridden by setting the video aspect-ratio 
manually.

(3) For Mac OS X without FFmpeg, the Perian Quicktime add-ons 
(http://perian.org/) may allow more video formats to be 
decoded. However, in this case videos with non-square pixels
may appear to be skewed diagonally, and this cannot currently
be over-ridden manually.

(4) For the precompiled Mac OS X version of RFF, which uses 
Apple's image-writing routines, the output images may have 
an embedded ColorSync profile that makes them appear to have 
different brightness or contrast than the video iteslf.
In this case, as a work-around the output images can be 
dragged onto the icon for the program 
/Library/Scripts/ColorSync/Remove.app
(which should already be installed on the Mac) to remove 
the ColorSync profile information. Multiple images can be 
selected and dragged all at once, but it does not seem to 
work to drag a folder onto Renove.app.

(5) For Mac OS X 10.6 on computers with 32-bit processors
(http://support.apple.com/kb/ht3696), FFmpeg may not compile 
correctly (https://trac.macports.org/ticket/20938). Also, to 
link to QuickTime on 64-bits processors, the OpenCV libraries
may have to be compiled in 32-bit mode  
(http://tech.groups.yahoo.com/group/OpenCV/message/65895)/

(6) For MPEG file formats that do not include index information,
OpenCV does not know the length of the movie, so the Trackbar
at the top of the window will not correctly indicate the progress 
of the of the playback. Also, in these cases directly accessing 
arbitrary frames does not work, so all frames must be decoded, 
even if one wishes to skip over many consecutive frames (e.g. 
with a long gap length).  The slow or auto mode should be used
for these videos, rather than the fast mode.  

(7) For some OS X versions, the video window does not have 
height added to it to accommodate the Trackbar at the top. 
Rather, the video window is squashed a bit vertically to make 
room for the Trackbar. This does not affect the output images, 
however.

 