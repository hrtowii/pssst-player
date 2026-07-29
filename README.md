# pssst-player
* simple C GUI for playing MP3s with psp

* might be doable:
* eq
* FFT equaliser
* ME acceleration

* todo:
1. GUI - rudimentary w Clay
2. actual playing backend is done, todo 2: FLAC, wav, pcm, means abstracting it to its own struct and adding fptrs -> done
```bash
libflac errs:

/nix/store/cm1mw2hi59x7awxv22z47q2k35hpjfq4-psp-binutils-allegrex-v2.44/bin/psp-ld: /nix/store/2l09nkc2zb9304mbp559vjfwsm1nzpja-psp-libflac-unstable/lib/libFLAC.a(metadata_iterators.c.obj): in function `set_file_stats_':
metadata_iterators.c:(.text+0xd104): undefined reference to `utimensat'
/nix/store/cm1mw2hi59x7awxv22z47q2k35hpjfq4-psp-binutils-allegrex-v2.44/bin/psp-ld: metadata_iterators.c:(.text+0xd120): undefined reference to `chown'
/nix/store/cm1mw2hi59x7awxv22z47q2k35hpjfq4-psp-binutils-allegrex-v2.44/bin/psp-ld: metadata_iterators.c:(.text+0xd184): undefined reference to `chown'
collect2: error: ld returned 1 exit status
ninja: build stopped: subcommand failed.
```

3. file scanning and config is done
