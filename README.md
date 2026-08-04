# pssst-player
* simple C GUI for playing music with psp

## features 
* MP3 playing at 320kbps/48khz
* FLAC playing at 24 bit / 48khz, auto resampled to 16/48
* Opus support (VBR obviously), use opustools to convert
* cool (imo!) UI

<img width="1604" height="1041" alt="Screenshot 2026-08-05 at 4 46 37 AM" src="https://github.com/user-attachments/assets/c298857a-5aef-47ec-8691-1476fdab2e39" />
<img width="1560" height="997" alt="Screenshot 2026-08-05 at 4 43 52 AM" src="https://github.com/user-attachments/assets/e1ac7b88-0a59-4b2d-b2ee-af954c95afcb" />


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
