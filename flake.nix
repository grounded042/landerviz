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
    cmsis-dsp = {
      url = "github:ARM-software/CMSIS-DSP/v1.17.1";
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
    cmsis-dsp,
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
    cmsisQ15Sources = nixpkgs.lib.concatStringsSep " " [
      "${cmsis-dsp}/Source/CommonTables/arm_common_tables.c"
      "${cmsis-dsp}/Source/CommonTables/arm_const_structs.c"
      "${cmsis-dsp}/Source/BasicMathFunctions/arm_shift_q15.c"
      "${cmsis-dsp}/Source/TransformFunctions/arm_bitreversal.c"
      "${cmsis-dsp}/Source/TransformFunctions/arm_bitreversal2.c"
      "${cmsis-dsp}/Source/TransformFunctions/arm_cfft_q15.c"
      "${cmsis-dsp}/Source/TransformFunctions/arm_cfft_radix4_q15.c"
      "${cmsis-dsp}/Source/TransformFunctions/arm_rfft_init_q15.c"
      "${cmsis-dsp}/Source/TransformFunctions/arm_rfft_q15.c"
    ];
    cmsisQ15Objects = "arm_common_tables.o arm_const_structs.o arm_shift_q15.o arm_bitreversal.o arm_bitreversal2.o arm_cfft_q15.o arm_cfft_radix4_q15.o arm_rfft_init_q15.o arm_rfft_q15.o";
    buildHostCmsisQ15 = ''
      $CC -O2 -D__GNUC_PYTHON__ \
        -I${cmsis-dsp}/Include -I${cmsis-dsp}/PrivateInclude \
        -c ${cmsisQ15Sources}
      ar rcs libcmsis_dsp_q15.a ${cmsisQ15Objects}
    '';
  in {
    packages = forAllSystems (system: let
      pkgs = pkgsFor system;
      toolchain = pkgs.gcc-arm-embedded;
      commonTools = [pkgs.cmake pkgs.ninja pkgs.gnumake pkgs.python3 pkgs.patch pkgs.clang toolchain];
      mkRp2040 = {
        pname,
        powerTest ? false,
      }:
        pkgs.stdenvNoCC.mkDerivation {
          inherit pname;
          version = "0.1.0";
          src = self;
          # These are bare-metal artifacts, not host executables for Nix to fix up.
          dontFixup = true;
          nativeBuildInputs = commonTools ++ [pkgs.picotool];
          PICO_SDK_PATH = "${pkgs.pico-sdk}/lib/pico-sdk";
          PICO_PIO_USB_PATH = pico-pio-usb;
          PICO_TINYUSB_PATH = tinyusb;
          CMSIS_DSP_PATH = cmsis-dsp;
          configurePhase = ''
            runHook preConfigure
            cmake -S . -B build -G Ninja \
              -DPICO_BOARD=sparkfun_promicro \
              -DCMAKE_BUILD_TYPE=Release \
              -DBUILD_TESTING=OFF \
              -DLANDERVIZ_POWER_TEST=${
              if powerTest
              then "ON"
              else "OFF"
            }
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
      mkMoonlander = {
        pname,
        powerTest ? false,
      }:
        pkgs.stdenvNoCC.mkDerivation {
          inherit pname;
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
            ${pkgs.lib.optionalString powerTest ''
              patch --fuzz=0 -d "$keymap_dir" -p1 < moonlander/power-test.patch
            ''}
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
    in {
      default = self.packages.${system}.rp2040;

      rp2040 = mkRp2040 {
        pname = "landerviz-rp2040";
      };
      "rp2040-power-test" = mkRp2040 {
        pname = "landerviz-rp2040-power-test";
        powerTest = true;
      };

      moonlander = mkMoonlander {
        pname = "landerviz-moonlander";
      };
      "moonlander-power-test" = mkMoonlander {
        pname = "landerviz-moonlander-power-test";
        powerTest = true;
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
          ${buildHostCmsisQ15}
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror -DLANDERVIZ_PREVIEW_FLOW_V2 -DLANDERVIZ_PREVIEW_FLOW_V3 -DLANDERVIZ_USE_CMSIS_DSP_Q15 -Iinclude -I${cmsis-dsp}/Include \
            tools/preview.cpp src/audio_analysis.cpp src/effects.cpp src/flow_v2.cpp src/flow_v3.cpp src/flow_v3_mid.cpp src/flow_v3_color.cpp src/flow_v3_combined.cpp src/flow_v3_energy.cpp src/flow_v3_attack.cpp src/flow_v3_texture.cpp src/visualizer.cpp \
            libcmsis_dsp_q15.a $(pkg-config --cflags --libs sdl2) -o landerviz-preview
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

      "power-capture" = pkgs.stdenv.mkDerivation {
        pname = "landerviz-power-capture";
        version = "0.1.0";
        src = self;
        nativeBuildInputs = [pkgs.pkg-config];
        buildInputs = [pkgs.hidapi];
        dontConfigure = true;
        buildPhase = ''
          runHook preBuild
          $CXX -std=c++17 -O2 -Wall -Wextra -Werror -Itools \
            tools/power_capture.cpp \
            $(pkg-config --cflags --libs ${
            if pkgs.stdenv.hostPlatform.isLinux
            then "hidapi-hidraw"
            else "hidapi"
          }) \
            -o landerviz-power-capture
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p "$out/bin"
          cp landerviz-power-capture "$out/bin/"
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
      "power-capture" = {
        type = "app";
        program = "${self.packages.${system}."power-capture"}/bin/landerviz-power-capture";
        meta.description = "Capture FNIRSI FNB58 power measurements and report extrema";
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
          ${buildHostCmsisQ15}
          $CXX -std=c++17 -Wall -Wextra -Werror -DLANDERVIZ_PREVIEW_FLOW_V2 -DLANDERVIZ_PREVIEW_FLOW_V3 -DLANDERVIZ_USE_CMSIS_DSP_Q15 -Iinclude -I${cmsis-dsp}/Include \
            tests/protocol_test.cpp src/audio_analysis.cpp src/effects.cpp src/flow_v2.cpp src/flow_v3.cpp src/flow_v3_mid.cpp src/flow_v3_color.cpp src/flow_v3_combined.cpp src/flow_v3_energy.cpp src/flow_v3_attack.cpp src/flow_v3_texture.cpp src/flow_v3_engine.cpp src/visualizer.cpp src/usb_status.cpp \
            libcmsis_dsp_q15.a -o protocol_test
          ./protocol_test
        '';
        installPhase = ''
          touch "$out"
        '';
      };
      rp2040 = self.packages.${system}.rp2040;
      "rp2040-power-test" = self.packages.${system}."rp2040-power-test";
      moonlander = self.packages.${system}.moonlander;
      "moonlander-power-test" = self.packages.${system}."moonlander-power-test";
      "power-capture" = self.packages.${system}."power-capture";
      "power-capture-tests" = pkgs.stdenv.mkDerivation {
        pname = "landerviz-power-capture-tests";
        version = "0.1.0";
        src = self;
        dontConfigure = true;
        buildPhase = ''
          $CXX -std=c++17 -Wall -Wextra -Werror \
            tests/fnb58_capture_test.cpp -o fnb58_capture_test
          ./fnb58_capture_test
        '';
        installPhase = ''
          touch "$out"
        '';
      };
      "power-test-core" = pkgs.stdenv.mkDerivation {
        pname = "landerviz-power-test-core";
        version = "0.1.0";
        src = self;
        dontConfigure = true;
        buildPhase = ''
          $CXX -std=c++17 -Wall -Wextra -Werror -Iinclude \
            tests/power_test_test.cpp src/power_test.cpp \
            -o power_test_test
          ./power_test_test
        '';
        installPhase = ''
          touch "$out"
        '';
      };
      "pcf8574-lcd-core" = pkgs.stdenv.mkDerivation {
        pname = "landerviz-pcf8574-lcd-core";
        version = "0.1.0";
        src = self;
        dontConfigure = true;
        buildPhase = ''
          $CXX -std=c++17 -Wall -Wextra -Werror -Iinclude \
            tests/pcf8574_lcd_test.cpp -o pcf8574_lcd_test
          ./pcf8574_lcd_test
        '';
        installPhase = ''
          touch "$out"
        '';
      };
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
          pkgs.hidapi
        ];
        PICO_SDK_PATH = "${pkgs.pico-sdk}/lib/pico-sdk";
        PICO_PIO_USB_PATH = pico-pio-usb;
        PICO_TINYUSB_PATH = tinyusb;
        CMSIS_DSP_PATH = cmsis-dsp;
      };
    });
  };
}
