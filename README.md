# Eluxi-Player
A GTK based front-end for MPV. A simple media-player project I started to see what it was like and to attempt trying to code in C which I have not done much.

Released here I suppose for myself. This program does indeed work for a media player.
![Example Appearance](Screenshots/Screenshot%20from%202025-04-29%2014-23-15.png)
![Example Appearance 2](Screenshots/Screenshot%20from%202025-04-28%2002-15-40.png)
Does it follow best practices, probably not. Has it been fixed to prevent all possible memory leaks, again probably not. 
I am not a professional, I am still working on this project. There are quite a few features and functions I have yet to add.
I am releasing it here so that anyone with better skills and more proper know how could potentially make it a viable use choice!
While I might use it as is...well once I fix it up to include more of the features I would like...many others might not.
Some things are imperfect and or unfinished, rigged to work but not correct in practice. I'm just a hobbyist sorry to dissappoint.
Why am I not making it sound like the ideal choice....idk I suppose I'm not upselling the thing, use it or don't, bothers me none lol just wanted to share it in case.
I thought "Fork it" why not! Punny.

Icons are theme based. The visuals in mine are because I use the BeautySolar icon pack.

Tested on Ubuntu, GNOME.

Current Hotkeys/Bindings
F1,F2,F3,F11,Space,Esc

F1 - F3 are show/hides, F1 attached to the buttons menu, F2 is the duration menu, F3 is the playlist.

Playlist lets you select which media to play and traverse the list. 

Will update the description later.


to run the built version you likely need to: "sudo apt install mpv libgtk-3-0 libglib2.0-0 libx11-6"


to build it yourself likely: "sudo apt install build-essential libmpv-dev libgtk-3-dev libglib2.0-dev libx11-dev"

Currently missing that I am going to add soon, video-track selection/audio-track selection. A button to show/hide playlist/an update to the playlist...and all sorts of other stuff.

v14 Update: removes F3 as playlist hotkey, implements playlist as gtkmenu. Implements audio/video-track buttons and gtkmenus. Targets new icons that ensure default gnome actually has the icons for such (I like them less, but later on I could make them config based and allow custom icons to be set)

Issues and need to do: Well a lot, but should function fine. Things to look into if you are not me and want to help, Memory Management/Leaks needs overhaul, separate from 1 file into manageable modules. Again all sorts of other stuff, sorry. One major note, is the auto-start of playlist and files has been rigged due to me lacking patience as I was tired and did not end up fixing it, the resulting bs is that sometimes when opening-a-set of files after a set has been opened they will not auto-start and you will have to start them via the playlist. This is actually because its rigged to work and not coded how it should be. I would love to see this turn into a real trusty usable media-player but sadly it will not be able to be me that fixes it up nice and proper. I mean maybe one day but....skill wise and know-how...as of right now that's not me. Anyhow enjoy and feel free to fix-it-up proper!

Goal: I only coded this to get the gist of C and how things are done with gtk and mpv for a front-end, I do better learning by doing, but I also probably in this case need to learn quite a bit first, this really likely doesn't follow best practices but I wanted to get something started at the very least to learn from it. I do like the style of this but there is a lot that has been rigged to work not rigged to work correctly.




