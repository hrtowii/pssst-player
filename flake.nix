{
  description = "psp crossplatform dev shell";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    pspdev.url = "github:pspdev/pspdev-nix";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      pspdev,
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
        in
        {
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
            ];
            shellHook = ''
                            echo "PSP toolchain ready (${system})"
              	       build() {
                    rm -rf build
                    psp-cmake -B build -S . || return 1
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
              	      
            '';
          };
        }
      );
}
