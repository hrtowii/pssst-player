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
    oslib-src = {
      type = "git";
      url = "https://github.com/dogo/oslib";
      submodules = true;
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
      oslib-src,
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

  	    AR      = "${pspdev.packages.${system}.psp-binutils}/bin/psp-ar";
            RANLIB  = "${pspdev.packages.${system}.psp-binutils}/bin/psp-ranlib";

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
          oslib = pkgs.stdenv.mkDerivation {
            pname = "psp-oslib";
            version = "unstable";
            src = oslib-src;

            nativeBuildInputs = [
              pspdev.packages.${system}.pspsdk
              pspdev.packages.${system}.psp-gcc
              pspdev.packages.${system}.psp-binutils
              pspdev.packages.${system}.psp-cmake
              pspdev.packages.${system}.psp-libpng
              pspdev.packages.${system}.psp-zlib
            ];

            patchPhase = ''
              substituteInPlace cmake/PSP.cmake \
                --replace-fail 'COMMAND psp-config --psp-prefix' \
                'COMMAND echo ${pspdev.packages.${system}.psp-sysroot}/psp'
              substituteInPlace cmake/PSP.cmake \
                --replace-fail 'include_directories(' \
                'include_directories(${pspdev.packages.${system}.psp-libpng}/psp/include ${pspdev.packages.${system}.psp-zlib}/psp/include '
              grep -v 'audio/mod.c' CMakeLists.txt > CMakeLists.txt.tmp && mv CMakeLists.txt.tmp CMakeLists.txt
            '';
	    # ^^ mikmod ok look i just wanna build this PLS 
            configurePhase = ''
              runHook preConfigure
              psp-cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              psp-cmake --build build -j$NIX_BUILD_CORES
              runHook postBuild
            '';
	    # hell isn't other people its fucking cmake and nix packaging holy shit
            installPhase = ''
              runHook preInstall
              mkdir -p $out/lib $out/include/oslib/adhoc
              cp build/libosl.a $out/lib/
              ${pspdev.packages.${system}.psp-binutils}/bin/psp-ranlib $out/lib/libosl.a
              cp build/osl_config.h $out/include/oslib/
              cp lib/libintraFont/include/intraFont.h lib/libintraFont/include/libccc.h $out/include/oslib/
              cp lib/libpspmath/include/pspmath.h $out/include/oslib/
              cp src/adhoc/pspadhoc.h $out/include/oslib/adhoc/
              for h in src/oslmath.h src/net.h src/browser.h src/audio.h src/bgm.h \
                       src/dialog.h src/drawing.h src/keys.h src/map.h src/messagebox.h \
                       src/osk.h src/saveload.h src/oslib.h src/text.h src/usb.h \
                       src/vfpu_ops.h src/VirtualFile.h src/vram_mgr.h src/ccc.h src/sfont.h; do
                cp "$h" $out/include/oslib/
              done
              runHook postInstall
            '';
          };
	  in
        {
          packages = {
            default = oslib;
            libmad = libmad;
            oslib = oslib;
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
	      oslib
            ];
            shellHook = ''
                  echo "PSP toolchain ready (${system})"
		  PSPGCC=${pspdev.packages.${system}.psp-gcc}/bin/psp-gcc
  		  PSP_SYSROOT=${pspdev.packages.${system}.pspsdk}/psp/sdk
  		  PSP_GCC_VERSION=$($PSPGCC -dumpversion)
  		  PSP_GCC_LIBDIR=$($PSPGCC -print-file-name=include)

              	  build() {
                    rm -rf build
                    psp-cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_LIBRARY_PATH="${libmad}/lib;${oslib}/lib" -DCMAKE_INCLUDE_PATH="${libmad}/include;${oslib}/include" || return 1
		    ln -sf build/compile_commands.json compile_commands.json
		        cat > "$PWD/.clangd" <<EOF
CompileFlags:
  CompilationDatabase: build
  Add:
    - -isystem''${libmad}/include
    - -isystem''${oslib}/include
    - -isystem''${libmad}/psp/sdk/include
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
		  export OSLIB_LIBDIR="${oslib}/lib"
		  export OSLIB_INCDIR="${oslib}/include"
              	      
            '';
          };
        }
      );
}
