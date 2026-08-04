# pssst-player
* simple C GUI for playing music with psp

## features 
* MP3 playing at 320kbps/48khz
* FLAC playing at 24 bit / 48khz, auto resampled to 16/48
* Opus support (VBR obviously), use opustools to convert
* cool (imo!) UI

<img width="3520" height="1980" alt="IMG_4123" src="https://github.com/user-attachments/assets/97e5f675-fbd8-4602-9109-9458a6dc4618" />
<img width="3520" height="1980" alt="IMG_4119" src="https://github.com/user-attachments/assets/48c2d67c-ba3b-40b0-ac63-d56db412090b" />


## controls:
* X to play. O to pause/play. triangle to shuffle. square to stop.
* L/R bumpers to skip/prev song
* SELECT to toggle file explorer in playing view
* START to toggle repeat modes
* HOME button to exit

## to implement:
* wav support
* i wanna get a proper tree view or at least file explorer view, so for that i cant just recurse i have to keep track of each file's parent dir if i traversed it
^^done

## wishlist:
* might be doable:
* eq
* FFT equaliser
* ME acceleration

## tofix:
* why de fuck is 48khz path so wonky with static
