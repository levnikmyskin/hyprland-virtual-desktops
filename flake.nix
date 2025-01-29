{
  description = "A plugin for the Hyprland compositor, implementing virtual-desktop functionality.";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  # Hyprland at tag v0.47.0
  inputs.hyprland.url = "github:hyprwm/Hyprland/04ac46c54357278fc68f0a95d26347ea0db99496";

  outputs = {
    self,
    nixpkgs,
    hyprland,
  }: let
    # Helper function to create packages for each system
    withPkgsFor = fn: nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system: fn system nixpkgs.legacyPackages.${system});
    virtualDesktops = withPkgsFor (system: pkgs:
      pkgs.gcc14Stdenv.mkDerivation {
        pname = "virtual-desktops";
        version = "2.2.7";
        src = ./.;

        inherit (hyprland.packages.${system}.hyprland) nativeBuildInputs;

        buildInputs = [hyprland.packages.${system}.hyprland] ++ hyprland.packages.${system}.hyprland.buildInputs;

        # Skip meson phases
        configurePhase = "true";
        mesonConfigurePhase = "true";
        mesonBuildPhase = "true";
        mesonInstallPhase = "true";

        buildPhase = ''
          runHook preBuild
          make all
          runHook postBuild
        '';

        installPhase = ''
          runHook preInstall
          mkdir -p $out/lib
          cp virtual-desktops.so $out/lib/libvirtual-desktops.so
          runHook postInstall
        '';

        meta = with pkgs.lib; {
          homepage = "https://github.com/levnikmyskin/hyprland-virtual-desktops";
          description = "A plugin for the Hyprland compositor, implementing virtual-desktop functionality.";
          license = licenses.bsd3;
          platforms = platforms.linux;
        };
      });
  in {
    packages = withPkgsFor (system: pkgs: rec {
      virtual-desktops = virtualDesktops.${system};
      default = virtual-desktops;
    });

    devShells = withPkgsFor (system: pkgs: {
      default = pkgs.mkShell.override {stdenv = pkgs.gcc14Stdenv;} {
        name = "virtual-desktops";
        inputsFrom = [hyprland.packages.${system}.hyprland];
        nativeBuildInputs = [pkgs.cmake];
        buildInputs = with pkgs; [
          clang
          clang-tools
          nodejs # for copilot
          include-what-you-use
          cpplint

          hyprland.packages.${system}.hyprland
        ];
        shellHook = ''
          exec ${pkgs.zsh}/bin/zsh
        '';
      };
    });
  };
}
