{
  description = "LanderViz RP2040 audio visualizer and Moonlander RGB receiver";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    pico-pio-usb = {
      url = "github:sekigon-gonnoc/Pico-PIO-USB/5a37a66dc5d3fbe0ef3cdbeda923a757440f984f";
      flake = false;
    };
    tinyusb = {
      url = "github:hathach/tinyusb/86ad6e56c1700e85f1c5678607a762cfe3aa2f47";
      flake = false;
    };
    zsa-qmk = {
      url = "github:zsa/qmk_firmware/93b2b9ec3368f86c5eb5a2e3f934049f8daef885";
      flake = false;
    };
    qmk-modules = {
      url = "github:zsa/qmk_modules/13890cd7856175de20798689d15ba6a46bf0c5c7";
      flake = false;
    };
    chibios = {
      url = "github:qmk/ChibiOS/be44b3305f9a9fe5f2f49a4e7b978db322dc463e";
      flake = false;
    };
    chibios-contrib = {
      url = "github:qmk/ChibiOS-Contrib/77cb0a4f7589f89e724f5e6ecb1d76d514dd1212";
      flake = false;
    };
    lufa = {
      url = "github:qmk/lufa/549b97320d515bfca2f95c145a67bd13be968faa";
      flake = false;
    };
    qmk-printf = {
      url = "github:qmk/printf/c2e3b4e10d281e7f0f694d3ecbd9f320977288cc";
      flake = false;
    };
  };

  outputs = inputs @ {
    self,
    nixpkgs,
    pico-pio-usb,
    tinyusb,
    zsa-qmk,
    qmk-modules,
    chibios,
    chibios-contrib,
    lufa,
    qmk-printf,
    ...
  }: let
    systems = ["aarch64-darwin" "aarch64-linux" "x86_64-linux"];
    forAllSystems = nixpkgs.lib.genAttrs systems;
    pkgsFor = system: import nixpkgs {inherit system;};
  in {
    packages = forAllSystems (system: let
      pkgs = pkgsFor system;
      toolchain = pkgs.gcc-arm-embedded;
      commonTools = [pkgs.cmake pkgs.ninja pkgs.gnumake pkgs.python3 pkgs.patch pkgs.clang toolchain];
    in {
      default = self.packages.${system}.rp2040;

      rp2040 = pkgs.stdenvNoCC.mkDerivation {
        pname = "landerviz-rp2040";
        version = "0.1.0";
        src = self;
        # These are bare-metal artifacts, not host executables for Nix to fix up.
        dontFixup = true;
        nativeBuildInputs = commonTools ++ [pkgs.picotool];
        PICO_SDK_PATH = "${pkgs.pico-sdk}/lib/pico-sdk";
        PICO_PIO_USB_PATH = pico-pio-usb;
        PICO_TINYUSB_PATH = tinyusb;
        configurePhase = ''
          runHook preConfigure
          cmake -S . -B build -G Ninja \
            -DPICO_BOARD=sparkfun_promicro \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_TESTING=OFF
          runHook postConfigure
        '';
        buildPhase = ''
          runHook preBuild
          cmake --build build
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p "$out"
          cp build/landerviz.uf2 build/landerviz.elf build/landerviz.bin "$out/"
          runHook postInstall
        '';
      };

      moonlander = pkgs.stdenvNoCC.mkDerivation {
        pname = "landerviz-moonlander";
        version = "0.1.0";
        src = self;
        nativeBuildInputs = commonTools ++ [pkgs.git pkgs.qmk pkgs.which];
        dontConfigure = true;
        buildPhase = ''
          runHook preBuild
          cp -R ${zsa-qmk} qmk
          chmod -R u+w qmk
          mkdir -p qmk/lib/chibios qmk/lib/chibios-contrib qmk/lib/lufa qmk/lib/printf
          cp -R ${chibios}/. qmk/lib/chibios/
          cp -R ${chibios-contrib}/. qmk/lib/chibios-contrib/
          cp -R ${lufa}/. qmk/lib/lufa/
          cp -R ${qmk-printf}/. qmk/lib/printf/
          mkdir -p qmk/modules/zsa
          cp -R ${qmk-modules}/. qmk/modules/zsa/
          keymap_dir=qmk/keyboards/zsa/moonlander/keymaps/landerviz
          mkdir -p "$keymap_dir"
          cp -R moonlander/oryx/. "$keymap_dir/"
          cp moonlander/landerviz.c moonlander/landerviz.h "$keymap_dir/"
          cp include/landerviz/protocol.h "$keymap_dir/protocol.h"
          patch --fuzz=0 -d "$keymap_dir" -p1 < moonlander/oryx-ARDpK.patch
          make -C qmk zsa/moonlander/reva:landerviz
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p "$out"
          cp qmk/zsa_moonlander_reva_landerviz.bin "$out/"
          runHook postInstall
        '';
      };

      preview = pkgs.stdenv.mkDerivation {
        pname = "landerviz-preview";
        version = "0.1.0";
        src = self;
        nativeBuildInputs = [pkgs.makeWrapper pkgs.pkg-config];
        buildInputs = [pkgs.SDL2];
        dontConfigure = true;
        buildPhase = ''
          runHook preBuild
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror -DLANDERVIZ_PREVIEW_FLOW_V2 -DLANDERVIZ_PREVIEW_FLOW_V3 -Iinclude \
            tools/preview.cpp src/audio_analysis.cpp src/effects.cpp src/flow_v2.cpp src/flow_v3.cpp src/visualizer.cpp \
            $(pkg-config --cflags --libs sdl2) -o landerviz-preview
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p "$out/bin"
          cp landerviz-preview "$out/bin/"
          wrapProgram "$out/bin/landerviz-preview" \
            --prefix PATH : ${nixpkgs.lib.makeBinPath [pkgs.ffmpeg]}
          ${nixpkgs.lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
            app="$out/LanderViz.app/Contents"
            mkdir -p "$app/MacOS"
            cp tools/macos/Info.plist "$app/Info.plist"
            substitute tools/macos/launch-preview.sh "$app/MacOS/LanderViz" \
              --replace-fail '@preview@' "$out/bin/landerviz-preview"
            chmod +x "$app/MacOS/LanderViz"
          ''}
          runHook postInstall
        '';
      };
    });

    apps = forAllSystems (system: {
      preview = {
        type = "app";
        program = "${self.packages.${system}.preview}/bin/landerviz-preview";
        meta.description = "Preview LanderViz effects with generated or recorded audio";
      };
    });

    checks = forAllSystems (system: let
      pkgs = pkgsFor system;
    in {
      protocol = pkgs.stdenv.mkDerivation {
        pname = "landerviz-protocol-tests";
        version = "0.1.0";
        src = self;
        dontConfigure = true;
        buildPhase = ''
          $CXX -std=c++17 -Wall -Wextra -Werror -DLANDERVIZ_PREVIEW_FLOW_V2 -DLANDERVIZ_PREVIEW_FLOW_V3 -Iinclude \
            tests/protocol_test.cpp src/audio_analysis.cpp src/effects.cpp src/flow_v2.cpp src/flow_v3.cpp src/visualizer.cpp src/usb_status.cpp \
            -o protocol_test
          ./protocol_test
        '';
        installPhase = ''
          touch "$out"
        '';
      };
      rp2040 = self.packages.${system}.rp2040;
      moonlander = self.packages.${system}.moonlander;
    });

    devShells = forAllSystems (system: let
      pkgs = pkgsFor system;
    in {
      default = pkgs.mkShell {
        packages = [
          pkgs.alejandra
          pkgs.cmake
          pkgs.clang
          pkgs.gcc-arm-embedded
          pkgs.gnumake
          pkgs.ninja
          pkgs.patch
          pkgs.picotool
          pkgs.pkg-config
          pkgs.python3
          pkgs.SDL2
          pkgs.ffmpeg
        ];
        PICO_SDK_PATH = "${pkgs.pico-sdk}/lib/pico-sdk";
        PICO_PIO_USB_PATH = pico-pio-usb;
        PICO_TINYUSB_PATH = tinyusb;
      };
    });
  };
}
