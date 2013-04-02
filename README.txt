libmpeg2-winrt

Now available in the Windows Store! App name is "MPEG-2 TS Player"
http://apps.microsoft.com/windows/app/mpeg-2-ts-player/4cde4355-69ea-45ca-8770-d84fd194cfc8

This project implements libmpeg2 (available at http://libmpeg2.sourceforge.net/)
as a Windows Media Framework transform decoder, using asynchronous callbacks to
queue and parse MPEG-2 packets into raw YUV 4:2:0 frames which are then
returned to MF. This can be embedded into a WinRT (Metro) app and used to play
back MPEG-2 videos. A sample Windows 8 app is provided.

Because libmpeg2 is GPL, this project must also be licensed as GPL as well.

TODO:
- Port SSE2/MMX code to Microsoft Visual Studio 12 (done)
- Add seek support (done)
- Implement quality-of-service control to allow older CPUs to framedrop
- Allow arbitrary playback (backward, seeking, etc.)
- Add a demultiplexer for MPEG-2 PS and TS, and WTV
- Add an audio processing decoder as well (doesn't seem entirely necessary)
- Migrate inline assembly code to Intel intrinsics, to allow x64 native builds
- Test ARM build (I'd need access to a Surface RT for this)
- Smoother seek transitions (e.g., seek only to nearest P-frames)
- Parallel decoding (this would be tricky to implement, but worth it)
- Various UI improvements to app (playlist, subtitle support, metadata...)