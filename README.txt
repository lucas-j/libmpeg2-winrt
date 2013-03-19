libmpeg2-winrt

This project implements libmpeg2 (available at http://libmpeg2.sourceforge.net/)
as a Windows Media Framework transform decoder, using asynchronous callbacks to queue
and parse MPEG-2 packets into raw YUV 4:2:0 frames which are then returned to MF.
This can be embedded into a WinRT (Metro) app and used to play back MPEG-2 videos.
A sample Windows 8 app is provided.

Because libmpeg2 is GPL, this project must also be licensed as GPL as well.

TODO:
- Port SSE2/MMX code to Microsoft Visual Studio 12 (done)
- Allow arbitrary playback (backward, seeking, etc.)
- Add a demultiplexer for MPEG-2 PS and TS, and WTV
- Add an audio processing decoder as well (doesn't seem entirely necessary)