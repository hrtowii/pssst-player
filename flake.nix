{
  description = "psp crossplatform dev shell";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    pspdev.url = "github:pspdev/pspdev-nix";
    libmad-src = {
      url = "github:pspdev/libmad";
      flake = false;
    };
    libogg-src = {
      url = "github:xiph/ogg";
      flake = false;
    };
    libflac-src = {
      url = "github:xiph/flac";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      pspdev,
      libmad-src,
      libogg-src,
      libflac-src,
    }:
    flake-utils.lib.eachSystem
      [
        "aarch64-darwin"
        "x86_64-darwin"
        "x86_64-linux"
        "aarch64-linux"
      ]
      (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          libmad = pkgs.stdenv.mkDerivation {
            pname = "psp-libmad";
            version = "unstable";
            src = libmad-src;

            nativeBuildInputs = [
              pkgs.gnumake
              pspdev.packages.${system}.pspsdk
              pspdev.packages.${system}.psp-gcc
              pspdev.packages.${system}.psp-binutils
              pspdev.packages.${system}.psp-pkg-config
            ];

            AR = "${pspdev.packages.${system}.psp-binutils}/bin/psp-ar";
            RANLIB = "${pspdev.packages.${system}.psp-binutils}/bin/psp-ranlib";

            configurePhase = ''
              	    	runHook preConfigure
                  	 	runHook postConfigure
            '';

            buildPhase = ''
              	    	runHook preBuild
                            	make -j$NIX_BUILD_CORES AR=$AR RANLIB=$RANLIB
                            	runHook postBuild
            '';

            installPhase = ''
              	    	runHook preInstall
                  	    	make install PSPDIR=$out AR=$AR RANLIB=$RANLIB
              		${pspdev.packages.${system}.psp-binutils}/bin/psp-ranlib $out/lib/libmad.a
                  		runHook postInstall
            '';
          };

          libogg = pkgs.stdenv.mkDerivation {
            pname = "psp-libogg";
            version = "unstable";
            src = libogg-src;

            nativeBuildInputs = [
              pkgs.cmake
              pspdev.packages.${system}.psp-cmake
              pspdev.packages.${system}.pspsdk
              pspdev.packages.${system}.psp-gcc
              pspdev.packages.${system}.psp-binutils
            ];

            configurePhase = ''
              runHook preConfigure
              psp-cmake -B build -S . -DCMAKE_INSTALL_PREFIX=$out -DBUILD_TESTING=OFF
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              cmake --build build -j$NIX_BUILD_CORES
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              cmake --install build
              runHook postInstall
            '';
          };

          libflac = pkgs.stdenv.mkDerivation {
            pname = "psp-libflac";
            version = "unstable";
            src = libflac-src;

            nativeBuildInputs = [
              pkgs.cmake
              pspdev.packages.${system}.psp-cmake
              pspdev.packages.${system}.pspsdk
              pspdev.packages.${system}.psp-gcc
              pspdev.packages.${system}.psp-binutils
              pspdev.packages.${system}.psp-pkg-config
            ];

            buildInputs = [ libogg ];
            CFLAGS = "-Wno-error=incompatible-pointer-types -fpermissive";
            #        > /nix/var/nix/builds/nix-2305-1326438599/source/src/libFLAC/include/private/bitreader.h:81:77: note: expected 'FLAC__int32 *' {aka 'long int *'} but argument is of type 'int *
            configurePhase = ''
                            runHook preConfigure
                            psp-cmake -B build -S . \
                              -DCMAKE_INSTALL_PREFIX=$out \
                              -DBUILD_PROGRAMS=OFF \
                              -DBUILD_EXAMPLES=OFF \
                              -DBUILD_TESTING=OFF \
              		-DINSTALL_MANPAGES=OFF \
              		-DCMAKE_PREFIX_PATH="${libogg}" \
                              -DOGG_INCLUDE_DIR="${libogg}/include" \
                              -DOGG_LIBRARY="${libogg}/lib/libogg.a"
                            runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              cmake --build build -j$NIX_BUILD_CORES
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              cmake --install build
              runHook postInstall
            '';
          };
        in
        {
          packages = {
            libmad = libmad;
          };
          devShells.default = pkgs.mkShell {
            name = "psp-dev";
            buildInputs = [
              pspdev.packages.${system}.psp-binutils
              pspdev.packages.${system}.psp-gcc
              pspdev.packages.${system}.pspsdk
              pspdev.packages.${system}.psplink
              pspdev.packages.${system}.psplinkusb
              pspdev.packages.${system}.psp-pkg-config
              pspdev.packages.${system}.ebootsigner
              pspdev.packages.${system}.psp-cmake
              libmad
              libogg
              libflac
            ];
            shellHook = ''
                                echo "PSP toolchain ready (${system})"
              		  PSPGCC=${pspdev.packages.${system}.psp-gcc}/bin/psp-gcc
                		  PSP_SYSROOT=${pspdev.packages.${system}.pspsdk}/psp/sdk
                		  PSP_GCC_VERSION=$($PSPGCC -dumpversion)
                		  PSP_GCC_LIBDIR=$($PSPGCC -print-file-name=include)

                            	  build() {
                                  rm -rf build
                                  psp-cmake -B build -S . \
                                -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
                                -DCMAKE_LIBRARY_PATH="${libmad}/lib;${libogg}/lib;${libflac}/lib" \
                                -DCMAKE_INCLUDE_PATH="${libmad}/include;${libogg}/include;${libflac}/include" || return 1
              		  ln -sf build/compile_commands.json compile_commands.json
              		        cat > "$PWD/.clangd" <<EOF
              CompileFlags:
                CompilationDatabase: build
                Add:
                  - -isystem''${libmad}/include
                  - -isystem''${libmad}/psp/sdk/include
                  - -isystem''${libogg}/include
                  - -isystem''${libflac}/include
                  - -isystem''${PSPGCC%/bin/psp-gcc}/psp/include
                  - -isystem''${PSP_GCC_LIBDIR}
                  - -isystem''${PSP_SYSROOT}/include
                  - --gcc-toolchain=''${PSPGCC%/bin/psp-gcc}
                  - -fgnuc-version=''${PSP_GCC_VERSION}
                  - -std=gnu17
              EOF
                                  cmake --build build
                                }

                                elf() {
                                  build || return 1
                                  echo "ELF/PBP output in ./build — drag build/*.pbp (or the .elf) onto PPSSPP"
                                  ls -la build/*.pbp build/*.elf 2>/dev/null
                                }

                                flash() {
                                  local dest="''${1:-/Volumes/PSP}"
                                  local name
                                  name=$(basename "$(pwd)")

                                  if [ ! -d "$dest" ]; then
                                    echo "PSP not found at $dest — plug it in, enable USB mode on the XMB, or pass the mount point: flash /Volumes/YourPSPName"
                                    return 1
                                  fi

                                  build || return 1

                                  mkdir -p "$dest/PSP/GAME/$name"
                                  cp -v build/*.pbp "$dest/PSP/GAME/$name/" 2>/dev/null \
                                    || { echo "no .pbp found in build/ — check create_pbp_file ran"; return 1; }

                                  echo "flashed to $dest/PSP/GAME/$name"
                                }

                                export -f build elf flash
              		  export LIBMAD_LIBDIR="${libmad}/lib"
              		  export LIBMAD_INCDIR="${libmad}/include"
              		  export OGG_LIBDIR="${libogg}/lib"
                            export OGG_INCDIR="${libogg}/include"
                            export FLAC_LIBDIR="${libflac}/lib"
                            export FLAC_INCDIR="${libflac}/include"
            '';
          };
        }
      );
}
